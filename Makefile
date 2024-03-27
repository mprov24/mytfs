CC = gcc
CFLAGS = -Wall -g

all: tinyFSDemo

tinyFsDemo.o: tinyFSDemo.c libTinyFS.h tinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libTinyFS.o: libTinyFS.c libTinyFS.h tinyFS.h libDisk.o TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libDisk.o: libDisk.c libDisk.h tinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

diskTest: diskTest.o libDisk.o
	$(CC) $(CFLAGS) -o diskTest diskTest.o libDisk.o

diskTest.o: diskTest.c libDisk.c libDisk.h
	$(CC) $(CFLAGS) -c $< -o $@

tfsTest: tfsTest.o libDisk.o libTinyFS.o
	$(CC) $(CFLAGS) -o tfsTest tfsTest.o libDisk.o libTinyFS.o

tfsTest.o: tfsTest.c tinyFS.h libTinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c $< -o $@

tinyFSDemo: tinyFSDemo.o libDisk.o libTinyFS.o
	$(CC) $(CFLAGS) -o tinyFSDemo tinyFSDemo.o libDisk.o libTinyFS.o

tinyFSDemo.o: tinyFSDemo.c tinyFS.h libTinyFS.h TinyFS_errno.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o 