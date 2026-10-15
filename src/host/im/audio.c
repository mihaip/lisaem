/* Keep the native frontend's square-wave pitch conversion, but produce PCM
 * against guest clocks so short beeps survive changes within a CPU slice. */
#include <vars.h>
#include "emulator.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static XTIMER last_clock;
static XTIMER tone_started;
static uint64_t remainder;
static uint64_t total_samples, stop_sample;
static unsigned period, phase;
static int playing;
static int8_t samples[882];
static unsigned sample_count;
#ifndef __EMSCRIPTEN__
static FILE *capture;
static int capture_initialized;
#endif

static void flush_samples(void)
{
    if (!sample_count) return;
#ifdef __EMSCRIPTEN__
    EM_ASM({ workerApi.enqueueAudio($0, $1); }, samples, sample_count);
#else
    /* Optional signed 8-bit, mono, 44100 Hz reference capture. */
    if (!capture_initialized) {
        const char *path = getenv("LISAEM_AUDIO_DUMP");
        if (path) capture = fopen(path, "wb");
        capture_initialized = 1;
    }
    if (capture) fwrite(samples, 1, sample_count, capture);
#endif
    sample_count = 0;
}

void im_audio_advance(void)
{
    XTIMER elapsed = cpu68k_clocks - last_clock;
    last_clock = cpu68k_clocks;
    if (elapsed < 0) { remainder = 0; return; }
    uint64_t scaled = remainder + (uint64_t)elapsed * 44100;
    uint64_t count = scaled / 5000000;
    remainder = scaled % 5000000;
    while (count--) {
        if (stop_sample && total_samples >= stop_sample) playing = 0;
        int amplitude = (volume & 7) * 8;
        samples[sample_count++] = playing ?
            (phase < period / 2 ? amplitude : -amplitude) : 0;
        if (playing && ++phase >= period) phase = 0;
        total_samples++;
        if (sample_count == sizeof(samples)) flush_samples();
    }
}

void sound_play(uint16 t2)
{
    im_audio_advance();
    if (!t2) { playing = 0; return; }
    unsigned cycles = t2 * 4;
    if (!(floppy_iorom & 0x20)) cycles = cycles * 8 / 10;
    cycles = (cycles >> 1) + (cycles >> 2);
    if (cycles < 2) cycles = 2;
    if (playing && period == cycles && cpu68k_clocks - tone_started < 50000)
        return;
    if (!playing || period != cycles) phase = 0;
    period = cycles;
    playing = 1;
    tone_started = cpu68k_clocks;
    stop_sample = 0;
}

void sound_off(void)
{
    im_audio_advance();
    /* Match wx's minimum 10 ms gate, but defer an early stop instead of
     * losing it and leaving the sound looping indefinitely. */
    XTIMER end = tone_started + 50000;
    if (end < cpu68k_clocks) end = cpu68k_clocks;
    stop_sample = (uint64_t)end * 44100 / 5000000;
}

void im_audio_close(void)
{
    im_audio_advance();
    flush_samples();
#ifndef __EMSCRIPTEN__
    if (capture) { fclose(capture); capture = NULL; }
#endif
}
