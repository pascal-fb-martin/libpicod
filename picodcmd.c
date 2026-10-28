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
 * picodcmd.c - A Picod C client test application.
 *
 * SYNOPSYS:
 *
 * picodcmd
 * picodcmd <command>..
 *
 * The first form takes commands from the standard input. If the input is
 * from a character device (tty) a prompt is displayed.
 *
 * The second form takes all parameters as making up a single command.
 *
 * The list of supported commands is described in string HelpText below.
 *
 * When multiple commands are submitted, the application waits for the
 * Pico response, if any is expected, before evaluating the next command.
 *
 * Don't use tabs, only space characters as separator. Any control character
 * is the end of the command.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "picod.h"
#include "picod_gpio.h"

static int ShowPrompt = 0;
static const char PicodPromptText[] = "Pico$ ";

static const char *HelpText =
    "  help                Show the list of commands.\n"
    "  list                Show the list of RPi Pico board detected.\n"
    "  trace [on|off]      Enable or disable packet traces.\n"
    "  reset               Reset all detected boards.\n"
    "  reset i             Reset the board at index i.\n"
    "  input i pin=0|1..   Set the listed GPIO pins as input.\n"
    "  output i pin=0|1..  Set the listed GPIO pins as output.\n"
    "  pull i              Get the current pullup and pulldown configuration\n"
    "  pull i pin=1|2|3..  Set the pullup (2 or 3) and/or pulldown (1 or 3)\n"
    "  gpio i              Read all GPIO pins.\n"
    "  gpio i pin=0|1..    Set the specified GPIO pins to 0 or 1.\n"
    "  debounce i pin t    Set a debounce timer on the specified GPIO pin.\n"
    "  alert i pin..       Enable alerts on the specified GPIO pins.\n"
    "  silence i pin..     Disable alerts on the specified GPIO pins.\n";

// What to wait for before accpting the next command.
// Two distinct variables are used because there are two independent conditions
static int WaitResponse = -1; // Wait for a response from the specified device
static int WaitDetected = -1; // Wait for a detection of the specified device

static void picodPrompt (void) {

    if (!ShowPrompt) return;
    printf (PicodPromptText);
    fflush (stdout);
}

static void tap (const char *action, const char *device,
                 const uint8_t *data, int length) {

    int prompt = (ShowPrompt && (WaitResponse < 0));

    printf ("%s%s: %s", prompt?"\n":"", device, action);
    while (length-- > 0) {
        printf (" %02x", *data++);
    }
    if (prompt) {
        printf ("\n%s", PicodPromptText);
        fflush (stdout);
    } else {
        printf ("\n");
    }
}

static int decodepins (int argc, const char **argv, int *pins, int *values) {

    int i;
    for (i = 0; i < argc; ++i) {
        pins[i] = atoi (argv[i]);
        if ((pins[i] < 0) || (pins[i] > 29)) return -1;
        const char *sep = strchr (argv[i], '=');
        if (values) values[i] = sep?atoi(sep+1):0;
    }
    return argc;
}

static void statusresponse (int index, void *data,
                            int status, const uint8_t *response) {

    const char *command = data;
    if (status) {
        printf ("%s: Pico device %d returned error code %d\n",
                command, index, status);
    } else {
        printf ("%s: Pico device %d returned OK\n", command, index);
    }
    if (WaitResponse == index) WaitResponse = -1;
}

static void readresponse (int index, void *data,
                          int status, const uint8_t *response) {

    if (status) {
        statusresponse (index, "GPIO Read", status, response);
        return;
    }

    int levels[30];
    picod_gpio_read_levels (response, levels);

    printf ("GPIO  State\n");
    int i;
    for (i = 0; i < 30; ++i) {
        printf ("%2d    %d\n", i, levels[i]);
    }

    if (WaitResponse == index) WaitResponse = -1;
}

