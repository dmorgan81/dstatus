NAME = dstatus
VERSION =

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -lm -lasound -lpthread -L${X11LIB} -lX11

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
#CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}
#LDFLAGS = -s ${LIBS}

# compiler and linker
CC = cc

# Module configuration
# Comment out the relavent line to disable that module
CPPFLAGS += -DWITH_X # If disabled print to STDOUT
CPPFLAGS += -DWITH_TIME
CPPFLAGS += -DWITH_CPU
CPPFLAGS += -DWITH_MEM
CPPFLAGS += -DWITH_VOL
CPPFLAGS += -DWITH_WIFI
