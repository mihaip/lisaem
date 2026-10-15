/* Small native harness for comparing this frontend with the wx reference. */
#include <vars.h>
#include "emulator.h"

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "Usage: lisaem-im ROM DISK SECONDS OUTPUT.pgm\n");
        return 1;
    }
    if (im_initialize(argv[1], argv[2], "pram.bin", 0xa8)) return 1;
    XTIMER end = (XTIMER)atoi(argv[3]) * 5000000;
    int booted = 0;
    while (!im_powered_off && cpu68k_clocks < end) {
        im_execute(25000);
        if (!booted && cpu68k_clocks > 20000000) {
            apple_3();
            booted = 1;
        }
    }
    uint32_t *pixels = malloc(720 * 364 * 4);
    im_render(pixels);
    FILE *f = fopen(argv[4], "wb");
    if (!f) return 1;
    fprintf(f, "P5\n720 364\n255\n");
    for (int i = 0; i < 720 * 364; i++) fputc(pixels[i] & 255, f);
    fclose(f);
    fprintf(stderr, "Stopped at PC=%06x cycles=%lld powered_off=%d\n", pc24,
            (long long)cpu68k_clocks, im_powered_off);
    im_close();
    free(pixels);
    return 0;
}
