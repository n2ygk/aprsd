# Makefile for APRS Server 2.1.4.vk3sb.1

CXX = g++
CC = $(CXX)
CXXFLAGS = $(USE_FHS)
CFLAGS = $(USE_FHS)
LIBS = -lpthread -lcrypt

LDFLAGS = $(LIBS)
SOCKETS := $(shell test -f /usr/include/netax25/axlib.h && echo 1)

ifeq ($(SOCKETS), 1)
	LDFLAGS += -lax25
	CXXFLAGS += -DSOCKETS
	CFLAGS += -DSOCKETS
	SOCKET_DEPENDS = sockets.o
endif

DEPENDS = utils.o history.o rf.o serial.o cpqueue.o aprsString.o mic_e.o queryResp.o dupCheck.o crc32.o validate.o $(SOCKET_DEPENDS)

all: aprsd aprspass

aprsd: aprsd.o $(DEPENDS)

aprspass: aprspass.o validate.o
	$(CC) -o $@ $(LIBS) $^

#######################################################################

aprsd.o: $(DEPENDS)
cpqueue.o: utils.o cpqueue.h constant.h
serial.o: serial.h constant.h
history.o: utils.o history.h constant.h
utils.o: utils.h utils.h constant.h
aprsString.o: aprsString.h  constant.h
mic_e.o: mic_e.h
rf.o: rf.h serial.o $(SOCKET_DEPENDS)
dupCheck.o: dupCheck.h constant.h
crc32.o: crc.h
aprspass.o: validate.h

clean:
	-rm *.o
	-rm aprsd aprspass

install:
	INSTALL

