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
 * picod_packets.h - The internal API for request encode/decode modules.
 */

void picod_pack16 (uint8_t *buffer, int value);
void picod_pack32 (uint8_t *buffer, int value);
int  picod_unpack16 (const uint8_t *buffer);
int  picod_unpack32 (const uint8_t *buffer);

uint8_t *picod_packet_add_request (int index,
                                   uint8_t code, int size,
                                   PicodResponse *response, void *data);

void picod_packet_register (int index,
                            int code, PicodResponse *response, void *data);

