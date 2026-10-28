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
 * picod.h - The application-level public API for the picod C client library.
 */
const char *picod_initialize (int argc, const char **argv);

typedef void PicodListener (void);
void picod_listen (PicodListener *listener);

typedef void PicodTap (const char *action, const char *device,
                       const uint8_t *data, int length);
void picod_spy (PicodTap *tap);

int  picod_count (void);
int  picod_descriptor (int index);
int  picod_busy (int index);
int  picod_transmit (int fd);
void picod_receive (int fd);

const uint8_t *picod_uid (int index);
int            picod_version (int index);
const char    *picod_tty (int index);

int picod_signature (const uint8_t *key, int length);

typedef void PicodResponse (int index, void *data,
                            int status, const uint8_t *response);

int picod_start (const unsigned char *uid);
const char *picod_submit (int index);

const char *picod_reset (const uint8_t *uid);
void picod_background (int now);

