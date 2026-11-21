CC ?= gcc
CPPFLAGS += -Iinclude -Iassets
CFLAGS ?= -O2 -Wall -Wextra -std=c99
LDLIBS += -lm

CORE = src/fb.c src/text.c src/theme.c src/easing.c src/library.c src/ui.c src/app.c src/assets.c src/assets_data.S
PREVIEW = build/preview

.PHONY: all preview clean psp
all: preview

preview: $(PREVIEW)
	./$(PREVIEW)

$(PREVIEW): $(CORE) src/main_preview.c include/ezt.h include/ezt_ui.h assets/atlas.h assets/art.h assets/atlas.bin assets/art.bin
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE) src/main_preview.c $(LDLIBS)

psp:
	$(MAKE) -f Makefile.psp

clean:
	rm -rf build/preview build/preview-*.png
