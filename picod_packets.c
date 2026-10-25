/* Picod C Client Library - A C API to access RPi Pico boards running picod
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
 *
 *
 * picod_packets.c - The module that manage connections to Pico boards.
 *
 * SYNOPSYS:
 *
 * const char *picod_initialize (int argc, char **argv);
 *
 *    Initialize the library. Supported command line options:
 *
 *       -pico=STRING   Path to a Pico serial device (typically in /dev).
 *
 *    Note that the -device option can be repeated, allowing support
 *    of multiple Pico boards simultaneously. Using the -device option
 *    disables auto discovery of Pico devices.
 *
 *    Return a null string on success, an error message on failure.
 *
 * void picod_listen (PicodListener *listener);
 *
 *    Listen to changes in the list of detected RPi Pico devices.
 *    The listener callback will be invoked each time a new Pico device
 *    has been identified, or when a known Pico device has disappeared.
 *
 *    A Pico device is identified when its UID has been acquired.
 *
 *    The listener can use picod_count() and picod_uid() to retrieve
 *    the current list of devices:
 *
 *        int count = picod_count();
 *        while (count-- > 0) {
 *            uint8_t *uid = picod_uid (count);
 *            if (!uid) continue;
 *            .. application logic here ..
 *        }
 *
 *    A Pico device may be listed and not have an associated UID yet:
 *    ignore the entries returning a null UID. Either the UID will be
 *    acquired later or this RPi Pico board is probably not running picod.
 *
 *    This listener will be invoked even when discovery is disabled: in
 *    that mode the library still detects reconnection events.
 *
 *    Calling picod_listen() with a null parameter disables the listening.
 *
 * void picod_spy (PicodTap *tap);
 *
 *    Install a tap to listen to packets sent and received.
 *    This is more of a debug feature.
 *
 * void picod_count (void);
 * int  picod_descriptor (int index);
 * int  picod_busy (int index);
 * void picod_receive (int fd);
 * int  picod_transmit (int fd);
 *
 *    These five function are typically used in a select() or poll() loop.
 *    The list of Pico devices may change from time to time: walk through
 *    the list each time and ignore any returned descriptor that is 0 or less.
 *
 *    Function picod_count() returns the total number of Pico devices known
 *    at that time. Some Pico devices may has been disconnected, or may have
 *    not been identified yet.
 *
 *    Function picod_descriptor() returns the file descriptor associated
 *    with the specified Pico device, or -1 if the device is not accessible.
 *    The file descriptor for a specific Pico device should not be monitored
 *    for writing if picod_busy() return 0, i.e. there is no data to send.
 *
 *    Function picod_transmit() should be called when the file descriptor is
 *    ready for writing. It returns 0 when there is no more data to send.
 *
 *    Function picod_receive() should be called when the file descriptor is
 *    ready for reading. It reads and processes Pico data.
 *
 *    Example using select():
 *
 *        int fdmax = 0;
 *        fd_set reads;
 *        fd_set writes;
 *        ...
 *        int count = picod_count();
 *        while (count-- > 0) {
 *            int fd = picod_descriptor (count);
 *            if (fd <= 0) continue;
 *            if (fdmax < fd) fdmax = fd;
 *            FD_SET (fd, &reads);
 *            if (picod_busy (count)) FD_SET (fd, &writes);
 *        }
 *        ...
 *        select (fdmax+1, &reads, &writes, 0, 0);
 *        count = picod_count();
 *        while (count-- > 0) {
 *            int fd = picod_descriptor (count);
 *            if (fd <= 0) continue;
 *            if (FD_ISSET (fd, &reads)) picod_receive (fd);
 *            if (FD_ISSET (fd, &writes)) picod_transmit (fd);
 *        }
 *
 * const unsigned char *picod_uid (int index);
 *
 *    Return the UID of the Pico board, or null when the UID has not
 *    been acquired yet.
 *
 * int picod_signature (const uint8_t *key, int length);
 *
 *    This is a helper function that can be used to build hash indexes
 *    using a UID.
 *
 * int picod_version (int index);
 *
 *    Return the version of the firmware running on the specified device,
 *    or 0 if that version is not known yet.
 *
 * int picod_start (const unsigned char *uid);
 *
 *    Prepare a new (empty) packet context for the specified Pico board.
 *
 *    Return the current Pico device index given the Pico UID, or -1 when
 *    no Pico with the specified UID can be found.
 *
 *    The index value for the same Pico board may change after waiting for I/O.
 *
 * const char *picod_submit (int index);
 *
 *    Send all commands accumulated for the specified Pico device.
 *    Return null on success, an error message on failure.
 *
 *    The application will typically call picod_start(), then one or more
 *    Picod command functions, then picod_submit() to transmit the requests
 *    all at once:
 *
 *        int index = picod_start (uid);
 *        if (index >= 0) {
 *            .. add commands here ..
 *            picod_submit (index);
 *        }
 *
 *    The Picod command functions are implemented in separate modules.
 *
 * const char *picod_reset (const uint8_t *uid);
 *
 *    Send a reset command to the specified Pico device. No response
 *    is expected. Return null on success, an error message on failure.
 *
 * TBD: picod_debug (int index, PicodResponse *unsollicited) to enable debug and capture debug messages.
 *
 * void picod_background (int now);
 *
 *    Periodic processing. Typically this covers continuous discovvery
 *    of Pico devices.
 *
 * The following API is for internal purpose only. This is in support
 * of request encoding and decoding.
 *
 * void picod_pack16 (uint8_t *buffer, int value);
 * void picod_pack32 (uint8_t *buffer, int value);
 * int  picod_unpack16 (const uint8_t *buffer);
 * int  picod_unpack32 (const uint8_t *buffer);
 *
 *    Pack and unpack multibyte values using the endianess expected by picod.
 *
 * uint8_t *picod_packet_add_request (int index,
 *                                    uint8_t code, int size,
 *                                    PicodResponse *response, void *data);
 *
 *    Reserve room for one more request in the transmission buffer.
 *    This returns a pointer to the packet in the buffer.
 *    This function is normally called from an API function that encodes
 *    a picod request.
 *
 * void picod_packet_register (int index,
 *                             int code, PicodResponse *response, void *data);
 *
 *    Register a response callback to an arbitrary request code.
 *    This is meant for unsollicited messages.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/time.h>

#include "picod.h"
#include "picod_crc.h"
#include "picod_packets.h"

#define PICODPACKETMAX 32764

struct PicodBuffer {
    int count;
    int consumed;
    uint8_t data[PICODPACKETMAX];
};

struct PicodCaller {
    PicodResponse *response;
    void *data;
    int transaction;
};

struct PicodDeviceLink {
    char *tty;
    int ttysignature;
    uint8_t uid[8];
    int uidsignature;
    int index;
    int version;
    int fd;
    int discovered;
    struct PicodCaller pending[256];
    struct PicodBuffer in;
    struct PicodBuffer out;
};

static int PicodDevicesDiscovery = 1;

#define PICODEVICESMAX 16
static struct PicodDeviceLink *PicodDevices[PICODEVICESMAX];
static int PicodDevicesCount = 0;

static PicodListener *PicodDiscoveryListener = 0;
static PicodTap *PicodTapCallback = 0;

void picod_pack16 (uint8_t *buffer, int value) {
    *buffer++ = (value >> 8) & 0xff;
    *buffer = value & 0xff;
}

void picod_pack32 (uint8_t *buffer, int value) {
    *buffer++ = (value >> 24) & 0xff;
    *buffer++ = (value >> 16) & 0xff;
    *buffer++ = (value >> 8) & 0xff;
    *buffer = value & 0xff;
}

int picod_unpack16 (const uint8_t *buffer) {
    return ((((int)(buffer[0])) & 0xff) << 8) + (((int)(buffer[1])) & 0xff);
}

int picod_unpack32 (const uint8_t *buffer) {
    return buffer[3] +
           (((int)(buffer[2])) << 8) +
           (((int)(buffer[1])) << 16) + (((int)(buffer[0])) << 24);
}

// Signature for hash indexes. This handle both null terminated strings
// (length == 0) and binary data (length > 0).
//
// This hash function is derived from Daniel J. Bernstein's djb2 hash function.
//
int picod_signature (const uint8_t *key, int length) {
    int hash = 5381;
    if (length > 0) {
        int i;
        for (i = 0; i < length; ++i)
            hash = ((hash << 5) + hash) + key[i]; /* hash * 33 + c */
    } else {
        int c;
        while ((c = *key++) != 0)
            hash = ((hash << 5) + hash) + tolower(c); /* hash * 33 + c */
    }
    return hash;
}