static void alertresponse (int index, void *data,
                           int status, const uint8_t *response) {

    if (status) {
        statusresponse (index, "ALERT Command", status, response);
        return;
    }
    struct timeval now;
    gettimeofday (&now, 0);

    int count = picod_gpio_alert_count (response);
    if (count <= 0) return;
    uint32_t base = picod_gpio_alert_timestamp (count-1, response);

    int prompt = (ShowPrompt && (WaitResponse < 0));

    printf (prompt?"\n":"");

    int i;
    for (i = 0; i < count; ++i) {
        uint32_t tick = picod_gpio_alert_timestamp (i, response);
        uint32_t delta = base - tick; // Play games with overflow rules
        struct timeval timestamp = now;
        if (delta) {
            while (delta > timestamp.tv_usec) {
                timestamp.tv_sec -= 1;
                delta += 1000000;
            }
            timestamp.tv_usec -= delta;
        }
        printf ("GPIO Alert #%d at %lld.%03lld: %08x\n",
                i, (long long)(timestamp.tv_sec),
                   (long long)(timestamp.tv_usec / 1000),
                   picod_gpio_alert_levels (i, response));
    }
    if (prompt) {
        printf (PicodPromptText);
        fflush (stdout);
    }
}

static void pullresponse (int index, void *data,
                          int status, const uint8_t *response) {

    if (status) {
        statusresponse (index, "PULL Get", status, response);
        return;
    }

    int pullup[30];
    int pulldown[30];
    picod_gpio_pull_up (response, pullup);
    picod_gpio_pull_down (response, pulldown);

    printf ("GPIO  Pull-Up   Pull-Down\n");
    int i;
    for (i = 0; i < 30; ++i) {
        printf ("%2d    %-8.8s  %-8.8s\n", i,
                pullup[i]?"enabled":"disabled", pulldown[i]?"enabled":"disabled");
    }
    if (WaitResponse == index) WaitResponse = -1;
}

static const char *setdirection (int device,
                                 int argc, const char **argv, int dir) {

    if (argc < 3) return "Missing GPIO pins";

    int pins[30];
    int dirs[30];
    int values[30];
    int count = decodepins (argc-2, argv+2, pins, values);

    int i;
    for (i = 0; i < count; ++i) dirs[i] = dir;

    return picod_gpio_direction (device,
                                 count, pins, dirs, values,
                                 statusresponse, "GPIO Set Direction");
}

static void picodList (void) {

    printf ("Index  UID               VERSION   PATH\n");
    int count = picod_count ();
    while (count-- > 0) {
        const uint8_t *uid = picod_uid (count);
        if (!uid) continue;
        printf ("%3d    %02x%02x%02x%02x%02x%02x%02x%02x  %08x  %s\n",
                count, uid[0], uid[1], uid[2], uid[3],
                       uid[4], uid[5], uid[6], uid[7],
                       picod_version(count), picod_tty(count));
    }
}

static void picodDetection (void) {

    if (ShowPrompt) {
        if (WaitResponse < 0) printf ("\n");
        printf ("Pico device list changed:\n");
        picodList ();
        if (WaitResponse < 0) picodPrompt ();
    } else {
        int count = picod_count ();
        while (count-- > 0) {
            const uint8_t *uid = picod_uid (count);
            if (!uid) continue;
            if (count == WaitDetected) WaitDetected = -1;
        }
    }
}

