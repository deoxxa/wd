cmark_cflags:=-std=c99 -Wall -Wextra -pedantic -Wno-unused-result -DHAVE_STDBOOL_H $(CFLAGS)
cmark_objects:=$(filter-out cmark-gfm/src/main.o,$(patsubst cmark-gfm/src/%.c,cmark-gfm/src/%.o,$(wildcard cmark-gfm/src/*.c)))

cmark_extensions_cflags:=$(cmark_cflags) -Icmark-gfm/src
cmark_extensions_objects:=$(patsubst cmark-gfm/extensions/%.c,cmark-gfm/extensions/%.o,$(wildcard cmark-gfm/extensions/*.c))

duktape_cflags:=-std=c99 -Wall -Wextra -pedantic -Wno-unused-result $(CFLAGS)
duktape_objects:=$(filter-out duktape/src/main.o,$(patsubst duktape/src/%.c,duktape/src/%.o,$(wildcard duktape/src/*.c)))

termbox_cflags:=-std=c99 -Wall -Wextra -pedantic -Wno-unused-result -D_XOPEN_SOURCE $(CFLAGS)
termbox_objects:=$(patsubst termbox/src/%.c,termbox/src/%.o,$(wildcard termbox/src/*.c))

uriparser_cflags:=-Wall -Wextra -pedantic -Wno-unused-result -Iuriparser/include -Iuriparser/src $(CFLAGS)
uriparser_objects:=$(patsubst uriparser/src/%.c,uriparser/src/%.o,$(wildcard uriparser/src/*.c))

all: cmark-gfm/src/cmark-gfm.a cmark-gfm/extensions/cmark-gfm-extensions.a duktape/src/duktape.a termbox/src/termbox.a uriparser/src/uriparser.a

cmark-gfm/src/cmark-gfm.a: $(cmark_objects)
	$(AR) rcs $@ $(cmark_objects)

$(cmark_objects): %.o: %.c cmark-gfm/src/config.h
	$(CC) -c $(cmark_cflags) $< -o $@

cmark-gfm/extensions/cmark-gfm-extensions.a: $(cmark_extensions_objects)
	$(AR) rcs $@ $(cmark_extensions_objects)

$(cmark_extensions_objects): %.o: %.c cmark-gfm/src/config.h
	$(CC) -c $(cmark_extensions_cflags) $< -o $@

cmark-gfm/src/config.h: cmark-gfm/src/config.h.in
	sed -e '/cmakedefine/ d' $< > $@

duktape/src/duktape.a: $(duktape_objects)
	$(AR) rcs $@ $(duktape_objects)

$(duktape_objects): %.o: %.c
	$(CC) -c $(duktape_cflags) $< -o $@

termbox/src/termbox.a: $(termbox_objects)
	$(AR) rcs $@ $(termbox_objects)

$(termbox_objects): %.o: %.c
	$(CC) -c $(termbox_cflags) $< -o $@

uriparser/src/uriparser.a: $(uriparser_objects)
	$(AR) rcs $@ $(uriparser_objects)

$(uriparser_objects): %.o: %.c
	$(CC) -c $(uriparser_cflags) $< -o $@

clean:
	rm -f cmark-gfm/src/cmark-gfm.a cmark-gfm/src/config.h $(cmark_objects)
	rm -f cmark-gfm/extensions/cmark-gfm-extensions.a $(cmark_extensions_objects)
	rm -f duktape/src/duktape.a $(cmark_extensions_objects)
	rm -f termbox/src/termbox.a $(termbox_objects)
	rm -f uriparser/src/uriparser.a $(uriparser_objects)
