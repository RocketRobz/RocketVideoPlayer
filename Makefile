# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

# User config
# ===========

NAME		:= RocketVideoPlayer

GAME_TITLE	:= Rocket Video Player
GAME_AUTHOR	:= Rocket Robz
GAME_ICON	:= icon.png

# DLDI and internal SD slot of DSi
# --------------------------------

# Root folder of the SD image
SDROOT		:= sdroot
# Name of the generated image it "DSi-1.sd" for no$gba in DSi mode
SDIMAGE		:= image.bin

# Source code paths
# -----------------

# List of folders to combine into the root of NitroFS:
NITROFSDIR	?=

# Tools
# -----

MAKE		:= make
RM		:= rm -rf

ifeq ($(OS),Windows_NT)
MAKECIA 	?= ./make_cia.exe

else
MAKECIA 	?= ./make_cia

endif

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Build artfacts
# --------------

ROM			:= $(NAME).nds
ROM_DSI		:= $(NAME).dsi
ROM_CIA		:= $(NAME).cia

# Targets
# -------

.PHONY: all clean arm9 arm7 dldipatch sdimage

all: $(ROM)

clean:
	@echo "  CLEAN"
	$(V)$(MAKE) -f arm9/Makefile clean --no-print-directory
	$(V)$(MAKE) -f arm7/Makefile clean --no-print-directory
	$(V)$(RM) $(ROM) $(ROM_DSI) $(ROM_CIA) build $(SDIMAGE)

arm9:
	$(V)+$(MAKE) -f arm9/Makefile --no-print-directory

arm7:
	$(V)+$(MAKE) -f arm7/Makefile --no-print-directory

ifneq ($(strip $(NITROFSDIR)),)
# Additional arguments for ndstool
NDSTOOL_ARGS	:= -d $(NITROFSDIR)

# Make the NDS ROM depend on the filesystem only if it is needed
$(ROM): $(NITROFSDIR)
endif

# Combine the title strings
ifeq ($(strip $(GAME_SUBTITLE)),)
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_AUTHOR)
else
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)
endif

$(ROM): arm9 arm7
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 build/arm7.elf -9 build/arm9.elf \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		$(NDSTOOL_ARGS)
	@echo "  NDSTOOL $(ROM_DSI)"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $(ROM_DSI) \
		-7 build/arm7.elf -9 build/arm9.elf \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		-g HRVA 00 "ROCKETVIDEO"
	$(V)$(MAKECIA) --srl=$(ROM_DSI)

sdimage:
	@echo "  MKFATIMG $(SDIMAGE) $(SDROOT)"
	$(V)$(BLOCKSDS)/tools/mkfatimg/mkfatimg -t $(SDROOT) $(SDIMAGE)

dldipatch: $(ROM)
	@echo "  DLDIPATCH $(ROM)"
	$(V)$(BLOCKSDS)/tools/dldipatch/dldipatch patch \
		$(BLOCKSDS)/sys/dldi_r4/r4tf.dldi $(ROM)
