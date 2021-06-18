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
	-a sce_sys/icon0.png=sce_sys/icon0.png \
    -a sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
    -a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	-a kernel/itls.skprx=kernel.skprx \
	-a old_dns_fix/old_dns_fix.suprx=dnsfix.suprx \
	-a misc/compat.suprx=compat.suprx \
	-a misc/lhttp.suprx=lhttp.suprx \
	-a misc/lssl.suprx=lssl.suprx \
	-a misc/comm2.suprx=comm2.suprx \
	-a misc/certs.cer=certs.cer \$@

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