static const char *picodPrepare (int argc, const char **argv) {

    if (argc < 1) return 0; // empty command.

    WaitResponse = -1;

    // This first set of commands either does not talk to a Pico device
    // or does not need to wait for an answer.

    if (!strcasecmp (argv[0], "list")) {

        picodList ();
        return 0;

    } else if (!strcasecmp (argv[0], "reset")) {

        if (argc < 2) {
            int count = picod_count ();
            while (count-- > 0) {
                const uint8_t *uid = picod_uid (count);
                if (uid) picod_reset (uid);
            }
        } else {
            int i;
            for (i = 1; i < argc; ++i) {
                int index = atoi (argv[i]);
                if (index >= 0) {
                    const uint8_t *uid = picod_uid (index);
                    if (uid) picod_reset (uid);
                }
            }
        }
        return 0;

    } else if (!strcasecmp (argv[0], "trace")) {

        if (argc > 1) {
            if (!strcasecmp (argv[1], "off")) picod_spy (0);
            if (!strcasecmp (argv[1], "on")) picod_spy (tap);
        } else {
            picod_spy (tap);
        }
        return 0;

    } else if (!strcasecmp (argv[0], "listen")) {

        if (argc > 1) {
            if (!strcasecmp (argv[1], "off")) picod_listen (0);
            if (!strcasecmp (argv[1], "on")) picod_listen (picodDetection);
        } else {
            picod_listen (picodDetection);
        }
        return 0;

    } else if (!strcasecmp (argv[0], "exit")) {

        exit (0);

    } else if (!strcasecmp (argv[0], "help")) {

        printf ("%s", HelpText);
        return 0;
    }

    // The subsequent commands must specify a Pico device (by index) and
    // prepare a command to submit to this device.

    if (argc < 2) return "Missing Pico device index";
    WaitResponse = atoi (argv[1]);

    const uint8_t *uid = picod_uid (WaitResponse);
    if (!uid) return "invalid Pico device";

    picod_start (uid);

    if (!strcasecmp (argv[0], "input")) {

        return setdirection (WaitResponse, argc, argv, 0);

    } else if (!strcasecmp (argv[0], "output")) {

        return setdirection (WaitResponse, argc, argv, 1);

    } else if (!strcasecmp (argv[0], "debounce")) {

        if (argc < 4) return "missing timer parameter";

        return picod_gpio_debounce (WaitResponse, atoi(argv[2]), atoi(argv[3]),
                                    statusresponse, "GPIO Debounce");

    } else if (!strcasecmp (argv[0], "pull")) {

        if (argc < 3)
            return picod_gpio_pull (WaitResponse, 0, 0, 0, 0, pullresponse, 0);

        int pins[30];
        int values[30];
        int pullup[30];
        int pulldown[30];

        int count = decodepins (argc-2, argv+2, pins, values);

        int i;
        for (i = 0; i < count; ++i) {
            pullup[i] = (values[i] & 2) >> 1;
            pulldown[i] = (values[i] & 1);
        }

        return picod_gpio_pull (WaitResponse, count,
                                pins, pullup, pulldown, statusresponse, "Set Pull");

    } else if (!strcasecmp (argv[0], "gpio")) {

        if (argc < 3) return picod_gpio_read (WaitResponse, readresponse, 0);

        int pins[30];
        int values[30];
        int count = decodepins (argc-2, argv+2, pins, values);

        if (count <= 0) return "invalid list of GPIO pins";

        return picod_gpio_write (WaitResponse, count, pins, values,
                                 statusresponse, "GPIO Write");

    } else if (!strcasecmp (argv[0], "alert")) {

        if (argc < 3) return "missing GPIO parameter";

        int pins[30];
        int count = decodepins (argc-2, argv+2, pins, 0);

        return picod_gpio_alert (WaitResponse, count, pins,
                                 statusresponse, alertresponse, "GPIO Alert");

    } else if (!strcasecmp (argv[0], "silence")) {

        if (argc < 3) return "missing GPIO parameter";

        int pins[30];
        int count = decodepins (argc-2, argv+2, pins, 0);

        return picod_gpio_alert (WaitResponse, count, pins,
                                 statusresponse, 0, "GPIO Silence");
    }

    WaitResponse = -1;
    return "invalid command";
}

