CFLAGS+=-Werror -Wall -pedantic 
LDLIBS+=-lcmark-gfm -lcmark-gfm-extensions -lssl -lcurl -ltermbox -lduktape -lm

wd: wd.o buf.o draw.o vm_node.o wd_node.o wd_node_meta.o

.PHONY: clean
clean:
	rm -f wd *.o
