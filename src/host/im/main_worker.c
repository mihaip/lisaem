#include <vars.h>
#include <emscripten.h>
#include "emulator.h"

extern void im_disks_flush(void);
extern void im_disks_close(void);
extern uint8 is_lower_floppy_currently_inserted(void);
static int mouse_x = 360, mouse_y = 182;

EM_JS(int, input_value, (const char *name), {
    return workerApi.getInputValue(workerApi.InputBufferAddresses[UTF8ToString(name)]);
});
EM_JS(char *, consume_disk_name, (void), {
    const name = workerApi.disks.consumeDiskName();
    if (!name) return 0;
    const size = lengthBytesUTF8(name) + 1;
    const ptr = _malloc(size);
    stringToUTF8(name, ptr, size);
    return ptr;
});

static void poll_input(void)
{
    if (!EM_ASM_INT({ return workerApi.acquireInputLock(); })) return;
    if (input_value("mousePositionFlagAddr")) {
        mouse_x = input_value("mousePositionXAddr");
        mouse_y = input_value("mousePositionYAddr");
        add_mouse_event(mouse_x, mouse_y, 0);
    }
    int button = input_value("mouseButtonStateAddr");
    if (button >= 0) add_mouse_event(mouse_x, mouse_y, button ? 1 : -1);
    if (input_value("keyEventFlagAddr")) {
        int code = input_value("keyCodeAddr");
        int down = input_value("keyStateAddr");
        if (code == 0x100) { if (down) presspowerswitch(); }
        else send_cops_keycode(code | (down ? 0x80 : 0));
    }
    EM_ASM({ workerApi.releaseInputLock(); });
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: lisaem ROM DC42-DISK\n");
        return 1;
    }
    if (im_initialize(argv[1], argv[2], NULL, 0xa8)) return 1;
    uint32_t *pixels = malloc(720 * 364 * 4);
    if (!pixels) return 1;
    EM_ASM({ workerApi.didOpenVideo(720, 364); });
    EM_ASM({ workerApi.didOpenAudio(44100, 8, 1); });
    double deadline = emscripten_get_now();
    int booted = 0;
    while (!im_powered_off) {
        poll_input();
        if (input_value("stopFlagAddr")) break;
        if (!input_value("pausedAddr")) {
            /* 5 MHz, with input serviced every ~5 ms of guest execution. */
            for (int i = 0; i < 4 && !im_powered_off; i++) {
                im_execute(25000);
                poll_input();
            }
            if (!booted && cpu68k_clocks > 20000000) {
                apple_3();
                booted = 1;
            }
        }
        im_disks_flush();
        im_render(pixels);
        EM_ASM({ workerApi.blit($0, 720 * 364 * 4); }, pixels);
        EM_ASM({ workerApi.checkForPeriodicTasks(); });
        char *name = is_lower_floppy_currently_inserted() ? NULL : consume_disk_name();
        if (name) {
            floppy_insert(name, 0);
            free(name);
        }
        deadline += 20;
        double now = emscripten_get_now();
        if (deadline < now - 100) deadline = now;
        double wait = deadline > now ? (deadline - now) / 1000 : 0;
        EM_ASM({ workerApi.sleep($0); }, wait);
    }
    im_close();
    im_disks_close();
    free(pixels);
    EM_ASM({ workerApi.exit(); });
    return 0;
}
