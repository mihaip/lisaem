/* Infinite Mac frontend. Initialization follows the maintained wx frontend. */
#include <vars.h>
#include <reg68k.h>
#include <cpu68k.h>
#include "emulator.h"

int im_powered_off;
static const char *pram_path;
static XTIMER next_decisecond;

int im_initialize(const char *rom, const char *disk, const char *pram, int io_rom)
{
    buglog = stderr;
    pram_path = pram;
    for (int i = 0; i < 256; i++) {
        unsigned v = i ^ (i >> 4);
        eparity[i] = !((0x6996 >> (v & 15)) & 1);
    }
    segment1 = segment2 = 0;
    start = 1;
    scc_a_port = scc_b_port = NULL;
    scc_a_IW = scc_b_IW = -1;
    serial_a = serial_b = 0;
    if (pram) {
        FILE *f = fopen(pram, "rb");
        if (f) {
            if (fread(floppy_ram, 1, 1024, f) != 1024) {
                fclose(f);
                fprintf(stderr, "Invalid PRAM file: %s\n", pram);
                return -1;
            }
            fclose(f);
        }
    }
    floppy_iorom = io_rom;
    init_floppy(io_rom);
    bitdepth = 8;
    memset(serialnum, 0, 8);
    serialnumshiftcount = serialnumshift = 0;
    init_cops();
    init_IRQ();
    init_vias();
    init_Profiles();
    maxlisaram = 0x200000;
    minlisaram = 0x080000;
    TWOMEGMLIM = maxlisaram - 1;
    videolatchaddress = lastvideolatchaddress = maxlisaram - 0x10000;
    videolatch = lastvideolatch = (maxlisaram >> 15) - 1;
    lisaram = malloc(2 * 1024 * 1024 + 1024);
    if (!lisaram) return -1;
    memset(lisaram, 0xff, 2 * 1024 * 1024 + 1024);
    set_keyboard_id(0xbf2f);
    memset(serialnum240, 0, 16);
    serialnum240[0] = serialnum240[7] = 0xff;
    vidfixromchk(serialnum240);
    initialize_scc(0);
    softmemerror = harderror = videoirq = bustimeout = videobit = 0;
    reg68k_sanity_check_bitorder();
    cpu68k_init();
    init_lisa_mmu();
    init_ipct_allocator();
    if (read_rom((char *)rom, lisarom) || checkromchksum()) {
        fprintf(stderr, "Could not load a valid Lisa ROM: %s\n", rom);
        return -1;
    }
    fixromchk();
    lisa_vid_size_x = 720;
    lisa_vid_size_y = 364;
    lisa_vid_size_xbytes = 90;
    has_lisa_xl_screenmod = 0;
    if (has_xl_screenmod()) {
        fprintf(stderr, "The Infinite Mac frontend requires the standard Lisa display ROM\n");
        return -1;
    }
    if (disk) {
        via[2].ProFile = calloc(1, sizeof(ProFileType));
        if (!via[2].ProFile || profile_mount((char *)disk, via[2].ProFile)) return -1;
        via[2].ProFile->vianum = 2;
        ProfileReset(via[2].ProFile);
    }
    disable_vidram();
    videoramdirty = 32768;
    cpu68k_reset();
    next_decisecond = cpu68k_clocks + 500000;
    return 0;
}

int im_execute(int cycles)
{
    XTIMER before = cpu68k_clocks;
    seek_mouse_event();
    reg68k_external_execute(cycles);
    get_next_timer_event();
    while (cpu68k_clocks >= next_decisecond) {
        decisecond_clk_tick();
        next_decisecond += 500000;
    }
    seek_mouse_event();
    return (int)(cpu68k_clocks - before);
}

void im_render(uint32_t *pixels)
{
    for (unsigned i = 0; i < 720 * 364; i++) {
        uint8 byte = lisaram[videolatchaddress + (i >> 3)];
        pixels[i] = byte & (0x80 >> (i & 7)) ? 0xff000000 : 0xffffffff;
    }
}

void save_pram(void)
{
    if (!pram_path) return;
    FILE *f = fopen(pram_path, "wb");
    if (f) {
        fwrite(floppy_ram, 1, 1024, f);
        fclose(f);
    }
}

void im_close(void)
{
    save_pram();
    profile_unmount();
}

void lisa_powered_off(void) { im_powered_off = 1; }
void on_lisa_exit(void) { im_powered_off = 1; }
void lisa_rebooted(void) { im_powered_off = 1; }
