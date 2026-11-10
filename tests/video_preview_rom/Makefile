# A test ROM for the Game info preview clips (src/menu/video_preview.c): the
# clip plays in the cover's place at the menu's resolution and frame cap,
# under a pretend menu workload, and how it keeps up is shown on screen and
# logged (ISViewer, e.g. ares).
#
#   make CLIP=../../build/previews/menu/metadata/N/F/X/preview.h264
#
# Its preview.seek, beside it, is copied in too when there is one.
#
# The clip is copied into the ROM; it is never committed.
BUILD_DIR = build
include $(N64_INST)/include/n64.mk

MENU = ../../src/menu
CLIP ?= ../../build/previews/menu/metadata/N/F/X/preview.h264

N64_CFLAGS += -iquote $(MENU)

# The player's own files, compiled as they are in the menu.
vpath %.c $(MENU)
src = main.c video_preview.c video_clip_check.c memory_note.c

all: video_preview_test.z64

filesystem/preview.h264: $(CLIP)
	@mkdir -p filesystem
	cp "$<" $@
	# Its seek table, if it has one (without it the clip loops the slower way).
	if [ -f "$(<:.h264=.seek)" ]; then cp "$(<:.h264=.seek)" filesystem/preview.seek; else rm -f filesystem/preview.seek; fi

$(BUILD_DIR)/video_preview_test.dfs: filesystem/preview.h264
$(BUILD_DIR)/video_preview_test.elf: $(src:%.c=$(BUILD_DIR)/%.o)

video_preview_test.z64: N64_ROM_TITLE = "Video Preview Test"
video_preview_test.z64: $(BUILD_DIR)/video_preview_test.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem video_preview_test.z64

-include $(wildcard $(BUILD_DIR)/*.d)
.PHONY: all clean
