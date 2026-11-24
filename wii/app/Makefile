# Wii build: needs devkitPPC + libogc, and sibling checkouts of quatric/libwiiinput and
# quatric/libwiiupdate built with `make` (override LIBINPUT / LIBUPDATE if elsewhere).
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
LIBINPUT  ?= ../../../libwiiinput
LIBUPDATE ?= ../../../libwiiupdate
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol
OGC := $(DEVKITPRO)/libogc
CFLAGS := -O2 -Wall -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -I$(OGC)/include -I$(LIBINPUT)/include -I$(LIBUPDATE)/include
LDFLAGS := -mrvl -mcpu=750 -meabi -mhard-float -L$(OGC)/lib/wii -L$(LIBINPUT)/lib -L$(LIBUPDATE)/lib
LIBS := -lwiiupdate -linput-wii -lwiikeyboard -lwiiuse -lbte -lfat -logc -lm

all: jetpackjoyride_wii.dol
jetpackjoyride_wii.elf: source/main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) $(LIBS) -o $@
%.dol: %.elf
	$(ELF2DOL) $< $@
clean:
	rm -f *.elf *.dol
