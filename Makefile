#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/ds_rules

export TARGET		:=	RocketVideoPlayer
export TOPDIR		:=	$(CURDIR)
#export NITRODATA	:=	nitrofiles

export VERSION_MAJOR	:= 1
export VERSION_MINOR	:= 99
export VERSTRING	:=	$(VERSION_MAJOR).$(VERSION_MINOR)

#---------------------------------------------------------------------------------
# External tools
#---------------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
MAKECIA 	?= make_cia.exe

else
MAKECIA 	?= make_cia

endif

#---------------------------------------------------------------------------------
# path to tools - this can be deleted if you set the path in windows
#---------------------------------------------------------------------------------
export PATH		:=	$(DEVKITARM)/bin:$(PATH)

.PHONY: $(TARGET).arm7 $(TARGET).arm9 libfat4

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: libfat4 $(TARGET).nds $(TARGET).dsi

$(TARGET).nds	:	$(TARGET).arm7 $(TARGET).arm9
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).arm7.elf -9 arm9/$(TARGET).arm9.elf \
			-b $(CURDIR)/icon.bmp "Rocket Video Player;Rocket Robz"

$(TARGET).dsi	:	$(TARGET).arm7 $(TARGET).arm9
	ndstool	-c $(TARGET).dsi -7 arm7/$(TARGET).arm7.elf -9 arm9/$(TARGET).arm9.elf \
			-b $(CURDIR)/icon.bmp "Rocket Video Player;Rocket Robz" \
			-g HRVA 00 "ROCKETVIDEO" -z 80040000 -u 00030004

	@$(TOPDIR)/$(MAKECIA) --srl=$(TARGET).dsi

#---------------------------------------------------------------------------------
$(TARGET).arm7	: arm7/$(TARGET).elf
$(TARGET).arm9	: arm9/$(TARGET).elf

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	$(MAKE) -C arm7
	
#---------------------------------------------------------------------------------
arm9/$(TARGET).elf:
	$(MAKE) -C arm9

#---------------------------------------------------------------------------------
clean:
	$(MAKE) -C arm9 clean
	$(MAKE) -C arm7 clean
	rm -f arm9/source/version.h
	@$(MAKE) -C libs/libfat4 clean
	rm -f $(TARGET).nds $(TARGET).arm7 $(TARGET).arm9 $(TARGET).dsi $(TARGET).cia

libfat4:
	$(MAKE) -C libs/libfat4
