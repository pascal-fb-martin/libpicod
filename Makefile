# housepicod - A simple home web server for world control through Pico GPIO
#
# Copyright 2026, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
# WARNING
#
prefix=/usr/local

INSTALL=/usr/bin/install

# LIBOBJS=picod_crc.o picod_packets.o picod_gpio.o picod_pwm.o picod_spi.o picod_uart.o
LIBOBJS=picod_crc.o picod_packets.o picod_gpio.o

all: libpicod.a picodtst picodcmd

clean:
	rm -f *.o *.a picodtst picodcmd

rebuild: clean all

%.o: %.c
	gcc -c -Wall -Os -g -o $@ $<

libpicod.a: $(LIBOBJS)
	ar r $@ $^
	ranlib $@

picodtst: picodtst.o libpicod.a
	gcc -Os -o picodtst picodtst.o libpicod.a -lrt

picodcmd: picodcmd.o libpicod.a
	gcc -Os -o picodcmd picodcmd.o libpicod.a -lrt

install:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/bin
	$(INSTALL) -m 0755 -s picodcmd $(DESTDIR)$(prefix)/bin
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/lib
	$(INSTALL) -m 0644 libpicod.a $(DESTDIR)$(prefix)/lib
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0644 picod.h picod_packets.h $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0644 picod_gpio.h $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0755 -d $(DESTDIR)/etc/udev/rules.d
	$(INSTALL) -m 0644 99-RPI-pico.rules $(DESTDIR)/etc/udev/rules.d
	if [ "x$(DESTDIR)" = "x" ] ; then /usr/bin/udevadm control --reload-rules ; fi
	if [ "x$(DESTDIR)" = "x" ] ; then /usr/bin/udevadm trigger ; fi

uninstall:
	rm -rf $(DESTDIR)$(prefix)/bin/picodcmd
	rm -f $(DESTDIR)$(prefix)/bin/housepicod
	rm -f $(DESTDIR)$(prefix)/include/picod.h
	rm -f $(DESTDIR)$(prefix)/include/picod_packets.h
	rm -f $(DESTDIR)$(prefix)/include/picod_gpio.h
	rm -f $(DESTDIR)$(prefix)/lib/libpicod.a
	rm -f $(DESTDIR)/etc/udev/rules.d/99-RPI-pico.rules
	/usr/bin/udevadm control --reload-rules
	/usr/bin/udevadm trigger

debian-package:
	rm -rf build
	$(INSTALL) -m 0755 -d build/libpicod/DEBIAN
	cat debian/control | sed "s/{{arch}}/`dpkg --print-architecture`/" > build/libpicod/DEBIAN/control
	chmod 0644 build/libpicod/DEBIAN/control
	$(INSTALL) -m 0644 debian/copyright build/libpicod/DEBIAN
	$(INSTALL) -m 0644 debian/changelog build/libpicod/DEBIAN
	$(INSTALL) -m 0755 debian/postinst build/libpicod/DEBIAN
	$(INSTALL) -m 0755 debian/postrm build/libpicod/DEBIAN
	make DESTDIR=build/libpicod install
	cd build ; fakeroot dpkg-deb -b libpicod

