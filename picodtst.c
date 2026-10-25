/* A small tool to test communication with a Raspberry Pi Pico running picod
 *
 * Copyright 2026, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/time.h>

#include "picod_crc.h"

static int PicoDebugTrace = 0;
static int PicoLine = -1;

static void dumphex (const char *preamble, const uint8_t *data, int length) {

    printf ("%s:", preamble);
    while (length-- > 0) {
        printf (" %02x", *data++);
    }
    printf ("\n");
}

static void tracehex (int level,
                      const char *preamble, const uint8_t *data, int length) {
    if (PicoDebugTrace < level) return;
    dumphex (preamble, data, length);
}

static void pack16 (uint8_t *buf, int value) {
    buf[0] = (value >> 8) & 0xff;
    buf[1] = value & 0xff;
}

static void pack32 (uint8_t *buf, int value) {
    buf[0] = (value >> 24) & 0xff;
    buf[1] = (value >> 16) & 0xff;
    buf[2] = (value >> 8) & 0xff;
    buf[3] = value & 0xff;
}

static struct timeval StartTime;

static void timedevent (const char *mesg) {

    struct timeval now;
    gettimeofday (&now, 0);
    long long delta = (now.tv_sec - StartTime.tv_sec) * 1000 +
                      (now.tv_usec - StartTime.tv_usec) / 1000;
    printf ("# %06lld %s\n", delta, mesg);
}

static int unpack16 (const uint8_t *buf) {
    return ((((int)(buf[0])) & 0xff) << 8) + (((int)(buf[1])) & 0xff);
}

static int unpack32 (const uint8_t *buf) {
    return buf[3] +
           (((int)(buf[2])) << 8) +
           (((int)(buf[1])) << 16) + (((int)(buf[0])) << 24);
}

static void fatal (const char *mesg) {
    printf ("%s: %s\n", mesg, strerror(errno));
    exit (1);
}

static void packet_send (const uint8_t *data, int length) {

    uint8_t packet[1024];
    int packetlength = length + 5 + 2; // 5 is header, 2 is trailer CRC.

    packet[0] = 0xff;
    pack16 (packet+1, packetlength);
    pack16 (packet+3, picod_crc16 (packet, 3));

    memcpy (packet+5, data, length);
    pack16 (packet+packetlength-2, picod_crc16 (packet, packetlength-2));

    if (write (PicoLine, packet, packetlength) <= 0) fatal ("write error");
    tracehex (1, "Sent", packet, packetlength);
}

static void adjust_by_tick (struct timeval *out,
                            const struct timeval *in, int tick) {
    *out = *in;
    if (out->tv_usec < tick) {
        out->tv_sec -= 1;
        out->tv_usec += 1000000;
    }
    out->tv_usec -= tick;
}

static int packet_decode (const uint8_t *data, int length, int max, int *stop) {

    if (length < 5) return 0; // Need more data
    if (*data != 0xff) return 1;
    if (unpack16 (data+3) != picod_crc16 (data, 3)) return 1;

    // At this point the packet header is valid. trust the length
    int packetlength = unpack16 (data+1);
    if (packetlength > max) return length; // Too big.

    if (packetlength > length) return 0; // Need more data.
    tracehex (1, "decoding", data, length);

    if (unpack16 (data+packetlength-2) != picod_crc16 (data, packetlength-2))
        return packetlength; // Ignore the whole packet

    int cursor = 5;
    while (cursor < packetlength - 2) {
        const uint8_t *response = data + cursor;
        int responselength = unpack16 (response);
        cursor += responselength;

        int cmd = response[3];
        int status = response[4];
        if ((cmd & 0xf0) == 0xf0) status = 0; // No status in unsollicited msg
        if (status != 0) {
            printf ("Command %d returned error code %d\n", cmd, status);
            continue;
        }
        switch (cmd) {
            case 90:
                dumphex ("Pico UID", response+5, 8);
                break;
            case 13:
                dumphex ("GPIO Read", response+5, 4);
                break;
            case 99:
                dumphex ("Picod version", response+5, 4);
                break;
            case 12:
            case 14:
            case 30:
            case 32:
            case 95:
            case 96:
            case 97:
                printf ("Received %s response for command %d\n", status?"error":"OK", cmd);
                break; // No response data on these commands.
            case 0xfa:
                printf ("! Pico debug message: %*.*s\n",
                        responselength-4, responselength-4, response+4);
                *stop = 0;
                break;
            case 0xf9:
                printf ("! Pico fatal error: %*.*s\n",
                        responselength-4, responselength-4, response+4);
                *stop = 0;
                break;
            case 0xfb:
                dumphex ("GPIO alert", response+4, responselength-4);
                struct timeval now;
                struct timeval ts;
                gettimeofday (&now, 0);
                int cursor = responselength - 8;
                int basetick = unpack32 (response+cursor);
                for (cursor = 4; cursor < responselength; cursor += 8) {
                    int deltatick = basetick - unpack32 (response+cursor);
                    adjust_by_tick (&ts, &now, deltatick);
                    printf ("    %lld.%03ld %08x\n",
                            (long long)(ts.tv_sec), ts.tv_usec/1000,
                            unpack32 (response+cursor+4));
                }
                break;
            default:
                printf ("Unknown command %d\n", cmd);
        }
    }
    return packetlength; // The packet was processed
}

static void packet_wait (void) {

    uint8_t input[1024];
    int received = 0;
    for (;;) {
        int count = read (PicoLine, input+received, sizeof(input)-received);
        if (count <= 0) fatal ("read error");
        tracehex (2, "Received", input+received, count);
        received += count;

        int stop = 1;
        int consumed = packet_decode (input, received, sizeof(input), &stop);
        if (consumed <= 0) continue; // Not a complete packet yet.

        received -= consumed;
        if (received > 0) {
            memmove (input, input+consumed, received);
        } else if (stop) {
            return;
        }
    }
}

static void pico_cleanscan (struct dirent **files, int n) {
    int i;
    for (i = 0; i < n; i++) {
        free (files[i]);
    }
    if (files) free (files);
}

static int pico_find_filter (const struct dirent *ent) {
    if (ent->d_name[0] == '.') return 0;
    if (ent->d_type != DT_LNK) return 0;
    return 1;
}

// This function depends on the file 99-RPI-pico.rules to be installed
// in directory /etc/udev/rules.d
static const char *pico_find (void) {

    static const char PicoDevPath[] = "/dev/RPiPico";
    static char filename[512];

    struct dirent **devices = 0;
    int n = scandir (PicoDevPath, &devices, pico_find_filter, alphasort);
    if ((n <= 0) || (devices == 0)) return 0;
    int i;
    for (i = 0; i < n; i++) {
        // Select the first one found, for now
        snprintf (filename, sizeof(filename),
                  "%s/%s", PicoDevPath, devices[i]->d_name);
        pico_cleanscan (devices, n);
        return filename;
    }
    pico_cleanscan (devices, n);
    return 0;
}

static void pico_open (const char *line) {

    if (!line) {
        line = pico_find();
        if (!line) fatal ("cannot find any Pico device");
    }
    PicoLine = open(line, O_RDWR | O_NOCTTY);
    if (PicoLine < 0) fatal (line);
    printf ("Found RPI Pico device at %s\n", line);

    struct termios tty;
    if (tcgetattr(PicoLine, &tty) != 0) fatal ("tcgetattr error");

    cfsetospeed(&tty, B230400);
    cfsetispeed(&tty, B230400);

    // Configure 8N1 mode (8 data bits, no parity, 1 stop bit)
    tty.c_lflag = 0;                                 // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                                 // no remapping, no delays
    tty.c_iflag = 0;
    tty.c_cc[VMIN]  = 1;                             // read blocks until 1 character is available
    tty.c_cc[VTIME] = 5;                             // 0.5 seconds read timeout

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_cflag |= (CLOCAL | CREAD);                 // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);               // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;                         // disable hardware flow control

    if (tcsetattr(PicoLine, TCSANOW, &tty) != 0) fatal ("tcsetattr error");
}

static void cmd_no_parameter (int code) {
    uint8_t cmd[4];
    pack16 (cmd, 4);
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = code;
    packet_send (cmd, 4);
}

static void cmd_gpio_direction_set (int count,
                                    int gpio[], int dir[], int level[]) {

    int maskgpio = 0;
    int maskdirection = 0;
    int masklevel = 0;
    while (count-- > 0) {
        maskgpio |= (1 << gpio[count]);
        if (dir[count]) maskdirection |= (1 << gpio[count]);
        if (level[count]) masklevel |= (1 << gpio[count]);
    }

    uint8_t cmd[16];
    pack16 (cmd, 16);
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = 12;
    pack32 (cmd+4, maskgpio);
    pack32 (cmd+8, maskdirection);
    pack32 (cmd+12, masklevel);
    packet_send (cmd, 16);
}

static void cmd_gpio_read (void) {
    cmd_no_parameter (13);
}

static void cmd_gpio_write (int count, int gpio[], int level[]) {

    int maskgpio = 0;
    int masklevel = 0;
    while (count-- > 0) {
        maskgpio |= (1 << gpio[count]);
        if (level[count]) masklevel |= (1 << gpio[count]);
    }

    uint8_t cmd[12];
    pack16 (cmd, 12);
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = 14;
    pack32 (cmd+4, maskgpio);
    pack32 (cmd+8, masklevel);
    packet_send (cmd, 12);
}

static void cmd_debounce (int gpio, int timer) {

    uint8_t cmd[9];
    pack16 (cmd, sizeof(cmd));
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = 30;
    cmd[4] = gpio;
    pack32 (cmd+5, timer);
    packet_send (cmd, sizeof(cmd));
}

static void cmd_alert (int count, int gpio[], int enable) {

    int maskgpio = 0;
    while (count-- > 0) {
        maskgpio |= (1 << gpio[count]);
    }

    uint8_t cmd[12];
    pack16 (cmd, 12);
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = 32;
    pack32 (cmd+4, maskgpio);
    pack32 (cmd+8, enable?maskgpio:0);
    packet_send (cmd, 12);
}

static void cmd_uid (void) {
    cmd_no_parameter (90);
}

static void cmd_sleep (int duration) {
    uint8_t cmd[8];
    pack16 (cmd, 8);
    cmd[2] = (1 << 6); // Ask for a reply
    cmd[3] = 95;
    pack32 (cmd+4, duration);
    packet_send (cmd, 8);
}

static void cmd_reset (void) {
    cmd_no_parameter (96);
}

static void cmd_version (void) {
    cmd_no_parameter (99);
}

static void cmd_debug (int levels) {
    uint8_t cmd[12];
    pack16 (cmd, 12);
    cmd[2] = (1 << 6);
    cmd[3] = 97;
    pack32 (cmd+4, 0); // DEBUG.
    pack32 (cmd+8, levels);
    packet_send (cmd, sizeof(cmd));
}

int main (int argc, const char *argv[]) {

    int i;
    for (i = 1; i < argc; ++i) {
        if (!strcmp (argv[i], "-t")) PicoDebugTrace += 1;
    }

    gettimeofday (&StartTime, 0);

    // pico_open ("/dev/ttyACM0");
    pico_open (0);
    timedevent ("Device is now open");

    cmd_reset ();
    timedevent ("Reset command sent");
    packet_wait ();
    timedevent ("Reset response received");

    cmd_version ();
    timedevent ("Get Version command sent");
    packet_wait ();
    timedevent ("Get Version response received");

    cmd_uid ();
    timedevent ("UID command sent");
    packet_wait ();
    timedevent ("UID response received");

    cmd_sleep (100000);
    timedevent ("Sleep 100 ms command sent");
    packet_wait ();
    timedevent ("Sleep response received");

    if (PicoDebugTrace) {
        cmd_debug (0x1f);
        packet_wait ();
    }

    static int gpio[] = {1, 3, 5, 7};
    static int dir[] = {1, 0, 1, 0};
    static int level[] = {0, 0, 1, 1};

    cmd_gpio_direction_set (4, gpio, dir, level);
    timedevent ("GPIO direction set (1=out,0 3=in,0 5=out,1 7=in,1) command sent");
    packet_wait ();
    timedevent ("GPIO direction set response received");

    cmd_gpio_read ();
    timedevent ("GPIO read command sent");
    packet_wait ();
    timedevent ("GPIO read response received");

    static int gpiowrite[] = {1, 5};
    static int levelwrite[] = {1, 0};

    cmd_gpio_write (2, gpiowrite, levelwrite);
    timedevent ("GPIO write (1=1 5=0) command sent");
    packet_wait ();
    timedevent ("GPIO write response received");

    cmd_gpio_read ();
    timedevent ("GPIO read command sent");
    packet_wait ();
    timedevent ("GPIO read response received");

    cmd_debounce (3, 5000);
    packet_wait ();
    cmd_debounce (7, 5000);
    packet_wait ();

    static int gpioread[] = {3, 7};
    cmd_alert (2, gpioread, 1);
    packet_wait ();
    timedevent ("RPi Pico is now ready for alerts on GPIO pins 3 and 7");
    for (i = 0; i < 4; ++i) packet_wait ();
    timedevent ("Done with alerts");
    cmd_alert (2, gpioread, 0);
    packet_wait ();

    return 0;
}

