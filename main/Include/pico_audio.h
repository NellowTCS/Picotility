// pico_audio.h
// PICO-8 audio subsystem (simplified, no reverb)

#ifndef PICO_AUDIO_H
#define PICO_AUDIO_H

#include "pico_config.h"
#include "pico_ram.h"

// Audio Constants
#define PICO_SAMPLE_RATE    22050
#define PICO_NOTES_PER_SFX  32

// Waveform types
typedef enum {
    PICO_WAVE_TRIANGLE  = 0,
    PICO_WAVE_TILTED    = 1,
    PICO_WAVE_SAWTOOTH  = 2,
    PICO_WAVE_SQUARE    = 3,
    PICO_WAVE_PULSE     = 4,
    PICO_WAVE_ORGAN     = 5,
    PICO_WAVE_NOISE     = 6,
    PICO_WAVE_PHASER    = 7,
} pico_waveform_t;

// Effect types
typedef enum {
    PICO_FX_NONE        = 0,
    PICO_FX_SLIDE       = 1,
    PICO_FX_VIBRATO     = 2,
    PICO_FX_DROP        = 3,
    PICO_FX_FADE_IN     = 4,
    PICO_FX_FADE_OUT    = 5,
    PICO_FX_ARPFAST     = 6,
    PICO_FX_ARPSLOW     = 7,
} pico_effect_t;

// Channel State (~50 bytes per channel)
typedef struct {
    int8_t sfx_index;           // -1 if not playing
    uint8_t note_index;
    uint32_t sample_counter;
    uint32_t samples_per_tick;
    uint32_t phase;
    uint32_t phase_inc;
    uint8_t waveform;
    uint8_t volume;
    uint8_t effect;
    fix32_t frequency;
    fix32_t base_frequency;
    fix32_t effect_value;
    uint16_t noise_lfsr;
} pico_channel_t;

// Music State
typedef struct {
    int8_t pattern_index;       // -1 if stopped
    uint8_t tick;
    bool loop_enabled;
} pico_music_state_t;

// Audio Context
typedef struct {
    pico_ram_t* ram;
    pico_channel_t channels[PICO_CHANNELS];
    pico_music_state_t music;
    int16_t buffer[PICO_AUDIO_BUFFER_SIZE];
    uint16_t buffer_pos;
    uint8_t master_volume;
} pico_audio_t;

// Initialization
void pico_audio_init(pico_audio_t* audio, pico_ram_t* ram);
void pico_audio_reset(pico_audio_t* audio);
void pico_audio_shutdown(pico_audio_t* audio);

// SFX Playback
// n: SFX number (0-63), -1 to stop channel, -2 to stop looping
// channel: 0-3 or -1 for auto-select, -2 to stop all on this SFX
void pico_sfx(pico_audio_t* audio, int8_t n, int8_t channel, uint8_t offset, uint8_t length);

// Music Playback
// n: Pattern number (0-63), -1 to stop
void pico_music(pico_audio_t* audio, int8_t n, uint16_t fade_ms, uint8_t channel_mask);

// Audio Processing
void pico_audio_fill(pico_audio_t* audio, int16_t* out, uint16_t samples);
void pico_audio_update(pico_audio_t* audio);

// Utility
fix32_t pico_note_to_freq(uint8_t note);
int16_t pico_wave_sample(pico_waveform_t wave, uint32_t phase, uint16_t* lfsr);

#endif // PICO_AUDIO_H
