#ifndef LISAEM_IM_EMULATOR_H
#define LISAEM_IM_EMULATOR_H
#include <stdint.h>
int im_initialize(const char *rom, const char *disk, const char *pram, int io_rom);
int im_execute(int cycles);
void im_render(uint32_t *pixels);
void im_close(void);
void im_audio_advance(void);
void im_audio_close(void);
extern int im_powered_off;
#endif