static struct PicodDeviceLink *picod_find_by_tty (const char *tty) {
    int i;
    int signature = picod_signature ((const uint8_t *)tty, 0);
    for (i = 0; i < PicodDevicesCount; ++i) {
        struct PicodDeviceLink *link = PicodDevices[i];
        if (!link) continue;
        if (link->ttysignature != signature) continue; // Faster
        if (!strcmp (tty, link->tty)) return link; // Slower
    }
    return 0;
}

static struct PicodDeviceLink *picod_find_by_uid (const uint8_t *uid) {
    int i;
    int signature = picod_signature (uid, 8);
    for (i = 0; i < PicodDevicesCount; ++i) {
        struct PicodDeviceLink *link = PicodDevices[i];
        if (!link) continue;
        if (link->uidsignature != signature) continue; // Faster
        if (!memcmp (link->uid, uid, 8)) return link; // Slower
    }
    return 0;
}

struct PicodDeviceLink *picod_find_by_fd (int fd) {
    int i;
    for (i = 0; i < PicodDevicesCount; ++i) {
        struct PicodDeviceLink *link = PicodDevices[i];
        if (link && (link->fd == fd)) return link;
    }
    return 0;
}

static struct PicodDeviceLink *picod_access (int index) {
    if ((index < 0) || (index >= PicodDevicesCount)) return 0;
    return PicodDevices[index];
}

