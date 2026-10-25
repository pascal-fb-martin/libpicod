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
 * picod_gpio.c - The module that manages picod GPIO requests.
 *
 * SYNOPSYS:
 *
 * Most functions here take response and data callback parameters. These
 * are meant to allow the caller to process the picod response asynchronously.
 * Set the response callback to null (0) if no response is needed. The
 * response parameter is a callback function to be called, the data parameter
 * is a caller context that will be provided when the callback is fired.
 *
 * No request is sent until picod_submit() is called.
 *
 * const char *picod_gpio_direction (int device, int count,
 *                                   int gpio[], int dir[], int level[],
 *                                   PicodResponse *response, void *data);
 *
 *    Build a request to set GPIO pins direction and initial level on
 *    the specified Pico device. The gpio array provides the GPIO pin numbers.
 *    The dir array indicate the direction for each pin, either input (0) or
 *    output (1). The level array provides the initial state for the pin,
 *    either low (0) or high (1).
 *
 * const char *picod_gpio_pull (int device, int count,
 *                              int *gpio, int *pullup, int *pulldown,
 *                              PicodResponse *response, void *data);
 *
 *    Build a request to enable the GPIO pull up and down on the specified
 *    Pico device.
 *
 *    The gpio array provides the list of GPIO pins to configure.
 *
 *    If the gpio array is null, the request is to get the current
 *    configuration and pullup, pulldown are ignored.
 *    See picod_gpio_pull_up() and picod_gpio_pull_down().
 *
 *    The up array indicate if the pull-up resistor must be enabled
 *    The pulldown array indicates if the pull-down resistor must be enabled.
 *
 *    If either pullup or pulldown is null, the corresponding pull is
 *    never enabled.
 *
 * void picod_gpio_pull_up (const uint8_t *response, int pullup[]);
 *
 *    Decode the current state of the pull-up resistor, indexed by the
 *    GPIO pin number. The pullup array must have 30 elements.
 *
 * void picod_gpio_pull_down (const uint8_t *response, int pulldown[]);
 *
 *    Decode the current state of the pull-down resistor, indexed by the
 *    GPIO pin number. The pulldown array must have 30 elements or more.
 *
 * const char *picod_gpio_read (int device,
 *                              PicodResponse *response, void *data);
 *
 *    Build a GPIO read request.
 *
 * picod_gpio_read_levels (const uint8_t *response, int *levels);
 *
 *    Retrieve the GPIO pins state after a response from the Pico device
 *    has been received. The level array must have 30 elements or more.
 *
 * const char *picod_gpio_write (int device,
 *                               int count, int gpio[], int level[],
 *                               PicodResponse *response, void *data);
 *
 *    Build a GPIO write request.
 *
 *
 * const char *picod_gpio_debounce (int device, int gpio, int timer,
 *                                  PicodResponse *response, void *data);
 *
 *    Enable a debounce timer on the specified GPIO pin. The debounce
 *    timer is a filter that block short timed pulses (bounces) that
 *    are often caused by transient contacts when a switch blade moves.
 *
 * const char *picod_gpio_alert (int device, int count, int gpio[],
 *                               PicodResponse *response,
 *                               PicodResponse *unsollicited, void *data);
 *
 *    Register for (unsollicted != 0), or deregister from (unsollicted == 0),
 *    alerts on the selected GPIO pins.
 *
 *    The unsollicited callback will be called each time an unsollicited
 *    message is received from the Pico device. This is a sticky callback:
 *    it can be replaced but it cannot be removed. That should not matter
 *    much: if all alerts were disabled, the picod unsollicited messages
 *    will stop anyway.
 *
 * int picod_gpio_alert_count (const uint8_t *response);
 * int      picod_gpio_alert_watchdog  (int i, const uint8_t *response);
 * uint32_t picod_gpio_alert_timestamp (int i, const uint8_t *response);
 * uint32_t picod_gpio_alert_levels    (int i, const uint8_t *response);
 *
 *    Decode the unsollicted alert message, one report at a time.
 *    There are two types of reports:
 *    - Watchdog reports indicate pins without changes for a specified time.
 *    - Other reports when states did change.
 *
 *    WARNING: the value returned by picod_gpio_alert_timestamp() is the raw
 *    Pico tick, not synchronized to the local time. Basic rule is to take
 *    the local time when the response was received, consider that it matches
 *    the tick of the last report and then evaluate the time of previous
 *    report items in this message relative to the last.
 */
#include <stdint.h>
#include <stdio.h> // Debug

#include "picod.h"
#include "picod_packets.h"
#include "picod_gpio.h"

const char *picod_gpio_direction (int device, int count,
                                  int gpio[], int dir[], int level[],
                                  PicodResponse *response, void *data) {

    uint8_t *request = picod_packet_add_request (device, 12, 16, response, data);
    if (!request) return "No space or invalid device";

    int maskgpio = 0;
    int maskdirection = 0;
    int masklevel = 0;
    while (count-- > 0) {
        int pinmask = (1 << gpio[count]);
        maskgpio |= pinmask;
        if (dir[count]) maskdirection |= pinmask;
        if (level[count]) masklevel |= pinmask;
    }

    picod_pack32 (request+4, maskgpio);
    picod_pack32 (request+8, maskdirection);
    picod_pack32 (request+12, masklevel);
    return 0;
}

