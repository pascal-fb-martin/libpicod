# The picod Protocol

## Overview

> [!NOTE]
> This document is the result of a reverse engineering of the picod code version0x600.

Picod allows access to the Pico GPIO pins 0 to 29 from the computer that the Pico is attached to. The USB attachment appears as a serial port (typically /dev/ttyACMO0).

Picod commands are formatted as two layers: a packet envelop contains a sequence of commands or a response.

All data is most significant byte (MSB) first.

## Packet Envelop layer

A packet is made of three parts: a packet header followed by the payload data and then a packet CRC at the end.

### Packet Format

A packet header includes its own CRC, which purpose is to secure the length field within a fixed size structure, so that there is no risk of using a corrupted length value when jumping to the final CRC.

> [!NOTE]
> I find the overall picod protocol simple, elegant and sound, but some design choices puzzle me. Using a special preamble character is frequent in serial communications, but I wonder about its purpose here. Interferences on a RS232 wire often lead to a burst of 0xFF characters, so the choice of value is odd (but oddly common, too). The picod protocol is binary and 0xFF is a valid data content (for example in CRCs, masks or GPIO data): this is not a very reliable synchronization mechanism. Serial protocols that use a preamble character typically do escape that value in packets, but there is no escape mechanism here.

| Offset  | Size  | Description     |
| :------ | ----- | :-------------- |
| 0 | 1 | Preamble (0xFF) |
| 1 | 2 | Length |
| 3 | 2 | CRC 1 (preamble & length) |
| 5 | N | Request(s) or response |
| N+5 | 2 | CRC 2 (whole packet) |

## Requests

### General Data Format

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 0 | 2 | Length |
| 2 | 1 | Flags |
| 3 | 1 | Command code |
| 4 | N | Optional parameters |

### Flags

| Bits | Description    |
| :--- | :------------- |
| 0-5  | Transaction ID |
| 6-7  | Reply |

If Reply is 0, the Pico will not respond, otherwise it immediately responds.

