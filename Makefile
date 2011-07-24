CROSS_GTK_PATH  = "/usr/GTK"
CPP  = i486-mingw32-g++
CC   = i486-mingw32-gcc
CFLAGS += -mms-bitfields
CFLAGS += -std=gnu99
CFLAGS += -I$(CROSS_GTK_PATH)/include/gtk-2.0
CFLAGS += -I$(CROSS_GTK_PATH)/lib/gtk-2.0/include
CFLAGS += -I$(CROSS_GTK_PATH)/include/atk-1.0
CFLAGS += -I$(CROSS_GTK_PATH)/include/cairo
CFLAGS += -I$(CROSS_GTK_PATH)/include/gdk-pixbuf-2.0
CFLAGS += -I$(CROSS_GTK_PATH)/include/pango-1.0
CFLAGS += -I$(CROSS_GTK_PATH)/include/glib-2.0
CFLAGS += -I$(CROSS_GTK_PATH)/lib/glib-2.0/include
CFLAGS += -I$(CROSS_GTK_PATH)/include
CFLAGS += -I$(CROSS_GTK_PATH)/include/freetype2
CFLAGS += -I$(CROSS_GTK_PATH)/include/libpng14
LDFLAGS += -L$(CROSS_GTK_PATH)/lib
LDFLAGS += -lgtk-win32-2.0 -lgdk-win32-2.0 -lwsock32
LDFLAGS += -latk-1.0 -lgio-2.0 -lpangowin32-1.0 -lgdi32 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lpango-1.0
LDFLAGS += -lcairo -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lglib-2.0 -lintl -lpng14

all:linux windows
linux:adcap.c fft.c
	gcc adcap.c fft.c -Wl,--export-dynamic `pkg-config --libs --cflags gtk+-2.0` -o adcap
windows:adcap.c
	${CC} ${CFLAGS} adcap.c fft.c ${LDFLAGS} -o adcap.exe -mwindows

clean:
	-@rm -rf *.o

distclean:clean
	-@rm -rf adcap adcap.exe
