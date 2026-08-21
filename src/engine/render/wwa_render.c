/* World Without Answers — wwa_render.c
   Software renderer: Pineda edge function + z-buffer + flat shading. */
#include <wwa_render.h>
#include <stdmem.h>
#include <stdmath.h>
#include <stdos.h>

static i32 g_render_avx2 = -1; /* -1 = unprobed */

static i32 wwa_render_use_avx2(void) {
    if (g_render_avx2 < 0) g_render_avx2 = wwa_os_cpu_avx2() ? 1 : 0;
    return g_render_avx2;
}

void wwa_mat4_identity(f32 m[16]) {
    wwa_memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void wwa_mat4_multiply(f32 out[16], const f32 a[16], const f32 b[16]) {
    f32 t[16];
    for (i32 c = 0; c < 4; c++)
        for (i32 r = 0; r < 4; r++)
            t[c*4+r] = a[r]*b[c*4]+a[4+r]*b[c*4+1]+a[8+r]*b[c*4+2]+a[12+r]*b[c*4+3];
    wwa_memmove(out, t, 64);
}

void wwa_mat4_perspective(f32 m[16], f32 fov, f32 asp, f32 n, f32 f) {
    f32 t2h = wwa_tanf(fov * 0.5f * 3.14159265f / 180.0f);
    wwa_memset(m, 0, 64);
    m[0] = 1.0f/(asp*t2h); m[5] = 1.0f/t2h;
    m[10] = -(f+n)/(f-n); m[11] = -1.0f; m[14] = -2.0f*f*n/(f-n);
}

void wwa_mat4_lookat(f32 m[16], f32 ex,f32 ey,f32 ez, f32 tx,f32 ty,f32 tz, f32 ux,f32 uy,f32 uz) {
    f32 fx=tx-ex,fy=ty-ey,fz=tz-ez,fl=wwa_sqrtf(fx*fx+fy*fy+fz*fz);
    if(fl>1e-6f){fx/=fl;fy/=fl;fz/=fl;}
    f32 rx=fy*uz-fz*uy,ry=fz*ux-fx*uz,rz=fx*uy-fy*ux,rl=wwa_sqrtf(rx*rx+ry*ry+rz*rz);
    if(rl>1e-6f){rx/=rl;ry/=rl;rz/=rl;}
    f32 ux2=ry*fz-rz*fy,uy2=rz*fx-rx*fz,uz2=rx*fy-ry*fx;
    wwa_mat4_identity(m);
    m[0]=rx;m[4]=ry;m[8]=rz;m[1]=ux2;m[5]=uy2;m[9]=uz2;
    m[2]=-fx;m[6]=-fy;m[10]=-fz;
    m[12]=-(rx*ex+ry*ey+rz*ez); m[13]=-(ux2*ex+uy2*ey+uz2*ez); m[14]=fx*ex+fy*ey+fz*ez;
}

void wwa_mat4_ortho(f32 m[16], f32 l,f32 r,f32 b,f32 t,f32 n,f32 f) {
    wwa_memset(m,0,64);
    m[0]=2.0f/(r-l);m[5]=2.0f/(t-b);m[10]=-2.0f/(f-n);m[15]=1.0f;
    m[12]=-(r+l)/(r-l);m[13]=-(t+b)/(t-b);m[14]=-(f+n)/(f-n);
}

void wwa_vec4_transform(f32 out[4], const f32 m[16], const f32 v[4]) {
    out[0]=m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12]*v[3];
    out[1]=m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13]*v[3];
    out[2]=m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]*v[3];
    out[3]=m[3]*v[0]+m[7]*v[1]+m[11]*v[2]+m[15]*v[3];
}

void wwa_render_init(wwa_render_state_t* r, i32 w, i32 h) {
    wwa_memset(r, 0, sizeof(wwa_render_state_t));
    r->fb.width=w; r->fb.height=h; r->fb.stride=w;
    r->tiles_x=(w+WWA_TILE_SIZE-1)/WWA_TILE_SIZE;
    r->tiles_y=(h+WWA_TILE_SIZE-1)/WWA_TILE_SIZE;
}

