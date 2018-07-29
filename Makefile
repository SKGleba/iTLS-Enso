TARGET=iTLS-Enso
TITLE_ID=SKGTLSE12
OBJS   = main.o font.o graphics.o

LIBS = -lSceCtrl_stub -lSceDisplay_stub -lScePower_stub -lSceVshBridge_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3
ASFLAGS = $(CFLAGS)

all: $(TARGET).vpk

%.vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "iTLS-Enso" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
	-a pkg/Media/00=Media/00 \$@


eboot.bin: $(TARGET).velf
	vita-make-fself -c $< $@

%.velf: %.elf
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

%.o: %.png
	$(PREFIX)-ld -r -b binary -o $@ $^

clean:
	@rm -rf $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) \
		eboot.bin param.sfo
