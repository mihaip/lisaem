/* Small native harness for comparing this frontend with the wx reference. */
#include <vars.h>
#include "emulator.h"
extern uint8 is_lower_floppy_currently_inserted(void);

/* Optional replay: guest-seconds move|down|up X Y, guest-seconds key COPS,
 * or guest-seconds floppy PATH. Equal timestamps exercise move/click ordering.
 * LISAEM_BOOT_FLOPPY selects a private installer instead of booting ProFile. */
static FILE *input;
static double input_seconds;
static char input_command[16];
static char input_path[2048];
static int input_x, input_y, have_input;

static void read_input(void)
{
    char line[2304];
    have_input = 0;
    while (input && fgets(line, sizeof(line), input)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int path_start = 0;
        if (sscanf(line, "%lf %15s %n", &input_seconds, input_command, &path_start) == 2 &&
            !strcmp(input_command, "floppy")) {
            snprintf(input_path, sizeof(input_path), "%s", line + path_start);
            input_path[strcspn(input_path, "\r\n")] = 0;
            have_input = 1;
            break;
        }
        int count = sscanf(line, "%lf %15s %i %i", &input_seconds,
                           input_command, &input_x, &input_y);
        if (count < 3 || (strcmp(input_command, "key") && count != 4)) {
            fprintf(stderr, "Invalid input replay line: %s", line);
            exit(1);
        }
        have_input = 1;
        break;
    }
}

int main(int argc, char **argv)
{
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "Usage: lisaem-im ROM DISK SECONDS OUTPUT.pgm [INPUT.txt]\n");
        return 1;
    }
    if (argc == 6) {
        input = fopen(argv[5], "r");
        if (!input) return 1;
        read_input();
    }
    if (im_initialize(argv[1], argv[2], "pram.bin", 0xa8)) return 1;
    const char *boot_floppy = getenv("LISAEM_BOOT_FLOPPY");
    if (boot_floppy && floppy_insert((char *)boot_floppy, 0)) return 1;
    XTIMER end = (XTIMER)atoi(argv[3]) * 5000000;
    int booted = 0;
    while (!im_powered_off && cpu68k_clocks < end) {
        while (have_input && cpu68k_clocks >= input_seconds * 5000000) {
            if (!strcmp(input_command, "floppy")) {
                if (is_lower_floppy_currently_inserted()) {
                    fprintf(stderr, "Replay attempted to replace an inserted floppy at %.1f seconds\n", input_seconds);
                    im_close();
                    return 1;
                }
                fprintf(stderr, "Inserting %s at %.1f seconds\n", input_path, input_seconds);
                if (floppy_insert(input_path, 0)) {
                    fprintf(stderr, "Could not insert %s\n", input_path);
                    return 1;
                }
            }
            else if (!strcmp(input_command, "key")) send_cops_keycode(input_x);
            else if (!strcmp(input_command, "move")) add_mouse_event(input_x, input_y, 0);
            else if (!strcmp(input_command, "down")) add_mouse_event(input_x, input_y, 1);
            else if (!strcmp(input_command, "up")) add_mouse_event(input_x, input_y, -1);
            else {
                fprintf(stderr, "Unknown input command: %s\n", input_command);
                return 1;
            }
            read_input();
        }
        im_execute(25000);
        if (!booted && cpu68k_clocks > 20000000) {
            if (boot_floppy) apple_2();
            else apple_3();
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
    if (input) fclose(input);
    free(pixels);
    return 0;
}
