#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "libDisk.h"
#include "tinyFS.h"

int openDisk(char *filename, int nBytes){
    int disk;
    if (nBytes == 0){
        disk = open(filename, O_RDWR);
        if (disk < 0){
            perror("open");
            return -1;  //ERROR CODE, file doesn't exist
        }
    }
    else if (nBytes < BLOCKSIZE){
        return -1; // ERROR CODE, block size too small
    }
    else{
        disk = open(filename, O_CREAT|O_RDWR, S_IRWXU|S_IRUSR|S_IRUSR);
        if (disk < 0){
            perror("open");
            return -1;  //ERROR CODE, failed to create file
        }

        int numBlocks = nBytes / BLOCKSIZE;
        unsigned char zeros[BLOCKSIZE];
        memset(zeros, 0, BLOCKSIZE);

        int b;
        for (b = 0;b<numBlocks;b++){
            if (write(disk, zeros, BLOCKSIZE) < BLOCKSIZE){
                perror("write");
                return -1; // ERROR CODE, failed to write to disk
            }
        }

        if (lseek(disk, 0, SEEK_SET) != 0){
            perror("lseek");
            return -1; // ERROR CODE, failed seek
        }
    }
    return disk;
}

int closeDisk(int disk){
    if (close(disk) == -1){
        perror("close");
        return -1; // ERROR CODE, failed to close
    }
    return 0;
}


int readBlock(int disk, int bNum, void *block){
    int byteOffset = bNum * BLOCKSIZE;
    if (lseek(disk, byteOffset, SEEK_SET) != byteOffset){
        perror("lseek");
        return -1; // ERROR CODE, failed seek
    }
    if (read(disk, block, BLOCKSIZE) < BLOCKSIZE){
        perror("read");
        return -1; // ERROR CODE, failed to read
    }
    return 0;
}


int writeBlock(int disk, int bNum, void *block){
    int byteOffset = bNum * BLOCKSIZE;
    if (lseek(disk, byteOffset, SEEK_SET) != byteOffset){
        perror("lseek");
        return -1; // ERROR CODE, failed seek
    }
    if (write(disk, block, BLOCKSIZE) < BLOCKSIZE){
        perror("write");
        return -1; // ERROR CODE, failed to write
    }
    return 0;
}

