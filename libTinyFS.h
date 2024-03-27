#ifndef LIBTINYFS_H
#define LIBTINYFS_H

#include "tinyFS.h"

#define FT_SIZE_INC 100

#define TYPE_S 1
#define TYPE_I 2
#define TYPE_D 3
#define TYPE_F 4

#define ROOT_BLOCK 1

#define OFFSET_TYPE 0
#define OFFSET_MAGIC 1
#define OFFSET_LINK 2

#define OFFSET_S_SIZE 4
#define LEN_S_SIZE 4
#define OFFSET_S_FREE 8

#define OFFSET_I_NAME 4
#define LEN_I_NAME 8
#define OFFSET_I_SIZE 12
#define LEN_I_SIZE 3
#define OFFSET_I_DIR 15
#define OFFSET_I_LINKS 16

#define OFFSET_D_DATA 4

#define MAX_FILENAME 255

struct openFileEntry_s{
    fileDescriptor fd;
    char filename[MAX_FILENAME+1];
    int byteOffset;
    int inodeBlock;
} typedef openFileEntry;

struct openFileTable_s{
    openFileEntry *table;
    int maxSize;
    int currSize;
    fileDescriptor nextFd;
}typedef openFileTable;


int tfs_mkfs(char *filename, int nBytes);
int tfs_mount(char *diskname);
int tfs_unmount(void);
fileDescriptor tfs_openFile(char *name);
int tfs_closeFile(fileDescriptor FD);
int tfs_writeFile(fileDescriptor FD,char *buffer, int size);
int tfs_deleteFile(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char *buffer);
int tfs_seek(fileDescriptor FD, int offset);

int tfs_createDir(char *dirName);
int tfs_removeDir(char *dirName);
int tfs_removeAll(char *dirName);
int tfs_readdir();
int tfs_rename(fileDescriptor FD, char* newName);

fileDescriptor accessFile(char *name, int isdir);
int getFreeBlock();
int createInode(char* name, int isdir, unsigned char *dirInode, int dirIdx);
int searchDir(char *filename, unsigned char *dirBlock);
int getFreeBlock();
int deleteBlock(int deleteIdx);
int deleteFileContent(int inodeIdx);
int checkInodeExists(int inodeIdx);
int openInode(char *name, int create, int isdir);
int readdir(char *dirName);
int deleteParentLinks(char *filename, int blockIdx);

int appendFileTable(char *name);
int popFileTable(fileDescriptor fd);
int searchFileTable(fileDescriptor FD);
int updateFileInodeNumber(fileDescriptor fd, int inodeIdx);


#endif