static void picod_drain (struct PicodDeviceLink *link) {
    int i;
    for (i = 0; i < 0xf0; ++i) {
        link->pending[i].response = 0;
    }
    link->in.count = link->in.consumed = 0;
    link->out.count = link->out.consumed = 0;
}

static void picod_close (struct PicodDeviceLink *link) {
    picod_drain (link);
    if (link->fd > 0) {
        close (link->fd);
        link->fd = -1;
    }
    link->version = 0;
}

static void picod_erase (int index) {

    struct PicodDeviceLink *link = PicodDevices[index];
    if (!link) return;
    if (link->tty) free (link->tty);
    if (link->fd > 0) close (link->fd);
    free (link);
    PicodDevices[index] = 0;
}

static void picod_wipeout (void) {
    int i;
    for (i = 0; i < PicodDevicesCount; ++i) picod_erase (i);
    PicodDevicesCount = 0;
}

static void picod_receive_uid (int index, void *data,
                               int status, const uint8_t *response) {

    struct PicodDeviceLink *link = PicodDevices[index];
    if (!link) return;
    memcpy (link->uid, response+5, 8);
    link->uidsignature = picod_signature (link->uid, 8);
}

static void picod_receive_version (int index, void *data,
                                   int status, const uint8_t *response) {

    struct PicodDeviceLink *link = PicodDevices[index];
    if (!link) return;
    link->version = picod_unpack32 (response+5);

    // The version is used as a flag that the UID is available.
    if (PicodDiscoveryListener) PicodDiscoveryListener();
}

