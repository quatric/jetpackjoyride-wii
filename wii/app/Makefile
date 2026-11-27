# Wii build: needs devkitPPC + libogc, and sibling checkouts of quatric/libwiiinput and
# quatric/libwiiupdate built with `make` (override LIBINPUT / LIBUPDATE if elsewhere).
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
LIBINPUT  ?= ../../../libwiiinput
LIBUPDATE ?= ../../../libwiiupdate
PORTLIBS := $(DEVKITPRO)/portlibs/ppc
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol
OGC := $(DEVKITPRO)/libogc2/wii
CFLAGS := -O2 -Wall -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -I$(OGC)/include -I$(LIBINPUT)/include -I$(LIBUPDATE)/include -I$(PORTLIBS)/include
LDFLAGS := -mrvl -mcpu=750 -meabi -mhard-float -L$(OGC)/lib -L$(LIBINPUT)/lib -L$(LIBUPDATE)/lib -L$(PORTLIBS)/lib
LIBS := -lwiiupdate -linput-wii -lwiikeyboard -lwiiuse -lbte -lfat -lvorbisidec -logg -lasnd -logc -lm

# make AUTOTEST=1 builds a self-playing test version
ifdef AUTOTEST
CFLAGS += -DAUTOTEST=$(AUTOTEST)   # 2 = stay on foot (no vehicles), for the on-foot paths
endif

all: jetpackjoyride_wii.dol
jetpackjoyride_wii.elf: $(wildcard source/*.c)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
%.dol: %.elf
	$(ELF2DOL) $< $@
clean:
	rm -f *.elf *.dol
