// pico_audio.c
// PICO-8 audio implementation (no reverb)

#include "pico_audio.h"
#include <string.h>

// PICO-8 note frequencies (C-0 to C-5, 64 notes) as fix32
static const fix32_t NOTE_FREQ[] = {
    1079,  1144,  1212,  1285,  1361,  1442,  1528,  1619,  1716,  1818,  1926,  2041,
    2162,  2291,  2427,  2572,  2725,  2887,  3059,  3241,  3434,  3639,  3856,  4086,
    4330,  4588,  4861,  5151,  5458,  5783,  6127,  6492,  6878,  7288,  7722,  8181,
    8670,  9185,  9732, 10311, 10926, 11576, 12265, 12995, 13768, 14588, 15457, 16376,
    17352, 18382, 19477, 20637, 21866, 23166, 24545, 26006, 27553, 29192, 30930, 32770,
    34716, 36781, 38968, 41285,
};

int16_t pico_wave_sample(pico_waveform_t wave, uint32_t phase, uint16_t* lfsr) {
    uint32_t pos = phase >> 16;
    int32_t sample = 0;

    switch (wave) {
        case PICO_WAVE_TRIANGLE:
            if (pos < 0x8000) {
                sample = (pos * 2) - 0x8000;
            } else {
                sample = 0x8000 - ((pos - 0x8000) * 2);
            }
            break;

        case PICO_WAVE_TILTED:
            if (pos < 0xE000) {
                sample = (pos * 0x10000 / 0xE000) - 0x8000;
            } else {
                sample = 0x8000 - ((pos - 0xE000) * 0x10000 / 0x2000);
            }
            break;

        case PICO_WAVE_SAWTOOTH:
            sample = pos - 0x8000;
            break;

        case PICO_WAVE_SQUARE:
            sample = (pos < 0x8000) ? -0x7FFF : 0x7FFF;
            break;

        case PICO_WAVE_PULSE:
            sample = (pos < 0x4000) ? -0x7FFF : 0x7FFF;
            break;

        case PICO_WAVE_ORGAN:
            {
                int32_t t1, t2;
                if (pos < 0x8000) {
                    t1 = (pos * 2) - 0x8000;
                } else {
                    t1 = 0x8000 - ((pos - 0x8000) * 2);
                }
                uint32_t pos2 = (pos * 2) & 0xFFFF;
                if (pos2 < 0x8000) {
                    t2 = (pos2 * 2) - 0x8000;
                } else {
                    t2 = 0x8000 - ((pos2 - 0x8000) * 2);
                }
                sample = (t1 + t2 / 2) * 2 / 3;
            }
            break;

        case PICO_WAVE_NOISE:
            {
                uint16_t bit = ((*lfsr >> 0) ^ (*lfsr >> 2) ^
                               (*lfsr >> 3) ^ (*lfsr >> 5)) & 1;
                *lfsr = (*lfsr >> 1) | (bit << 15);
                sample = ((*lfsr & 0xFFFF) - 0x8000);
            }
            break;

        case PICO_WAVE_PHASER:
            {
                uint32_t pos2 = (pos + 0x4000) & 0xFFFF;
                sample = ((pos - 0x8000) + (pos2 - 0x8000)) / 2;
            }
            break;
    }

    return (int16_t)(sample >> 1);
}

void pico_audio_init(pico_audio_t* audio, pico_ram_t* ram) {
    memset(audio, 0, sizeof(pico_audio_t));
    audio->ram = ram;
    audio->master_volume = 255;

    for (int i = 0; i < PICO_CHANNELS; i++) {
        audio->channels[i].sfx_index = -1;
        audio->channels[i].noise_lfsr = 0xACE1;
    }

    audio->music.pattern_index = -1;
}

void pico_audio_reset(pico_audio_t* audio) {
    for (int i = 0; i < PICO_CHANNELS; i++) {
        audio->channels[i].sfx_index = -1;
    }
    audio->music.pattern_index = -1;
    audio->buffer_pos = 0;
}

void pico_audio_shutdown(pico_audio_t* audio) {
    pico_audio_reset(audio);
}

fix32_t pico_note_to_freq(uint8_t note) {
    if (note >= 64) note = 63;
    return NOTE_FREQ[note];
}

