LXPANEL_CFLAGS=$(shell pkg-config --cflags --libs lxpanel)
SRCS=\
	nl.c \
	netload.c
GLIB2_CFLAGS=$(shell pkg-config --cflags --libs glib-2.0)
GTK2_CFLAGS=$(shell pkg-config --cflags --libs gtk+-2.0)
CFLAGS=-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector --param=ssp-buffer-size=4  -mtune=generic -c $(LXPANEL_CFLAGS) $(GLIB2_CFLAGS) $(GTK2_CFLAGS) -I/usr/include/lxpanel -fPIC -DPIC
LDFLAGS=-shared -Wl,-soname -Wl,cpu.so $(shell pkg-config --libs lxpanel) $(shell pkg-config --libs glib-2.0) $(shell pkg-config --libs gtk+-2.0)



all: compile

compile:
	gcc $(CFLAGS) nl.c -o nl.o
	gcc $(CFLAGS) netload.c -o netload.o
	gcc $(LDFLAGS) nl.o netload.o -o netload.so