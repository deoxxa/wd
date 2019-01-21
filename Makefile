prefix?=/usr/local

wd_cflags:=-std=c99 -Wall -Wextra -pedantic -I. $(CFLAGS)
wd_dynamic_libs:=-ltermbox -lcmark-gfm -lcmark-gfm-extensions
wd_static_libs:=vendor/cmark-gfm/extensions/cmark-gfm-extensions.a vendor/cmark-gfm/src/cmark-gfm.a vendor/termbox/src/libtermbox.a
wd_ldlibs:=-lssl -lcurl -lduktape -lm -luriparser $(LDLIBS)
wd_objects:=$(patsubst %.c,%.o,$(wildcard *.c))
wd_vendor_deps:=

ifdef wd_vendor
  wd_ldlibs:=$(wd_static_libs) $(wd_ldlibs)
  wd_cflags:=-Ivendor/termbox/src -Ivendor/cmark-gfm/src $(wd_cflags)
  wd_vendor_deps:=$(wd_static_libs)
else
  wd_ldlibs:=$(wd_dynamic_libs) $(wd_ldlibs)
endif

all: wd

wd: $(wd_vendor_deps) $(wd_objects)
	$(CC) $(wd_cflags) $(wd_objects) $(wd_ldflags) $(wd_ldlibs) -o $@

$(wd_objects): %.o: %.c
	$(CC) -c $(wd_cflags) $< -o $@

$(wd_vendor_deps):
	$(MAKE) -C vendor

install: wd
	install -D -v -m 755 wd $(DESTDIR)$(prefix)/bin/wd

clean:
	rm -f wd $(wd_objects) $(wd_vendor_deps)
	$(MAKE) -C vendor clean
