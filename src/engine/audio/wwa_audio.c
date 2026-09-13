/* World Without Answers — wwa_audio.c
   Software audio mixer + waveform generation. */
#include <wwa_audio.h>
#include <stdmem.h>
#include <stdmath.h>

void wwa_audio_init(wwa_audio_state_t* a) {
    wwa_memset(a, 0, sizeof(wwa_audio_state_t));
    a->master_volume = 0.5f;
    a->sample_rate = WWA_AUDIO_SAMPLE_RATE;
}

f32 wwa_audio_wave_generate(wwa_wave_type_t type, f32 phase) {
    f32 t = phase - (f32)(i32)phase;  /* fmodf */
    switch(type) {
        case WWA_WAVE_SINE:     return wwa_sinf(t * 6.2831853f);
        case WWA_WAVE_SQUARE:   return t < 0.5f ? 1.0f : -1.0f;
        case WWA_WAVE_SAW:      return 2.0f * t - 1.0f;
        case WWA_WAVE_TRIANGLE: return 4.0f * (t < 0.5f ? t : 1.0f - t) - 1.0f;
        case WWA_WAVE_NOISE:    return 0; /* use separate RNG */
    }
    return 0.0f;
}

void wwa_audio_adsr_update(wwa_audio_source_t* src, f32 dt) {
    src->elapsed += dt;
    if(src->elapsed >= src->duration) { src->env_stage = 4; src->env_level = 0; return; }
    switch(src->env_stage) {
        case 0: /* Attack */
            src->env_level += dt / src->attack;
            if(src->env_level >= 1.0f) { src->env_level = 1.0f; src->env_stage = 1; }
            break;
        case 1: /* Decay */
            src->env_level -= dt / src->decay;
            if(src->env_level <= src->sustain) { src->env_level = src->sustain; src->env_stage = 2; }
            break;
        case 2: /* Sustain */
            if(src->elapsed >= src->duration - src->release) src->env_stage = 3;
            break;
        case 3: /* Release */
            src->env_level -= dt / src->release;
            if(src->env_level <= 0.0f) { src->env_level = 0.0f; src->env_stage = 4; }
            break;
    }
}

u32 wwa_audio_play(wwa_audio_state_t* a, wwa_wave_type_t type, f32 freq, f32 amp, f32 dur) {
    for(u32 i=0;i<WWA_AUDIO_MAX_SOURCES;i++) {
        if(!a->sources[i].active) {
            wwa_audio_source_t* s = &a->sources[i];
            s->type = type; s->frequency = freq; s->phase = 0;
            s->amplitude = amp; s->duration = dur; s->elapsed = 0;
            s->attack = 0.01f; s->decay = 0.1f; s->sustain = 0.7f; s->release = 0.1f;
            s->env_level = 0; s->env_stage = 0; s->active = 1;
            return i;
        }
    }
    return 0xFFFFFFFF;
}

void wwa_audio_stop_all(wwa_audio_state_t* a) {
    for(u32 i=0;i<WWA_AUDIO_MAX_SOURCES;i++) a->sources[i].active=0;
}

void wwa_audio_mix(wwa_audio_state_t* a) {
    wwa_memset(a->mix_buf.left, 0, sizeof(f32)*WWA_AUDIO_BUFFER_SIZE);
    wwa_memset(a->mix_buf.right, 0, sizeof(f32)*WWA_AUDIO_BUFFER_SIZE);
    f32 dt = 1.0f / (f32)a->sample_rate;

    for(u32 i=0;i<WWA_AUDIO_MAX_SOURCES;i++) {
        wwa_audio_source_t* s = &a->sources[i];
        if(!s->active) continue;
        wwa_audio_adsr_update(s, dt);
        if(s->env_stage == 4) { s->active = 0; continue; }

        f32 inc = s->frequency / (f32)a->sample_rate;
        for(i32 f=0;f<WWA_AUDIO_BUFFER_SIZE;f++) {
            f32 wave = wwa_audio_wave_generate(s->type, s->phase);
            f32 sample = wave * s->amplitude * s->env_level * a->master_volume;
            a->mix_buf.left[f] += sample;
            a->mix_buf.right[f] += sample;
            s->phase += inc;
        }
    }
    /* Soft clip */
    for(i32 i=0;i<WWA_AUDIO_BUFFER_SIZE;i++) {
        f32 l = a->mix_buf.left[i];
        f32 r = a->mix_buf.right[i];
        if(l>1.0f)l=1.0f; if(l<-1.0f)l=-1.0f;
        if(r>1.0f)r=1.0f; if(r<-1.0f)r=-1.0f;
        a->mix_buf.left[i]=l; a->mix_buf.right[i]=r;
    }
}

void wwa_audio_generate(wwa_audio_state_t* a, wwa_audio_buffer_t* out) {
    wwa_audio_mix(a);
    wwa_memmove(out, &a->mix_buf, sizeof(wwa_audio_buffer_t));
}