static void picod_open (struct PicodDeviceLink *link) {

    picod_close (link);

    link->fd = open(link->tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (link->fd < 0) return;

    struct termios ttyio;
    if (tcgetattr(link->fd, &ttyio) != 0) goto abort;

    cfsetospeed(&ttyio, B230400);
    cfsetispeed(&ttyio, B230400);

    // Configure 8N1 mode (8 data bits, no parity, 1 stop bit)
    ttyio.c_lflag = 0;                              // no signaling chars, no echo, no canonical processing
    ttyio.c_oflag = 0;                              // no remapping, no delays
    ttyio.c_iflag = 0;
    ttyio.c_cc[VMIN]  = 1;                          // read blocks until 1 character is available
    ttyio.c_cc[VTIME] = 5;                          // 0.5 seconds read timeout

    ttyio.c_cflag = (ttyio.c_cflag & ~CSIZE) | CS8; // 8-bit chars
    ttyio.c_cflag |= (CLOCAL | CREAD);              // ignore modem controls, enable reading
    ttyio.c_cflag &= ~(PARENB | PARODD);            // shut off parity
    ttyio.c_cflag &= ~CSTOPB;
    ttyio.c_cflag &= ~CRTSCTS;                      // disable hardware flow control

    if (tcsetattr(link->fd, TCSANOW, &ttyio) != 0) goto abort;

    // Immediately submit requests for the device UID and picod version,
    // in that order.
    picod_packet_add_request (link->index, 90, 4, picod_receive_uid, 0);
    picod_packet_add_request (link->index, 99, 4, picod_receive_version, 0);
    if (picod_submit (link->index)) goto abort;
    return;

abort:
    close (link->fd);
    link->fd = -1;
}

static void picod_discovered (const char *tty) {

    struct PicodDeviceLink *link = picod_find_by_tty (tty);
    if (!link) {
        int i;
        for (i = 0; i < PicodDevicesCount; ++i) {
            if (!PicodDevices[i]) break; // Reuse empty slots
        }
        if (i >= PicodDevicesCount) {
            if (i >= PICODEVICESMAX) return; // Table is full, ignore
            i = PicodDevicesCount++;
        }
        if (!PicodDevices[i])
            PicodDevices[i] = malloc (sizeof (struct PicodDeviceLink));
        link = PicodDevices[i];
        link->tty = strdup (tty);
        link->ttysignature = picod_signature ((const uint8_t *)tty, 0);
        link->fd = -1;
        link->version = 0;
        link->index = i;
        picod_drain (link);
    }

    if (link->fd <= 0) picod_open (link);
    link->discovered = 1;
}

static void picod_cleanscan (struct dirent **files, int n) {
    int i;
    for (i = 0; i < n; i++) {
        free (files[i]);
    }
    if (files) free (files);
}

static int picod_find_filter (const struct dirent *ent) {
    if (ent->d_name[0] == '.') return 0;
    if (ent->d_type != DT_LNK) return 0;
    return 1;
}

static void picod_discover (void) {

    if (! PicodDevicesDiscovery) return;

    static const char PicoDevPath[] = "/dev/RPiPico";

    int changed = 0;
    char filename[512];
    struct dirent **devices = 0;
    int n = scandir (PicoDevPath, &devices, picod_find_filter, alphasort);
    if ((n <= 0) || (devices == 0)) {
        changed = (PicodDevicesCount > 0);
        picod_wipeout ();
        if (changed && PicodDiscoveryListener) PicodDiscoveryListener();
        return;
    }

    int i;
    for (i = 0; i < PicodDevicesCount; ++i) {
        if (!PicodDevices[i]) continue;
        PicodDevices[i]->discovered = 0;
    }
    for (i = 0; i < n; i++) {
        snprintf (filename, sizeof(filename),
                  "%s/%s", PicoDevPath, devices[i]->d_name);
        picod_discovered (filename);
    }
    picod_cleanscan (devices, n);

    // Eliminate all the Pico devices that were disconnected
    // Shrink the table of devices whenever possible, leave holes
    // otherwise.
    //
    for (i = PicodDevicesCount-1; i >= 0; --i) {
        if (!PicodDevices[i]) continue;
        if (PicodDevices[i]->discovered) continue;
        picod_erase (i);
        if (i == PicodDevicesCount-1) {
            PicodDevicesCount -= 1;
        }
        changed = 1;
    }
    if (changed && PicodDiscoveryListener) PicodDiscoveryListener();
}

const char *picod_initialize (int argc, const char **argv) {

    int i;
    for (i = 1; i < argc; ++i) {
        const char *p = argv[i];
        if (*p != '-') continue;
        while (*p == '-') p += 1;
        if (!strncmp (p, "pico=", 5)) picod_discovered (p+5);
    }
    if (PicodDevicesCount > 0) PicodDevicesDiscovery = 0;
    else picod_discover ();
    return 0;
}

void picod_listen (PicodListener *listener) {
    PicodDiscoveryListener = listener;
}

void picod_spy (PicodTap *tap) {
    PicodTapCallback = tap;
}

int picod_count (void) {
    return PicodDevicesCount;
}

int picod_descriptor (int index) {
    struct PicodDeviceLink *link = picod_access (index);
    if (!link) return -1;
    return link->fd;
}

const unsigned char *picod_uid (int index) {
    struct PicodDeviceLink *link = picod_access (index);
    if (link && (link->version)) return link->uid; // Use version as flag.
    return 0;
}

int picod_version (int index) {
    struct PicodDeviceLink *link = picod_access (index);
    if (link) return link->version;
    return 0;
}

int picod_start (const unsigned char *uid) {

    struct PicodDeviceLink *link = picod_find_by_uid (uid);
    if ((!link) || (link->fd <= 0)) return -1;
    picod_drain (link);
    return link->index;
}

uint8_t *picod_packet_add_request (int index,
                                   uint8_t code, int size,
                                   PicodResponse *response, void *data) {

    struct PicodDeviceLink *link = picod_access (index);
    if (!link) return 0;

    if (link->out.count <= 0) link->out.count = 5; // Room for packet header.
    if (link->out.count + size + 2 >= sizeof(link->out.data)) return 0;

    uint8_t *start = link->out.data + link->out.count;
    link->out.count += size;

    picod_pack16 (start, size);
    if (response) {
        struct PicodCaller *pending = link->pending + code;
        pending->response = response;
        pending->data = data;
        if (++(pending->transaction) > 0x3f) pending->transaction = 1;
        start[2] = (1 << 6) + pending->transaction;
    } else {
        start[2] = 0;
    }
    start[3] = code;
    return start;
}

void picod_packet_register (int index,
                            int code, PicodResponse *response, void *data) {

    if ((code < 0) || (code > 255)) return;

    struct PicodDeviceLink *link = picod_access (index);
    if (!link) return;

    struct PicodCaller *pending = link->pending + code;
    pending->response = response;
    pending->data = data;
}

const char *picod_submit (int index) {

    struct PicodDeviceLink *link = picod_access (index);
    if (!link) return "invalid pico device index";
    if (link->fd <= 0) return "no access to pico device";

    uint8_t *data = link->out.data;
    int count = link->out.count;
    if (count <= 0) return "no data";
    int length = count + 2; // Will append the CRC.

    data[0] = 0xff;
    picod_pack16 (data+1, length);
    picod_pack16 (data+3, picod_crc16 (data, 3));
    picod_pack16 (data+count, picod_crc16 (data, count));

    if (PicodTapCallback) PicodTapCallback ("Sending", link->tty, data, length);

    int sent = write (link->fd, data, length);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0; // Send later
        picod_close (link);
        return "write error";
    }
    if (sent >= length) {
        link->out.consumed = link->out.count = 0;
        return 0;
    }
    link->out.consumed = sent;
    return 0;
}