void wwa_render_clear(wwa_render_state_t* r, u32 color, f32 depth) {
    u32 n=(u32)(r->fb.width*r->fb.height);
    for(u32 i=0;i<n;i++){r->fb.color[i]=color;r->fb.depth[i]=depth;}
    r->pixels_drawn=0; r->triangles_clipped=0;
}

void wwa_render_set_perspective(wwa_render_state_t* r, f32 fov, f32 asp, f32 n, f32 f) {
    wwa_mat4_perspective(r->proj,fov,asp,n,f);
}
void wwa_render_set_ortho(wwa_render_state_t* r, f32 l,f32 ri,f32 b,f32 t,f32 n,f32 f) {
    wwa_mat4_ortho(r->proj,l,ri,b,t,n,f);
}
void wwa_render_set_lookat(wwa_render_state_t* r, f32 ex,f32 ey,f32 ez, f32 tx,f32 ty,f32 tz, f32 ux,f32 uy,f32 uz) {
    wwa_mat4_lookat(r->view,ex,ey,ez,tx,ty,tz,ux,uy,uz);
    r->cam_x=ex;r->cam_y=ey;r->cam_z=ez;
    wwa_mat4_multiply(r->mvp,r->proj,r->view);
}

u32 wwa_render_add_vertex(wwa_render_state_t* r, f32 x,f32 y,f32 z, f32 nx,f32 ny,f32 nz, u32 c) {
    if(r->vert_count>=WWA_RENDER_MAX_VERTICES) return 0xFFFFFFFF;
    u32 i=r->vert_count++;
    r->verts[i].x=x;r->verts[i].y=y;r->verts[i].z=z;
    r->verts[i].nx=nx;r->verts[i].ny=ny;r->verts[i].nz=nz;
    r->verts[i].color=c; return i;
}
u32 wwa_render_add_triangle(wwa_render_state_t* r, u32 v0,u32 v1,u32 v2) {
    if(r->tri_count>=WWA_RENDER_MAX_TRIANGLES) return 0xFFFFFFFF;
    u32 i=r->tri_count++;
    r->tris[i].v0=v0;r->tris[i].v1=v1;r->tris[i].v2=v2;
    r->tris[i].color=r->verts[v0].color; return i;
}

void wwa_render_transform(wwa_render_state_t* r) {
    f32 hw=(f32)r->fb.width*0.5f, hh=(f32)r->fb.height*0.5f;
    r->screentri_count=0;
    for(u32 t=0; t<r->tri_count; t++) {
        wwa_render_triangle_t* tri=&r->tris[t];
        wwa_render_screentri_t* st=&r->screentris[r->screentri_count];
        u32 vis=0;
        for(i32 v=0;v<3;v++){
            u32 vi=(v==0)?tri->v0:(v==1)?tri->v1:tri->v2;
            f32 p[4]={r->verts[vi].x,r->verts[vi].y,r->verts[vi].z,1.0f},cl[4];
            wwa_vec4_transform(cl,r->mvp,p);
            if(cl[3]<0.001f) continue;
            f32 iw=1.0f/cl[3],sx=(cl[0]*iw+1.0f)*hw,sy=(1.0f-cl[1]*iw)*hh,sz=cl[2]*iw;
            if(v==0){st->x0=sx;st->y0=sy;st->z0=sz;st->inv_w0=iw;}
            if(v==1){st->x1=sx;st->y1=sy;st->z1=sz;st->inv_w1=iw;}
            if(v==2){st->x2=sx;st->y2=sy;st->z2=sz;st->inv_w2=iw;}
            vis++;
        }
        if(vis<3) continue;
        st->color=tri->color;
        f32 dx1=st->x1-st->x0,dy1=st->y1-st->y0,dx2=st->x2-st->x0,dy2=st->y2-st->y0;
        if(dx1*dy2-dy1*dx2<=0.0f) continue;
        r->screentri_count++;
    }
}