static void picodWaitDevice (int *device) {

    do {
        int max = 0;
        int busy = 0;

        fd_set reads;
        fd_set writes;
        fd_set except;

        FD_ZERO (&reads);
        FD_ZERO (&writes);
        FD_ZERO (&except);

        int count = picod_count();
        while (count-- > 0) {
            int fd = picod_descriptor (count);
            if (fd <= 0) continue;
            FD_SET (fd, &reads);
            if (picod_busy (count)) {
                busy = 1;
                FD_SET (fd, &writes);
            }
            FD_SET (fd, &except);
            if (fd > max) max = fd;
        }

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        select (max+1, &reads, busy?&writes:0, &except, &timeout);

        count = picod_count();
        while (count-- > 0) {
            int fd = picod_descriptor (count);
            if (fd <= 0) continue;
            if (FD_ISSET (fd, &reads) || FD_ISSET (fd, &except))
                picod_receive (fd);
            if (busy && FD_ISSET (fd, &writes))
                picod_transmit (fd);
        }
        picod_background (time(0));
    } while (*device >= 0);
}

static void picodExecute (int argc, const char **argv) {

    const char *error = picodPrepare (argc, argv);
    if (error) {
        printf ("Error: %s\n", error);
        return;
    }
    if (WaitResponse < 0) return;

    picod_submit (WaitResponse);
    picodWaitDevice (&WaitResponse);
}

static void picodParse (char *cmd) {

    // Split the command into arguments
    const char *arguments[34];
    int i = 0;
    int count = 1;
    arguments[0] = cmd;
    while (cmd[i] >= ' ') {
        if (cmd[i] == ' ') {
            cmd[i] = 0;
            while (cmd[++i] == ' ');
            arguments[count++] = cmd + i;
            if (count >= 34) {
                printf ("Too many parameters\n");
                return;
            }
        }
        i += 1;
    }
    cmd[i] = 0; // Eliminate end of line characters.
    picodExecute (count, arguments);
}

static void picodWaitInput (int tty) {

    time_t start = time(0) + 1;

    for (;;) {
        int max = 0;
        fd_set reads;
        fd_set writes;
        fd_set except;
        int busy = 0;

        FD_ZERO (&reads);
        FD_ZERO (&writes);
        FD_ZERO (&except);

        int count = picod_count();
        while (count-- > 0) {
            int fd = picod_descriptor (count);
            if (fd <= 0) continue;
            FD_SET (fd, &reads);
            FD_SET (fd, &except);
            if (fd > max) max = fd;
        }
        if (tty) FD_SET (0, &reads);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        select (max+1, &reads, 0, &except, &timeout);

        if (tty && FD_ISSET (0, &reads)) break;
        if ((!tty) && (time(0) > start)) break;

        count = picod_count();
        while (count-- > 0) {
            int fd = picod_descriptor (count);
            if (fd <= 0) continue;
            if (FD_ISSET (fd, &reads) || FD_ISSET (fd, &except))
                picod_receive (fd);
            if (busy && FD_ISSET (fd, &writes))
                picod_transmit (fd);
        }
        picod_background (time(0));
    }
}

static void picodDetectAndExit (void) {

    int count = picod_count ();
    if (count <= 0) return;

    while (count-- > 0) {
        if (!picod_uid(count)) return;
    }

    // Now all Pico devices present have been identified.
    picodList ();
    exit (0);
}

int main (int argc, const char **argv) {

    if ((argc >= 2) && (!strcmp (argv[1], "-t"))) {
        picod_spy (tap);
        argc -= 1;
        argv += 1;
    }

    const char *error = picod_initialize (argc, argv);
    if (error) {
        printf ("%s\n", error);
        exit (1);
    }

    if (argc < 2) {
        struct stat entry;
        if (!fstat (0, &entry)) {
            if (S_ISCHR(entry.st_mode)) ShowPrompt = 1;
        }
        for (;;) {
            char buffer[160];
            picodPrompt ();
            picodWaitInput (1);
            if (!fgets (buffer, sizeof(buffer), stdin)) exit (0);
            if ((buffer[0] > ' ') && (buffer[0] != '#')) picodParse (buffer);
        }
    }

    if (argc > 2) {
        picod_listen (picodDetection);
        WaitDetected = atoi(argv[2]);
        picodWaitDevice (&WaitDetected);
    } else if (!strcasecmp (argv[1], "list")) {
        picod_listen (picodDetectAndExit);
        picodWaitInput (0);
    }
    picodExecute (argc-1, argv+1);
}

