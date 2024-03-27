/* TinyFS demo file
 *  * Foaad Khosmood, Cal Poly / modified Winter 2014
 *   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinyFS.h"
#include "libTinyFS.h"
#include "TinyFS_errno.h"

// read, write, seek
void test_RW(){
    if (tfs_mkfs (DEFAULT_DISK_NAME, 200*BLOCKSIZE) < 0){
        printf("failed to make disk\n");
        return;
    }
    if (tfs_mount (DEFAULT_DISK_NAME) < 0){
        printf("failed to mount disk\n");
        return;
    }

    char buffer[BLOCKSIZE*400];
    memset(buffer, '$', BLOCKSIZE*400);
    buffer[100] = '!';
    buffer[2] = '!';

    fileDescriptor aFD = tfs_openFile("afile");
    tfs_writeFile(aFD, buffer, BLOCKSIZE*400);  // should fail
    tfs_writeFile(aFD, buffer, BLOCKSIZE*100);
    char c[2];
    c[1] = '\0';

    tfs_readByte(aFD, c);   // $
    printf("%s",c);
    tfs_readByte(aFD, c);   // $
    printf("%s",c);
    tfs_readByte(aFD, c);   // !
    printf("%s",c);
    printf("\n");

    tfs_seek(aFD, 100); 
    tfs_readByte(aFD, c);   // !
    printf("%s",c);
    printf("\n");

    tfs_seek(aFD, BLOCKSIZE*100 + 100);
    tfs_readByte(aFD, c);   // should fail

    tfs_deleteFile(aFD);
    tfs_readByte(aFD, c);   // should fail
    tfs_closeFile(aFD);
    tfs_readByte(aFD, c);   // should fail

    tfs_unmount();
}

// mount, mkfs, unmount
void test_mount(){
    tfs_mkfs(DEFAULT_DISK_NAME, 500*BLOCKSIZE);
    tfs_mount("nonexistant");
    tfs_mkfs(DEFAULT_DISK_NAME, 1*BLOCKSIZE);

    tfs_mkfs(DEFAULT_DISK_NAME, 2*BLOCKSIZE);
    tfs_mount(DEFAULT_DISK_NAME);
    tfs_openFile("afile");

    tfs_unmount();
}

// test open, close, delete, create, rename, directory operations
void test_dir(){
    tfs_mkfs(DEFAULT_DISK_NAME, 100*BLOCKSIZE);
    tfs_mount(DEFAULT_DISK_NAME);

    fileDescriptor fd1 = tfs_openFile("file2");
    tfs_openFile("/file2");
    fileDescriptor fd2 = tfs_openFile("/12345678");

    tfs_createDir("/home");
    tfs_createDir("/home/mprov");
    tfs_createDir("/home/mprov/Desktop");
    tfs_openFile("/home/mprov/Desktop/cat_pic");
    tfs_openFile("/home/mprov/Desktop/dog_pic");
    tfs_createDir("/home/mprov/Desktop/cat_pic");   // should fail

    tfs_openFile("/home/mprov/Pictures/nope");      // should fail
    tfs_createDir("/home/mprov/Pictures/nope");     // should fail

    tfs_readdir();
    printf("\n");

    tfs_rename(fd1, "newName1");
    tfs_rename(fd2, "newName2");
    tfs_readdir();
    printf("\n");

    tfs_removeDir("/home/mprov/Desktop");           // should fail
    tfs_removeAll("/home/mprov/Desktop");
    tfs_readdir();
    printf("\n");

    tfs_removeAll("/");
    tfs_readdir();
    printf("\n");

    tfs_unmount();
}

int main ()
{
    printf("test mount -------------------------------\n");
    test_mount();
    printf("\n");

    printf("test RW -------------------------------\n");
    test_RW();
    printf("\n");

    printf("test dir -------------------------------\n");
    test_dir();
    printf("\n");
    return 0;
}