(The Pico response follows the same general format, with the Flags and Command Code fields repeating the request's -- see section Responses.)

### GPIO Open (code 10)

Open a list of GPIO pins. The one parameter is a 32 bit mask that indicates which pins to open (bit set to 1). Opening a pin sets it in GPIO mode (vs. I2C or SPI).

> Question: why does this mark the pin as GPIO but does not call gpio_set_function(), i.e. it does not set the actual hardware mode? (gpio_set_function() is called through the GPIO Set Function command.)

If some pins are currently used in a different mode, status "GPIO is use" will be returned. Other requested pins will be set to GPIO mode regardless.

### GPIO Close (code 11)

Close a list of GPIO pins. The one parameter is a 32 bit mask that indicates which pins to close. The listed GPIO pins will be marked as free, regardless of their current mode..

### GPIO Set Direction (code 12)

This command takes 3 parameters:

- A mask of pins to configure (32 bit).
- A mask of directions (32 bit). Bit set to 1 means output, 0 means input.
- A mask of levels (32 bit). This sets the state of the output pins.

All listed pins will be set to GPIO mode if currently free (i.e. it also performs a GPIO Open command).

The response contains no data.

### GPIO Read (code 13)

Read the current level of all GPIO pins. The response contains a 32 bit mask, one bit per GPIO.

### GPIO Write (code 14)

Write to the listed GPIO pins. The first parameter is a 32 bit mask that indicates which pins to set. The second parameter is a 32 bit mask that provides the values to set the pins to.

This request implicitly set the GPIO pin to output before the state changes.

The response contains no data.

### GPIO Set Pulls (code 20)

Enable or disable the internal pull up and pull down resistors for the specified GPIO pins. This command takes three parameters:

- A 32 bit mask that indicates which pins to configure.
- A 32 bit mask for GPIO 0 to 15, organized as sets of two bits: in each set, bit 0 enables (1) or disables (0) the pull down resistor, while bit 1 enables (1) or disables (0) the pull up resistor.
- The third parameter is a similar 32 bit mask for GPIO 16 to 31.

### GPIO Get Pulls (code 21)

Return the current pull up and pull down configuration for all GPIO pins. The response contains two 32 bit masks, the first for GPIO pins 0 to 15 and the second for GPIO pins 16 to 31. See the GPIO Set Pulls command for a description of these masks.

### GPIO Set Function (code 25)

Set the function for all listed GPIO pins. The first parameter is a 32 bit mask that indicates which pins to set the mode for. The following four parameters are 32 bit set of 4 bit fields, where each 4 bit field represent the function mode of one GPIO pin:

| Value  | Function |
| :----- | :------- |
| 1 | SPI mode |
| 2 | UART mode |
| 3 | I2C mode |
| 4 | PWM mode |
| 5 | SIO mode, i.e. standard GPIO |

### GPIO Get Function (code 26)

Return the current function mode for all GPIO pins. See GPIO Set Function for a description of the format of the 4 32 bit values returned.

### Alert Debounce (code 30)

Set the debounce timer. The first parameter (8 bit) is the GPIO pin number, the second parameter (32 bit) is the debounce timer value in microseconds. If the second parameter is 0, the debounce function is disabled.

### Alert Watchdog (code 31)

Set the watchdog timer. The first parameter (8 bit) is the GPIO pin number, the second parameter (32 bit) is the watchdog timer value in microseconds. If the second parameter is 0, the watchdog function is disabled.

### Alert Select (code 32)

Enable or disable alert on a set of GPIO pins. The first parameter (32 bit) is a mask that indicates which pins to configure. The second parameter is a 32 bit mask that enables or disables alert on each GPIO pin.

The Pico will continually monitor the pins and report any change as a GPIO level message (code 0xfb), unsollicited. The report data is a sequence of two 32 bit values: the first value is a tick value in microseconds, the second value is the mask of the pin states. The tick is only meaningful compared to a previous tick.

### Event Config (code 35)

The first parameter is a 8 bit value that represents an event type. The second parameter (8 bit) is a boolean that enable (1) or disable (0) alerts for this event type. The third parameter is a 16 bit counter value.

(Not sure that I understand what this event is or how it works..)

### ADC Read (code 40)

Read an analog value. The first parameter is a 8 bit analog channel index (0 to 3). A channel index is the GPIO pin number minus 26.

If the GPIO pin was not set to ADC function mode, it is silently initialized: there is no need for an open command.

Return the channel number (8 bit) followed by the analog value (16 bit).

### ADC Close (code 41)

Close an analog channel. The parameter is a 8 bit analog channel index. The GPIO pin is not available for other purposes.

### I2C Open (code 50)

TBD

### I2C Close (code 51)

TBD

### I2C Read (code 52)

TBD

### I2C Write (code 53)

TBD

### I2C Push (code 54)

TBD

### I2C Pop (code 55)

TBD

### PWM Read Frequency (code 60)

TBD

### PWM Read Duty (code 61)

TBD

### PWM Read Edge (code 62)

TBD

### PWM (code 63)

TBD

### Servo (code 64)

TBD

### PWM Close (code 65)

TBD

### SPI Open (code 70)

Open one of the two SPI channels. This takes 8 parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | GPIO pin number for TX, see Pico pinout |
| 6 | 1 | GPIO pin number for RX, see Pico pinout |
| 7 | 1 | GPIO pin number for SCK, see Pico pinout |
| 8 | 1 | CLK mode, bit 0: phase, bit 1: polarity |
| 9 | 1 | Number of data bits: 4 to 16 |
| 10 | 1 | GPIO pin for CS (slave mode) or 0 |
| 11 | 4 | SPI speed |

The offsets are relative to the command's first byte.

### SPI Close (code 71)

Close the specified SPI channel. The 8 bit parameter is the channel number (0 or 1).

### SPI Read (code 75)

Read from the specified channel. This takes four parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | GPIO pin number for chip select |
| 6 | 2 | Number of bytes to read |
| 8 | 1 | SPI Repeated TX data, normally 0 |

The offsets are relative to the command's first byte.

The response contains the received data.

> [!NOTE]
> This is a blocking request.

### SPI Write (code 76)

Write to the specified channel. This takes four parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | GPIO pin number for chip select |
| 6 | 2 | Number of bytes to write |
| 8 | N | TX data |

The offsets are relative to the command's first byte.

The response contains no data.

> [!NOTE]
> This is a synchronous, blocking request.

### SPI Transfer (code 77)

Write to the specified channel and then read the device's response. This takes four parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | GPIO pin number for chip select |
| 6 | 2 | Number of bytes to write and read |
| 8 | N | TX data |

The offsets are relative to the command's first byte.

The response contains the received data.

> [!NOTE]
> This is a synchronous, blocking request.

### SPI Push (code 78)

This request is for SPI channel in slave mode only.

Asynchronously push data (for a future response?) on the specified channel. This takes three parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | Number of data bytes |
| 6 | N | data |

The response contains a 8 bit count of how many bytes where successfully pushed to the local buffer.

> [!NOTE]
> Not sure on how this combines with SPI Pop.

### SPI Pop (code 79)

This request is for SPI channel in slave mode only.

Asynchronously pop data that was received on the specified channel. This takes two parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 1 | Number of data bytes to pop |

The response contains the data popped from the receive buffer.

> [!NOTE]
> Not sure on how this combines with SPI Push.

### UART Open (code 85)

Open the specified channel. This takes nine parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 4 | Serial speed (baud rate) |
| 8 | 1 | channel (0 or 1) |
| 9 | 1 | GPIO pin number for TX, see Pico pinout |
| 10 | 1 | GPIO pin number for RX, see Pico pinout |
| 11 | 1 | GPIO pin number for CTS, see Pico pinout |
| 12 | 1 | GPIO pin number for RTS, see Pico pinout |
| 13 | 1 | Number of data bits |
| 14 | 1 | Number of stop bits |
| 15 | 1 | Parity: 0=none, 1=even, 2=odd |
| 10 | 1 | GPIO pin for CS (slave mode) or 0 |
| 11 | 4 | SPI speed |

The offsets are relative to the command's first byte.

The response contains a 32 bit value that represents the actual baud rate used.

### UART Close (code 86)

Close the specified channel. The 8 bit parameter is the channel number (0 or 1).

The response contains no data.

### UART Read (code 87)

Asynchronously read from the specified channel. This takes four parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 2 | Number of bytes to read |

The offsets are relative to the command's first byte.

The response contains the received data.

### UART Write (code 88)

Write to the specified channel. This takes four parameters:

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 4 | 1 | channel (0 or 1) |
| 5 | 2 | Number of bytes to write |
| 8 | N | TX data |

The offsets are relative to the command's first byte.

The response contains no data.

> [!NOTE]
> This is a synchronous, blocking request.

### UID (code 90)

Return this board's unique identifier. The response contains the 64 bit binary UID value.

### Tick (code 94)

Read the RPi Pico's current tick value. The response contains a 32 bit value that represents the tick in microseconds.

This could be useful to calculate the local time that corresponds to tick 0. Do not expect a precise synchronization: the response time is variable (it depends on the USB activity). At best the response comes in about 1 to 2 milliseconds, but it can go as high as 20 milliseconds.

> [!CAUTION]
> Because the tick value is a 32 bit, there is a rollover about every 1h 11mn 34s.

### Sleep (95)

Sleep for the specified duration. The 32 bit parameter contains the sleep duration in microseconds.

> [!CAUTION]
> This is a blocking sleep on the RPi Pico: the microcontroller will not process any requests or sample GPIO inputs while sleeping.

### Reset Pico (code 96)

Reset all GPIO pins to their default configuration.

> [!NOTE]
> This is not a full reset of the RPi Pico microcontroller. Some values will not be reset, like the current tick.

### Set Config Value (code 97)

Set a picod configuration parameter. The first parameter is a 32 bit config ID, the 2nd parameter is a 32 bit parameter value.

At this time only config ID 0 is implemented. The value is a mask that enables specific types of debug messages:

| bit | Description    |
| :---- | :------------- |
| 0 | Debug level 1 |
| 1 | Trace open close actions |
| 2 | Trace watchdog actions |
| 3 | Trace debounce actions |
| 4 | Trace event actions |

### Get Config Value (code 98)

Get a picod configuration parameter. The parameter is a 32 bit config ID.

The response contains the 32 bit config value.

### Get Version (code 99)

This is a request for the picod software version. The response contains one 32 bit value than contains the version number.

## Responses

> [!NOTE]
> While picod supports accumulating multiple commands in a request packet to the RPi Pico, and each command may demand a response, but each response will be sent as its own separate packet.

### Data Format

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 0 | 2 | Length |
| 2 | 1 | Reflects the request flags |
| 3 | 1 | Reflects the request code |
| 4 | 1 | Status Code |
| 5 | N | Optional response data |

### Status Code

| Value | Description    |
| :---- | :------------- |
| 0 | No error |
| 1 | Bad channel |
| 2 | Channel closed |
| 3 | Bad GPIO pin |
| 4 | Bad parameter |
| 5 | Write error |
| 6 | Read error |
| 7 | No reply |
| 8 | GPIO pin in use |
| 9 | Unknown command |
| 10 | Timed out |
| 11 | Invalid when master |
| 12 | Invalid when station |
| 13 | Bad config item |

## Unsollicited Responses

Some messages from picod are not issued as a response to a request. The client may setup alerts and watchdogs, and picod will autonomously generate messages when a state changes (or does not change). Each message provides a sequence of events, each one with timing information. This is much more efficient than polling, with a much lower risk of missing a transition.

### Data Format

| Offset | Size | Description    |
| :----- | ---- | :------------- |
| 0 | 2 | Length |
| 2 | 1 | 0 |
| 3 | 1 | Unsollicited message code |
| 4 | N | Data |

That format is compatible with the response format, with the exception of the missing status code field. A decoder must check the content of the request/message code field, as the messages codes are distinct from the requests codes, when decoding any message data from the RPi Pico.

### GPIO Levels (code 0xfb)

The Pico will continually monitor the pins that were configured for alert (see Alert Select) or watchdog (see Alert Watchdog), and reports any change as a GPIO level message. The data is a sequence of reports, where each report is made of two 32 bit values: the first value is a tick value in microseconds, the second value is the mask of the pin states. The tick is only meaningful compared to a previous tick.

The watchdog reports are indicated by bit 31 being set in the mask of pin states, (The number of GPIO pins on a RPi Pico is limited to 30, i.e. bits 0 to 29.)

The picod code may accumulate multiple reports when the changes are extremely fast, or when different types of reports are triggered simultaenously. Each report provide a complete state of all GPIO pins, and therefore multiple state changes could be included in a single report.

