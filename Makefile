CC=gcc
CFLAGS= -O3 -Wall -g
LIBS=-lm
AR=ar

ALL=suncalc

all: ${ALL}

clean:
	rm -f *.o ${ALL}

suncalc: spa.o suncalc.o
	$(CC) spa.o suncalc.o -o suncalc ${LIBS}
