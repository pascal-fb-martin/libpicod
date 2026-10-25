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
 * picod_gpio.h - The module that manages picod GPIO requests.
 */

const char *picod_gpio_direction (int device, int count,
                                  int gpio[], int dir[], int level[],
                                  PicodResponse *response, void *data);

const char *picod_gpio_pull (int device, int count,
                             int *gpio, int *pullup, int *pulldown,
                             PicodResponse *response, void *data);

void picod_gpio_pull_up (const uint8_t *response, int pullup[]);
void picod_gpio_pull_down (const uint8_t *response, int pulldown[]);

const char *picod_gpio_read (int device,
                             PicodResponse *response, void *data);

void picod_gpio_read_levels (const uint8_t *response, int *levels);

const char *picod_gpio_write (int device,
                              int count, int gpio[], int level[],
                              PicodResponse *response, void *data);

const char *picod_gpio_debounce (int device, int gpio, int timer,
                                 PicodResponse *response, void *data);

const char *picod_gpio_alert (int device, int count, int gpio[],
                              PicodResponse *response,
                              PicodResponse *unsollicited, void *data);

int picod_gpio_alert_count (const uint8_t *response);
int      picod_gpio_alert_watchdog  (int i, const uint8_t *response);
uint32_t picod_gpio_alert_timestamp (int i, const uint8_t *response);
uint32_t picod_gpio_alert_levels    (int i, const uint8_t *response);

