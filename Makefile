#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/ds_rules

export TARGET		:=	RocketVideoPlayer
export TOPDIR		:=	$(CURDIR)
export NITRODATA	:=	nitrofiles

export VERSION_MAJOR	:= 1
export VERSION_MINOR	:= 99
export VERSTRING	:=	$(VERSION_MAJOR).$(VERSION_MINOR)

#---------------------------------------------------------------------------------
# path to tools - this can be deleted if you set the path in windows
#---------------------------------------------------------------------------------
export PATH		:=	$(DEVKITARM)/bin:$(PATH)

.PHONY: $(TARGET).arm7 $(TARGET).arm9

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: $(TARGET).nds

$(TARGET).nds	:	$(TARGET).arm7 $(TARGET).arm9
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).arm7.elf -9 arm9/$(TARGET).arm9.elf -d $(NITRODATA) \
			-b $(CURDIR)/icon.bmp "Rocket Video Player;RocketRobz"
	python patch_ndsheader_dsiware.py $(CURDIR)/$(TARGET).nds --accessControl 0x00000038

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
	rm -f arm9/data/load.bin
	rm -f arm9/source/version.h
	rm -f $(TARGET).ds.gba $(TARGET).nds $(TARGET).arm7 $(TARGET).arm9 $(TARGET).nds.orig.nds $(TARGET).cia