// Handle packets with multiple responses (the picod code never do this,
// but this might change in a subsequence version) and multiple packets
// in the input buffer (the picod code does send packets back to back).
//
static int picode_packet_decode (struct PicodDeviceLink *link) {

    uint8_t *data = link->in.data + link->in.consumed;
    int count = link->in.count - link->in.consumed; // Bytes received

    if (count < 5) return 0; // Need more data
    if (data[0] != 0xff) return 1;
    if (picod_unpack16 (data+3) != picod_crc16 (data, 3)) {
        if (PicodTapCallback) PicodTapCallback ("Bad Head CRC", link->tty, data, 3);
        return 1;
    }

    // The packet header is valid, trust the length
    int length = picod_unpack16 (data+1); // Length of the first packet received
    if (length > sizeof (link->in.data)) {
        link->in.consumed = link->in.count = 0;
        return -1; // Too big.
    }
    if (length > count) return 0; // Need more data.

    if (picod_unpack16 (data + length - 2) != picod_crc16 (data, length-2)) {
        if (PicodTapCallback)
            PicodTapCallback ("Bad Data CRC", link->tty, data, length);
        return length; // Ignore the whole packet when corrupted
    }

    if (PicodTapCallback)
        PicodTapCallback ("Received", link->tty, data, count);

    int cursor = 5;
    while (cursor < length - 2) {
        const uint8_t *response = data + cursor;
        int responselength = picod_unpack16 (response);
        cursor += responselength;

        int cmd = response[3];
        int unsollicited = ((cmd & 0xf0) == 0xf0);
        struct PicodCaller *pending = link->pending + cmd;

        int transaction = response[2] & 0x3f;
        if (pending->response && (transaction == pending->transaction)) {
            int status = unsollicited ? 0 : response[4];
            pending->response (link->index, pending->data, status, response);
            if (!unsollicited) pending->response = 0; // Consumed.
        }
    }
    return length;
}

