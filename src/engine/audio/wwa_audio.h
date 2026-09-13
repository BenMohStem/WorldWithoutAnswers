/* World Without Answers — wwa_audio.h
   Software audio mixer + waveform generation.
   No libc, no 3rd-party libs. Pure from-scratch audio.
   SIMD-friendly: planar buffer layout (not interleaved). */
#ifndef WWA_AUDIO_H
#define WWA_AUDIO_H
#include <stdtype.h>

#define WWA_AUDIO_SAMPLE_RATE 44100
#define WWA_AUDIO_BUFFER_SIZE 1024
#define WWA_AUDIO_CHANNELS   2
#define WWA_AUDIO_MAX_SOURCES 32

typedef enum {
    WWA_WAVE_SINE, WWA_WAVE_SQUARE, WWA_WAVE_SAW, WWA_WAVE_TRIANGLE, WWA_WAVE_NOISE
} wwa_wave_type_t;

typedef struct {
    wwa_wave_type_t type;
    f32 frequency;
    f32 phase;
    f32 amplitude;
    f32 attack, decay, sustain, release;  /* ADSR */
    f32 env_level;
    i32 env_stage;  /* 0=attack,1=decay,2=sustain,3=release,4=done */
    f32 duration;
    f32 elapsed;
    i32 active;
} wwa_audio_source_t;

typedef struct {
    f32 left[WWA_AUDIO_BUFFER_SIZE];
    f32 right[WWA_AUDIO_BUFFER_SIZE];
    i32 frames;
} wwa_audio_buffer_t;

typedef struct {
    wwa_audio_source_t sources[WWA_AUDIO_MAX_SOURCES];
    wwa_audio_buffer_t mix_buf;
    f32 master_volume;
    u32 sample_rate;
} wwa_audio_state_t;

void wwa_audio_init(wwa_audio_state_t* a);
void wwa_audio_mix(wwa_audio_state_t* a);
void wwa_audio_generate(wwa_audio_state_t* a, wwa_audio_buffer_t* out);
u32  wwa_audio_play(wwa_audio_state_t* a, wwa_wave_type_t type, f32 freq, f32 amp, f32 dur);
void wwa_audio_stop_all(wwa_audio_state_t* a);
f32  wwa_audio_wave_generate(wwa_wave_type_t type, f32 phase);
void wwa_audio_adsr_update(wwa_audio_source_t* src, f32 dt);

#endif /* WWA_AUDIO_H */
