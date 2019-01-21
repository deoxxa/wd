prefix?=/usr/local

wd_cflags:=-std=c99 -Wall -Wextra -pedantic -I. $(CFLAGS)
wd_dynamic_libs:=-lcmark-gfm -lcmark-gfm-extensions -lduktape -ltermbox -luriparser
wd_static_libs:=vendor/cmark-gfm/extensions/cmark-gfm-extensions.a vendor/cmark-gfm/src/cmark-gfm.a vendor/duktape/src/duktape.a vendor/termbox/src/termbox.a vendor/uriparser/src/uriparser.a
wd_ldlibs:=-lcurl -lm $(LDLIBS)
wd_objects:=$(patsubst %.c,%.o,$(wildcard *.c))
wd_vendor_deps:=
wd_static_flag:=

ifdef wd_vendor
  wd_ldlibs:=$(wd_static_libs) $(wd_ldlibs)
  wd_cflags:=-Ivendor/termbox/src -Ivendor/cmark-gfm/src -Ivendor/duktape/src -Ivendor/uriparser/include $(wd_cflags)
  wd_vendor_deps:=$(wd_static_libs)
else
  wd_ldlibs:=$(wd_dynamic_libs) $(wd_ldlibs)
endif

ifdef wd_static
	wd_static_flag:=-static
endif

all: wd

wd: $(wd_vendor_deps) $(wd_objects)
	$(CC) $(wd_static_flag) $(wd_cflags) $(wd_objects) $(wd_ldflags) $(wd_ldlibs) -o $@

$(wd_objects): %.o: %.c
	$(CC) -c $(wd_cflags) $< -o $@

$(wd_vendor_deps):
	$(MAKE) -C vendor

install: wd
	install -D -v -m 755 wd $(DESTDIR)$(prefix)/bin/wd

clean:
	rm -f wd $(wd_objects) $(wd_vendor_deps)
	$(MAKE) -C vendor clean