static int picod_read_input (struct PicodDeviceLink *link) {

    uint8_t *data = link->in.data + link->in.count;
    int count = sizeof(link->in.data) - link->in.count;

    if (count < 100) {
        // Not enough data left for efficient operations.
        // Try to reclaim space
        if (link->in.consumed > 0) {
            link->in.count -= link->in.consumed;
            memmove (link->in.data,
                     link->in.data+link->in.consumed, link->in.count);
            link->in.consumed = 0;
            data = link->in.data + link->in.count;
            count = sizeof(link->in.data) - link->in.count;
        }
    }

    count = read (link->fd, data, count);
    if (count <= 0) {
        picod_close (link);
        return 0;
    }
    link->in.count += count;
    return 1;
}

void picod_receive (int fd) {

    struct PicodDeviceLink *link = picod_find_by_fd (fd);
    if (!link) {
        // This device is no longer known. Why this call??
        // To avoid a vicious cycle, consume all data regardless.
        char ignored[1024];
        read (fd, ignored, sizeof(ignored));
        return;
    }

    if (! picod_read_input (link)) return;

    while (link->in.count > link->in.consumed) {

        int count = picode_packet_decode (link);
        if (count <= 0) break;
        link->in.consumed += count;
    }
}

int picod_busy (int index) {

    struct PicodDeviceLink *link = picod_access (index);
    if (!link) return 0;

    if (link->out.count > link->out.consumed) return 1;

    if (link->out.count > 0) link->out.count = link->out.consumed = 0;
    return 0;
}

int picod_transmit (int fd) {

    struct PicodDeviceLink *link = picod_find_by_fd (fd);
    if (!link) return 0;

    if (link->out.count <= link->out.consumed) {
        link->out.count = link->out.consumed = 0;
        return 0;
    }

    uint8_t *data = link->out.data + link->out.consumed;
    int length = link->out.count - link->out.consumed;

    int sent = write (link->fd, data, length);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1; // Send later
        picod_close (link);
        return 0;
    }
    if (sent >= length) {
        link->out.consumed = link->out.count = 0;
        return 0;
    }
    link->out.consumed += sent;
    return 1;
}

const char *picod_reset (const uint8_t *uid) {

    int index = picod_start (uid);
    if (index < 0) return "Unknown Pico device";

    picod_packet_add_request (index, 96, 4, 0, 0);
    return picod_submit (index);
}

void picod_background (int now) {

    static time_t NextScan = 0;
    if (!NextScan) {
        NextScan = now + 10;
        return;
    }
    if (now < NextScan) return;
    NextScan += 10;

    picod_discover ();
}

