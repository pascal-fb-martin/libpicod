# A C Client library for picod

## Overview

This is a small library for Linux that detects and communicates with one or more RaspBerry Pi Pico devices that are running [picod](https://github.com/joan2937/picod). The RPi Pico devices must be attached to the local computer using USB.

The detection of the RPi Pico devices relies a device naming convention implemented by a `udev` rule.

The current implementation handles up to 16 RPi Pico devices simultaneously.

## Installation.

Clone this repository, run `make rebuild` and then `sudo make install`.

You can now plug a RPi Pico device (with picod installed) to a USB port and test that the installation was successful by typing `picodcmd list`.

## Command line tool for test and experimentation

The library comes with a test application that doubles as an example: `picodcmd`:

```
   picodcmd [-t] [command..]
```

With no command, picodcmd starts in interactive mode. Otherwise it executes the provided command and exits.

Type `picodcmd help` for a list of commands.

The library can be removed: run `sudo make uninstall`. This also removes the `udev` rules.

## API

The libpicod API is asynchronous. It is assumed that the application will implement an event loop based on `select()` or `poll()`.

> [!NOTE]
> The original objective of this library is to implement a web service that provides access to the RPi Pico GPIO pins. That web service must simultanously handle multiple RPi Pico devices and concurrent HTTP requests.

The libpicod library is modular: a core module implements the detection of, and communication with, the RPi Pico devices; extension modules implement the access to each picod feature. At this time an extension module is provided for the direct GPIO access. Future extension modules will give access to the PWM, UART and SPI features.

> [!NOTE]
> The motivation for this modular implementation is a subsequent DCC project. This idea is to create alternative RPi Pico firmwares that implement specialized features in a way that is compatible with the picod protocol.

### Core API

Using the libpicod API requires including file `picod.h`.

```
const char *picod_initialize (int argc, char **argv);
```

Initialize the library. The supported command line options are:

* `-pico=STRING`: Provide an explicit path to a RPi Pico device (typically in `/dev`).

The -pico option can be repeated, allowing support of multiple RPi Pico devices simultaneously. Using the -device option disables auto discovery of RPi Pico devices. That option can be used when the udev naming convention cannot be relied on.

This returns a null string on success, a pointer to an error message on failure.

```
typedef void PicodListener (void);
void picod_listen (PicodListener *listener);
```

Listen to changes in the list of detected RPi Pico devices. The listener callback will be invoked each time a new RPi Pico device has been identified, or when a known RPi Pico device has disappeared.

This monitoring is optional. It can be used to update a list of RPi Pico devices asynchronously. An alternative is to walk the list of devices periodically.

A RPi Pico device is identified when its UID has been acquired.

The listener can use `picod_count()` and `picod_uid()` to retrieve the current list of devices:

```
      int count = picod_count();
      while (count-- > 0) {
          uint8_t *uid = picod_uid (count);
          if (!uid) continue;
          .. application logic here ..
      }
```

A RPi Pico device may be listed and not have an associated UID yet: ignore any entry returning a null UID. Either the UID will be acquired later or this RPi Pico device is probably not running picod.

> [!NOTE]
> This listener will be invoked even when discovery is disabled (through the `-pico` option): in that mode the library still detects disconnection and reconnection events.

Calling picod_listen() with a null parameter disables the listening.

```
typedef void PicodTap (const char *action, const char *device,
                       const uint8_t *data, int length);
void picod_spy (PicodTap *tap);
```

Install a tap to listen to packets sent and received. This is more of a debug feature. The tap function is also called when a CRC error is detected.

```
void picod_count (void);
int  picod_descriptor (int index);
int  picod_busy (int index);
void picod_receive (int fd);
int  picod_transmit (int fd);
```

These five function are typically used in a `select()` or `poll()` loop. The list of Pico devices may change from time to time: walk through the list each time and ignore any returned descriptor that is 0 or less. Function `picod_count()` returns the total number of Pico devices known at that time. Some RPi Pico devices may has been disconnected, or may have not been identified yet.

Function `picod_descriptor()` returns the file descriptor associated with the specified RPi Pico device, or -1 if the device is not accessible. The file descriptor for a specific RPi Pico device should not be monitored for writing if `picod_busy()` return 0, i.e. there is no data to send.

Function `picod_transmit()` should be called when the file descriptor is ready for writing. It returns 0 when there is no more data to send.

Function `picod_receive()` should be called when the file descriptor is ready for reading. It reads and processes picod messages from the RPi Pico device.

Example using `select()`:

```
   int fdmax = 0;
   fd_set reads;
   fd_set writes;
   ...
   int count = picod_count();
   while (count-- > 0) {
       int fd = picod_descriptor (count);
       if (fd <= 0) continue;
       if (fdmax < fd) fdmax = fd;
       FD_SET (fd, &reads);
       if (picod_busy (count)) FD_SET (fd, &writes);
   }
   ...
   select (fdmax+1, &reads, &writes, 0, 0);
   count = picod_count();
   while (count-- > 0) {
       int fd = picod_descriptor (count);
       if (fd <= 0) continue;
       if (FD_ISSET (fd, &reads)) picod_receive (fd);
       if (FD_ISSET (fd, &writes)) picod_transmit (fd);
   }
```

```
const unsigned char *picod_uid (int index);
```

Return the UID of the RPi Pico device, or null when the UID has not been acquired yet.

```
const char *picod_tty (int index);
```

Return the full path to the UNIX device used to access the RPi Pico device.

```
int picod_signature (const uint8_t *key, int length);
```

This is a helper function that can be used to build hash indexes using a UID.

```
int picod_version (int index);
```

Return the version of the firmware running on the specified device, or 0 if that version is not known yet.

```
int picod_start (const unsigned char *uid);
```

Prepare a new (empty) packet context for the specified RPi Pico device.

Return the current RPi Pico device index given the RPi Pico UID, or -1 when no RPi Pico with the specified UID can be found.

The index value for the same RPi Pico device may change after waiting for I/O.

```
const char *picod_submit (int index);
```

Send all commands accumulated for the specified RPi Pico device. Return null on success, an error message on failure.

The application will typically call `picod_start()`, then one or more picod command functions, then `picod_submit()` to transmit the requests all at once:

```
   int index = picod_start (uid);
   if (index >= 0) {
       .. add commands here ..
       picod_submit (index);
   }
```

The picod command functions are implemented in separate modules.

```
const char *picod_reset (const uint8_t *uid);
```

Send a reset command to the specified RPi Pico device. No response is expected. Return null on success, an error message on failure.

```
void picod_background (int now);
```

This function must be called periodically, typically once a second. This covers continuous discovery of RPi Pico devices.

### Internal Core API

The following API is for internal purpose only. This is in support of request encoding and decoding. The code must include file `picod_packets.h`.

```
void picod_pack16 (uint8_t *buffer, int value);
void picod_pack32 (uint8_t *buffer, int value);
int  picod_unpack16 (const uint8_t *buffer);
int  picod_unpack32 (const uint8_t *buffer);
```

Pack and unpack multibyte values using the endianess expected by picod.

```
typedef void PicodResponse (int index, void *data,
                            int status, const uint8_t *response);

uint8_t *picod_packet_add_request (int index,
                                   uint8_t code, int size,
                                   PicodResponse *response, void *data);
```

Reserve room for one more request in the transmission buffer. This returns a pointer to the packet in the buffer. This function is normally called from an API function that encodes a picod request.

The `response` function is typically the application level function that will handle the response. The `data` parameter is meant to propagate a client context.

```
void picod_packet_register (int index,
                            int code, PicodResponse *response, void *data);
```

Register a response callback to an arbitrary request code. This is meant for unsollicited messages.

### GPIO API

Most functions here take response and data callback parameters. These are meant to allow the caller to process the picod response asynchronously. Set the response callback to null (0) if no response is needed. The `response` parameter is the callback function to be called, the `data` parameter is a caller context that will be provided when the callback is fired.

No request is sent until `picod_submit()` is called.

```
const char *picod_gpio_direction (int device, int count,
                                  int gpio[], int dir[], int level[],
                                  PicodResponse *response, void *data);
```

Build a request to set GPIO pins direction and initial level on the specified RPi Pico device. The `gpio` array provides the GPIO pin numbers. The `dir` array indicates the direction for each pin, either input (0) or output (1). The `level` array provides the initial state for the pin, either low (0) or high (1).

```
const char *picod_gpio_pull (int device, int count,
                             int *gpio, int *pullup, int *pulldown,
                             PicodResponse *response, void *data);
```

Build a request to enable the GPIO pull up and down on the specified RPi Pico device.

The `gpio` array provides the list of GPIO pins to configure.

If the `gpio` array is null, the request is to get the current configuration and the `pullup`, `pulldown` parameters are ignored. The `response` function can call `picod_gpio_pull_up()` and `picod_gpio_pull_down()` to retrieve the pull resistor configuration.

The `pullup` array indicate if the pull-up resistor must be enabled

The `pulldown` array indicates if the pull-down resistor must be enabled.

If either `pullup` or `pulldown` is null, the corresponding pull resistor is never enabled, equivalent to have the array filled with 0.

```
void picod_gpio_pull_up (const uint8_t *response, int pullup[]);
```

Decode the current state of the pull-up resistors from the response to a get request (see above). The `pullup` array is indexed by the GPIO pin number and must have 30 elements.

```
void picod_gpio_pull_down (const uint8_t *response, int pulldown[]);
```

Decode the current state of the pull-down resistors from the response to a get request (see above). The `pulldown` array is indexed by the GPIO pin number and must have 30 elements.

```
const char *picod_gpio_read (int device,
                             PicodResponse *response, void *data);
```

Build a GPIO read request.

```
picod_gpio_read_levels (const uint8_t *response, int *levels);
```

Retrieve the GPIO pins state after a response from the RPi Pico device has been received. The `level` array is indexed by the GPIO pin number and must have 30 elements or more.

```
const char *picod_gpio_write (int device,
                              int count, int gpio[], int level[],
                              PicodResponse *response, void *data);
```

Build a GPIO write request.

```
const char *picod_gpio_debounce (int device, int gpio, int timer,
                                 PicodResponse *response, void *data);
```

Enable a debounce timer on the specified GPIO pin. The debounce timer is a filter that block short timed pulses (bounces) that are often caused by transient contacts when a switch blade moves. The debounce timer is disabled if `timer` is 0.

```
const char *picod_gpio_alert (int device, int count, int gpio[],
                              PicodResponse *response,
                              PicodResponse *unsollicited, void *data);
```

Register for (unsollicted != 0), or deregister from (unsollicted == 0), alerts on the selected GPIO pins.

The unsollicited callback will be called each time an unsollicited message is received from the RPi Pico device. This is a sticky callback: it can be replaced but it cannot be removed. That should not matter much: if all alerts are disabled, the picod unsollicited messages will stop anyway.

```
int picod_gpio_alert_count (const uint8_t *response);
int      picod_gpio_alert_watchdog  (int i, const uint8_t *response);
uint32_t picod_gpio_alert_timestamp (int i, const uint8_t *response);
uint32_t picod_gpio_alert_levels    (int i, const uint8_t *response);

```
Decode the unsollicted alert message, one report at a time.

There are two types of reports:

- Watchdog reports indicate pins without changes for a specified time.
- Other reports when states did change.

> [!WARNING]
> The value returned by picod_gpio_alert_timestamp() is the raw RPi Pico tick, not synchronized to the local time. Basic rule is to take the local time when the response was received, consider that it matches the tick of the last report and then evaluate the time of previous report items in this message relative to the last.

