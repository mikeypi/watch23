CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
#CFLAGS = $(shell $(PKGCONFIG) --cflags)
#LIBS = $(shell $(PKGCONFIG) --libs)
#GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
#GLIB_COMPILE_SCHEMAS = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

SRC = watch23.c


OBJS =  $(SRC:.c=.o)

BACKUPS = $(SRC:.c=.c~) $(SRC:.h=.h~)

all: watch23

%.o: %.c
	$(CC) -c -o $(@F) $(CFLAGS) $<

watch23: $(OBJS) 
	$(CC) -o $(@F) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS)
	rm -f watch23





