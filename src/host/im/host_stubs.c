/* Optional desktop devices are disconnected in this frontend. */
#include <vars.h>

void messagebox(char *message, char *title) { fprintf(stderr, "%s: %s\n", title, message); }
int yesnomessagebox(char *message, char *title) { messagebox(message, title); return 0; }
int pickprofilesize(char *filename, int allowexisting) { return -2; }
void update_profile_preferences_path(char *filename) {}
void rename_rompath(char *filename) {}
void LisaScreenRefresh(void) {}
void contrastchange(void) {}
void eject_floppy_animation(void) {}
void floppy_motor_sounds(int track) {}
void connect_serial_devices(void) {}
void init_terminal_serial_port(int port) {}
void lisa_console_output(uint8 c) {}
void lpw_console_output(char *text) {}
void lpw_console_output_c(char c) {}
int ImageWriter_LisaEm_Init(int port) { return 0; }
void ImageWriterLoop(int port, uint8 c) {}
void iw_formfeed(int port) {}
void gen_clock_diff_sleep(long diff) {}

#define DISCONNECTED_SERIAL(name) \
    char read_serial_port_##name(unsigned int port) { return 0; } \
    void write_serial_port_##name(unsigned int port, char data) {}
DISCONNECTED_SERIAL(shell)
DISCONNECTED_SERIAL(pty)
DISCONNECTED_SERIAL(tty)
DISCONNECTED_SERIAL(telnetd)
DISCONNECTED_SERIAL(terminal)
int poll_telnet_serial_read(int port) { return 0; }
void check_telnet_connection(int port) {}
void set_dtr_pty(unsigned int port, uint8 value) {}
void set_port_baud_tty(int port, uint32 baud) {}