void pico_sfx(pico_audio_t* audio, int8_t n, int8_t channel,
              uint8_t offset, uint8_t length) {
    if (n < 0) {
        if (channel >= 0 && channel < PICO_CHANNELS) {
            audio->channels[channel].sfx_index = -1;
        }
        return;
    }

    if (n >= PICO_SFX_COUNT) return;

    if (channel < 0) {
        for (int i = 0; i < PICO_CHANNELS; i++) {
            if (audio->channels[i].sfx_index < 0) {
                channel = i;
                break;
            }
        }
        if (channel < 0) channel = 0;
    }

    if (channel >= PICO_CHANNELS) return;

    pico_channel_t* ch = &audio->channels[channel];
    pico_sfx_t* sfx = &audio->ram->sfx[n];

    ch->sfx_index = n;
    ch->note_index = offset;
    ch->sample_counter = 0;
    ch->phase = 0;

    uint32_t speed = sfx->speed;
    if (speed == 0) speed = 1;
    ch->samples_per_tick = (PICO_SAMPLE_RATE * speed) / 120;

    if (offset < PICO_NOTES_PER_SFX) {
        pico_note_t* note = &sfx->notes[offset];
        uint8_t pitch     = pico_note_key(note);
        ch->waveform      = pico_note_waveform(note);
        ch->volume        = pico_note_volume(note);
        ch->effect        = pico_note_effect(note);
        ch->base_frequency = pico_note_to_freq(pitch);
        ch->frequency      = ch->base_frequency;
        ch->phase_inc      = ((uint64_t)ch->frequency * 65536) / PICO_SAMPLE_RATE;
    }

    (void)length;  // length limiting not yet implemented
}

void pico_music(pico_audio_t* audio, int8_t n, uint16_t fade_ms,
                uint8_t channel_mask) {
    if (n < 0) {
        audio->music.pattern_index = -1;
        return;
    }

    if (n >= PICO_MUSIC_COUNT) return;

    audio->music.pattern_index = n;
    audio->music.tick = 0;
    audio->music.loop_enabled = true;

    (void)fade_ms;
    (void)channel_mask;
}

static void update_channel(pico_audio_t* audio, pico_channel_t* ch) {
    if (ch->sfx_index < 0) return;

    pico_sfx_t* sfx = &audio->ram->sfx[ch->sfx_index];

    // Advance tick counter
    ch->sample_counter++;
    if (ch->sample_counter >= ch->samples_per_tick) {
        ch->sample_counter = 0;
        ch->note_index++;

        if (ch->note_index >= PICO_NOTES_PER_SFX) {
            if (sfx->loop_start < sfx->loop_end) {
                ch->note_index = sfx->loop_start;
            } else {
                ch->sfx_index = -1;
                return;
            }
        }

        ch->phase = 0;  // Reset phase at note boundary
    }

    // Read current note data (done every tick so phase_inc is always current)
    pico_note_t* note = &sfx->notes[ch->note_index];
    uint8_t pitch     = pico_note_key(note);
    ch->waveform      = pico_note_waveform(note);
    ch->volume        = pico_note_volume(note);
    ch->effect        = pico_note_effect(note);

    ch->base_frequency = pico_note_to_freq(pitch);
    ch->frequency      = ch->base_frequency;
    // phase_inc is now always set before the first sample of a new note
    ch->phase_inc      = ((uint64_t)ch->frequency * 65536) / PICO_SAMPLE_RATE;
}

void pico_audio_fill(pico_audio_t* audio, int16_t* out, uint16_t samples) {
    for (uint16_t i = 0; i < samples; i++) {
        int32_t mix = 0;

        for (int c = 0; c < PICO_CHANNELS; c++) {
            pico_channel_t* ch = &audio->channels[c];

            if (ch->sfx_index < 0) continue;

            // Advance tick/note state and refresh note parameters
            update_channel(audio, ch);

            if (ch->sfx_index < 0) continue;

            int16_t sample;
            if (ch->waveform == PICO_WAVE_NOISE) {
                // higher notes = faster noise texture
                uint32_t prev_phase = ch->phase;
                ch->phase += ch->phase_inc;
                if ((ch->phase ^ prev_phase) & 0x10000) {
                    // Phase wrapped — clock LFSR
                    uint16_t bit = ((ch->noise_lfsr >> 0) ^ (ch->noise_lfsr >> 2) ^
                                    (ch->noise_lfsr >> 3) ^ (ch->noise_lfsr >> 5)) & 1;
                    ch->noise_lfsr = (ch->noise_lfsr >> 1) | (bit << 15);
                }
                sample = (int16_t)((ch->noise_lfsr - 0x8000) >> 1);
            } else {
                sample = pico_wave_sample(ch->waveform, ch->phase, &ch->noise_lfsr);
                ch->phase += ch->phase_inc;
            }

            sample = (sample * ch->volume * 32) / 256;
            mix += sample;
        }

        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;

        mix = (mix * audio->master_volume) / 256;

        out[i] = (int16_t)mix;
    }
}

void pico_audio_update(pico_audio_t* audio) {
    if (audio->music.pattern_index < 0) return;
    // TODO: Implement proper music sequencing
}