void wwa_render_triangle_fill(wwa_render_state_t* r,
    f32 x0,f32 y0,f32 x1,f32 y1,f32 x2,f32 y2, f32 z0,f32 z1,f32 z2, u32 color)
{
    i32 minx=(i32)x0,miny=(i32)y0,maxx=(i32)x0,maxy=(i32)y0;
    if((i32)x1<minx)minx=(i32)x1; if((i32)x1>maxx)maxx=(i32)x1;
    if((i32)y1<miny)miny=(i32)y1; if((i32)y1>maxy)maxy=(i32)y1;
    if((i32)x2<minx)minx=(i32)x2; if((i32)x2>maxx)maxx=(i32)x2;
    if((i32)y2<miny)miny=(i32)y2; if((i32)y2>maxy)maxy=(i32)y2;
    if(minx<0)minx=0; if(miny<0)miny=0;
    if(maxx>=r->fb.width)maxx=r->fb.width-1;
    if(maxy>=r->fb.height)maxy=r->fb.height-1;

    f32 dx01=x0-x1,dy01=y0-y1,dx12=x1-x2,dy12=y1-y2,dx20=x2-x0,dy20=y2-y0;
    f32 area=dx01*dy12-dy01*dx12;
    if(area<=0.0f)return;
    f32 inv=1.0f/area;
    u32 cr=(color>>16)&0xFF,cg=(color>>8)&0xFF,cb=color&0xFF;

    for(i32 py=miny;py<=maxy;py++){
        for(i32 px=minx;px<=maxx;px++){
            f32 fx=(f32)px+0.5f,fy=(f32)py+0.5f;
            f32 w0=(fx-x1)*dy01-(fy-y1)*dx01;
            f32 w1=(fx-x2)*dy12-(fy-y2)*dx12;
            f32 w2=(fx-x0)*dy20-(fy-y0)*dx20;
            if(w0>=0&&w1>=0&&w2>=0){
                w0*=inv; w1*=inv; w2*=inv;
                f32 z=z0*w0+z1*w1+z2*w2;
                u32 idx=(u32)(py*r->fb.stride+px);
                if(z<r->fb.depth[idx]){
                    r->fb.depth[idx]=z;
                    /* Simple directional light from top-right */
                    f32 light=0.7f+0.3f*w0;
                    u32 lr=(u32)(cr*light); if(lr>255)lr=255;
                    u32 lg=(u32)(cg*light); if(lg>255)lg=255;
                    u32 lb=(u32)(cb*light); if(lb>255)lb=255;
                    r->fb.color[idx]=0xFF000000|(lr<<16)|(lg<<8)|lb;
                    r->pixels_drawn++;
                }
            }
        }
    }
}

void wwa_render_line(wwa_render_state_t* r, i32 x0,i32 y0,i32 x1,i32 y1, u32 color) {
    i32 dx=x1-x0,dy=y1-y0;
    i32 sx=(dx>0)?1:-1,sy=(dy>0)?1:-1;
    dx=(dx<0)?-dx:dx; dy=(dy<0)?-dy:dy;
    i32 err=dx-dy;
    for(;;){
        if(x0>=0&&x0<r->fb.width&&y0>=0&&y0<r->fb.height){
            r->fb.color[y0*r->fb.stride+x0]=color;
        }
        if(x0==x1&&y0==y1)break;
        i32 e2=2*err;
        if(e2>-dy){err-=dy;x0+=sx;}
        if(e2<dx){err+=dx;y0+=sy;}
    }
}

void wwa_render_clip_and_rasterize(wwa_render_state_t* r) {
    i32 avx = wwa_render_use_avx2();
    for(u32 i=0;i<r->screentri_count;i++){
        wwa_render_screentri_t* s=&r->screentris[i];
        if (avx) {
            wwa_render_triangle_fill_avx2(r,s->x0,s->y0,s->x1,s->y1,s->x2,s->y2,
                                          s->z0,s->z1,s->z2,s->color);
        } else {
            wwa_render_triangle_fill(r,s->x0,s->y0,s->x1,s->y1,s->x2,s->y2,
                                     s->z0,s->z1,s->z2,s->color);
        }
    }
}
