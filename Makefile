CC ?= gcc
CPPFLAGS += -Iinclude -Iassets
CFLAGS ?= -O2 -Wall -Wextra -std=c99
LDLIBS += -lm

CORE = src/fb.c src/text.c src/theme.c src/easing.c src/library.c src/ui.c src/app.c src/assets.c src/assets_data.S src/mp4.c
PREVIEW = build/preview

.PHONY: all preview clean psp test
all: preview

test:
	mkdir -p build
	python3 tests/make_mp4_fixture.py
	$(CC) -Iinclude $(CFLAGS) -o build/test_mp4 tests/test_mp4.c src/mp4.c
	./build/test_mp4 build/test-avc.mp4

preview: $(PREVIEW)
	./$(PREVIEW)

$(PREVIEW): $(CORE) src/main_preview.c include/ezt.h include/ezt_ui.h assets/atlas.h assets/art.h assets/atlas.bin assets/art.bin
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(CORE) src/main_preview.c $(LDLIBS)

psp:
	$(MAKE) -f Makefile.psp

clean:
	rm -rf build/preview build/preview-*.png
