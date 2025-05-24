#
# Utility to connect to DCNet using dreampi.
# To install:
# make install
# Make a copy of /home/pi/dreampi/dreampi.py
# Copy the included dreampi.py into /home/pi/dreampi
#
CFLAGS=-O3 -Wall -Wconversion

dcnet: dcnet.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f dcnet.o dcnet

install: dcnet
	cp dcnet /home/pi/dreampi/dcnet.rpi

