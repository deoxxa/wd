cmark_cflags:=-std=c99 -Wall -Wextra -pedantic -Wno-unused-result -DHAVE_STDBOOL_H $(CFLAGS)
cmark_objects:=$(filter-out cmark-gfm/src/main.o,$(patsubst cmark-gfm/src/%.c,cmark-gfm/src/%.o,$(wildcard cmark-gfm/src/*.c)))
cmark_extensions_objects:=$(patsubst cmark-gfm/extensions/%.c,cmark-gfm/extensions/%.o,$(wildcard cmark-gfm/extensions/*.c))

termbox_cflags:=-std=c99 -Wall -Wextra -pedantic -Wno-unused-result -D_XOPEN_SOURCE $(CFLAGS)
termbox_objects:=$(patsubst termbox/src/%.c,termbox/src/%.o,$(wildcard termbox/src/*.c))

all: cmark-gfm/src/cmark-gfm.a cmark-gfm/extensions/cmark-gfm-extensions.a termbox/src/libtermbox.a

termbox/.git cmark-gfm/.git:
	git submodule update --init

cmark-gfm/src/cmark-gfm.a: cmark-gfm/.git $(cmark_objects)
	$(AR) rcs $@ $(cmark_objects)

$(cmark_objects): %.o: %.c cmark-gfm/src/config.h
	$(CC) -c $(cmark_cflags) $< -o $@

cmark-gfm/extensions/cmark-gfm-extensions.a: cmark-gfm/.git $(cmark_extensions_objects)
	$(AR) rcs $@ $(cmark_extensions_objects)

$(cmark_extensions_objects): %.o: %.c cmark-gfm/src/config.h
	$(CC) -c $(cmark_cflags) -Icmark-gfm/src $< -o $@

cmark-gfm/src/config.h: cmark-gfm/src/config.h.in
	sed -e '/cmakedefine/ d' $< > $@

cmark-gfm/src/config.h.in: cmark-gfm/.git

termbox/src/libtermbox.a: termbox/.git $(termbox_objects)
	$(AR) rcs $@ $(termbox_objects)

$(termbox_objects): %.o: %.c
	$(CC) -c $(termbox_cflags) $< -o $@

clean:
	rm -f cmark-gfm/src/cmark-gfm.a cmark-gfm/src/config.h $(cmark_objects)
	rm -f cmark-gfm/extensions/cmark-gfm-extensions.a $(cmark_extensions_objects)
	rm -f termbox/src/libtermbox.a $(termbox_objects)
