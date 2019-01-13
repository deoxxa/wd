CFLAGS+=-Werror -Wall -pedantic 
LDLIBS+=-lcmark -lssl -lcurl -ltermbox -lduktape

wd: wd.o buf.o draw.o

.PHONY: clean
clean:
	rm -f wd *.o
