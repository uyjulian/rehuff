CFLAGS = -g -Wall -W -O2

all: rehuff

rehuff: rehuff.o sogg.o headers.o count.o recode.o recode-headers.o
	$(CC) $(CFLAGS) -o rehuff rehuff.o sogg.o headers.o count.o recode.o recode-headers.o -logg -lm

clean:
	-rm rehuff *.o