const char *picod_gpio_pull (int device, int count,
                             int *gpio, int *pullup, int *pulldown,
                             PicodResponse *response, void *data) {

    if (!gpio) {
        uint8_t *request =
            picod_packet_add_request (device, 21, 4, response, data);
        if (!request) return "No space or invalid device";
        return 0;
    }

    uint8_t *request =
        picod_packet_add_request (device, 20, 16, response, data);
    if (!request) return "No space or invalid device";

    int maskgpio = 0;
    int mask0_15 = 0;
    int mask16_31 = 0;
    while (count-- > 0) {
        int pin = gpio[count];
        maskgpio |= 1 << pin;
        int offset = 2 * pin;
        if (pin < 16) {
            if (pullup && pullup[count]) mask0_15 |= 2 << offset;
            if (pulldown && pulldown[count]) mask0_15 |= 1 << offset;
        } else {
            if (pullup && pullup[count]) mask16_31 |= 2 << offset;
            if (pulldown && pulldown[count]) mask16_31 |= 1 << offset;
        }
    }

    picod_pack32 (request+4, maskgpio);
    picod_pack32 (request+8, mask0_15);
    picod_pack32 (request+12, mask16_31);
    return 0;
}

void picod_gpio_pull_up (const uint8_t *response, int pullup[]) {

    if (!response) return;
    uint32_t mask0_15 = picod_unpack32 (response+5);
    uint32_t mask16_31 = picod_unpack32 (response+9);

    int i;
    for (i = 0; i < 16; ++i) {
        int offset = 1 + (2 * i);
        pullup[i] = (mask0_15 >> offset) & 1;
        pullup[16+i] = (mask16_31 >> offset) & 1;
    }
}

void picod_gpio_pull_down (const uint8_t *response, int pulldown[]) {

    if (!response) return;
    uint32_t mask0_15 = picod_unpack32 (response+5);
    uint32_t mask16_31 = picod_unpack32 (response+9);

    int i;
    for (i = 0; i < 16; ++i) {
        int offset = 2 * i;
        pulldown[i] = (mask0_15 >> offset) & 1;
        pulldown[16+i] = (mask16_31 >> offset) & 1;
    }
}

const char *picod_gpio_read (int device, PicodResponse *response, void *data) {
    if (!picod_packet_add_request (device, 13, 4, response, data))
        return "No space or invalid device";
    return 0;
}

void picod_gpio_read_levels (const uint8_t *response, int *levels) {

    if (!response) return;
    uint32_t levelmask = picod_unpack32 (response+5);

    int i;
    for (i = 0; i < 30; ++i) {
        levels[i] = (levelmask >> i) & 1;
    }
}

const char *picod_gpio_write (int device,
                              int count, int gpio[], int level[],
                              PicodResponse *response, void *data) {

    uint8_t *request =
                 picod_packet_add_request (device, 14, 12, response, data);
    if (!request) return "No space or invalid device";

    int maskgpio = 0;
    int masklevel = 0;
    while (count-- > 0) {
        int pinmask = (1 << gpio[count]);
        maskgpio |= pinmask;
        if (level[count]) masklevel |= pinmask;
    }

    picod_pack32 (request+4, maskgpio);
    picod_pack32 (request+8, masklevel);
    return 0;
}

const char *picod_gpio_debounce (int device, int gpio, int timer,
                                 PicodResponse *response, void *data) {

    uint8_t *request =
                 picod_packet_add_request (device, 30, 9, response, data);
    if (!request) return "No space or invalid device";

    request[4] = (uint8_t) gpio;
    picod_pack32 (request+5, timer);
    return 0;
}

const char *picod_gpio_alert (int device, int count, int gpio[],
                              PicodResponse *response,
                              PicodResponse *unsollicited, void *data) {

    uint8_t *request =
                 picod_packet_add_request (device, 32, 12, response, data);
    if (!request) return "No space or invalid device";

    int maskgpio = 0;
    while (count-- > 0) {
        maskgpio |= (1 << gpio[count]);
    }

    if (unsollicited)
        picod_packet_register (device, 0xfb, unsollicited, data);

    picod_pack32 (request+4, maskgpio);
    picod_pack32 (request+8, unsollicited?maskgpio:0);
    return 0;
}

int picod_gpio_alert_count (const uint8_t *response) {
    return (picod_unpack16 (response) - 4) / 8;
}

int picod_gpio_alert_is_watchdog (int i, const uint8_t *response) {
    int offset = 4 + (i * 8) + 4;
    return picod_unpack32 (response+offset) & 0x10000000;
}

uint32_t picod_gpio_alert_timestamp (int i, const uint8_t *response) {
    int offset = 4 + (i * 8);
    return picod_unpack32 (response+offset);
}

uint32_t picod_gpio_alert_levels (int i, const uint8_t *response) {
    int offset = 4 + (i * 8) + 4;
    return picod_unpack32 (response+offset) & 0x7fffffff;
}

