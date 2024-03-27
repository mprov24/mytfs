#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tinyFS.h"
#include "libTinyFS.h"
#include "libDisk.h"
#include "TinyFS_errno.h"

// GLOBALS --------------------------------------------------------------------
static int mount;
static openFileTable fileTable;

// ESSENTIAL INTERFACE FUNCTIONS ----------------------------------------------

// Allocates space for filesystem, formats superblock and root inode
// Assigns rest of blocks as free
int tfs_mkfs(char *filename, int nBytes){
    uint32_t nBlocks = nBytes / BLOCKSIZE;
    if (nBlocks < 2){
        printf("Error: tfs_mkfs nBytes too small to create file system\n");
        return ERR_INVALID_FS_SIZE;
    }
    if (nBlocks > 255){
        printf("Error: number of blocks must not exceed 255\n");
        return ERR_INVALID_FS_SIZE;
    }
    
    int disk;
    if ((disk = openDisk(filename, nBytes)) < 0)
        return ERR_DISK_OPERATION;

    unsigned char blockTemp[BLOCKSIZE];
    memset(blockTemp, 0, BLOCKSIZE);
    int b = 0;

    // superblock
    blockTemp[OFFSET_TYPE] = TYPE_S;        // superblock
    blockTemp[OFFSET_MAGIC] = 0x44;         // magic number
    blockTemp[OFFSET_LINK] = ROOT_BLOCK;    // root inode block
    if (nBlocks > 2)
        blockTemp[OFFSET_S_FREE] = 2;       // free block start
    memcpy(blockTemp+OFFSET_S_SIZE, &nBlocks, LEN_S_SIZE);
    if (writeBlock(disk, b++, blockTemp))
        return ERR_DISK_OPERATION;

    // root inode
    memset(blockTemp, 0, BLOCKSIZE);
    blockTemp[OFFSET_TYPE] = TYPE_I;        // inode
    blockTemp[OFFSET_MAGIC] = 0x44;         // magic number
    blockTemp[OFFSET_I_NAME] = '/';         // root name
    blockTemp[OFFSET_I_DIR] = 1;            // dir flag
    if (writeBlock(disk, b++, blockTemp))
        return ERR_DISK_OPERATION;

    // free blocks
    for (; b < nBlocks; b++){
        memset(blockTemp, 0, BLOCKSIZE);
        blockTemp[OFFSET_TYPE] = TYPE_F;    // free
        blockTemp[OFFSET_MAGIC] = 0x44;     // magic
        if (b != nBlocks-1){
            blockTemp[OFFSET_LINK] = b+1;
        }
        if (writeBlock(disk, b, blockTemp))
            return ERR_DISK_OPERATION;
    }

    if (closeDisk(disk) < 0)
        return ERR_DISK_OPERATION;

    return 0;
}

// Opens disk as mount, verifies file system, creates open file table
int tfs_mount(char *diskname){
    if (mount){
        if (tfs_unmount())
            return ERR_DISK_OPERATION;
    }

    if ((mount = openDisk(diskname, 0)) < 0){
        mount = 0;
        return ERR_DISK_OPERATION;
    }

    // superblock
    unsigned char blockTemp[BLOCKSIZE];
    if (readBlock(mount, 0, blockTemp)){
        closeDisk(mount);
        return ERR_DISK_OPERATION;
    }
    
    uint32_t nBlocks;
    memcpy(&nBlocks, blockTemp+OFFSET_S_SIZE, LEN_S_SIZE);
    if (nBlocks < 2){
        printf("Error: tfs_mount number of blocks too small to mount file system\n");
        closeDisk(mount);
        return ERR_INVALID_FS_SIZE;
    }
    if (nBlocks > 255){
        printf("Error: tfs_mount number of blocks must not exceed 255\n");
        closeDisk(mount);
        return ERR_INVALID_FS_SIZE;
    }

    // verify file system blocks
    int b;
    for (b=0;b<nBlocks;b++){
        if (readBlock(mount, b, blockTemp)){
            closeDisk(mount);
            return ERR_DISK_OPERATION;
        }
        if (b == 0 && blockTemp[OFFSET_TYPE] != TYPE_S){
            printf("Error: tfs_mount first block not superblock\n");
            closeDisk(mount);
            return ERR_FS_INTEGRITY;
        }
        if (blockTemp[OFFSET_MAGIC] != 0x44){
            printf("Error: tfs_mount magic number not found\n");
            closeDisk(mount);
            return ERR_FS_INTEGRITY;
        } 
    }

    // create open file table
    fileTable.table = calloc(FT_SIZE_INC, sizeof(openFileEntry));
    if (!fileTable.table){
        perror("calloc");
        return ERR_NO_MEMORY;
    }
    fileTable.currSize = 0;
    fileTable.maxSize = FT_SIZE_INC;
    fileTable.nextFd = 1;

    return 0;
}

// closes mount
int tfs_unmount(void){
    if (closeDisk(mount))
        return ERR_DISK_OPERATION; 
    
    free(fileTable.table);
    fileTable.currSize = 0;
    fileTable.maxSize = 0;

    mount = 0;
    return 0;
}

// creates open file entry, opens/creates file on disk
fileDescriptor tfs_openFile(char *name){
    // create entry in file table
    int entryIdx = appendFileTable(name);
    if (entryIdx < 0)
        return entryIdx;

    // create/open inode on disk
    int inodeIdx = openInode(name, 1, 0);
    if (inodeIdx < 0)
        return inodeIdx;
    
    // update table with inode
    fileTable.table[entryIdx].inodeBlock = inodeIdx;
    
    return fileTable.table[entryIdx].fd;
}

// close file, remove entry from open file table
int tfs_closeFile(fileDescriptor FD){
    int i = searchFileTable(FD);
    if (i < 0)
        return ERR_FD_NOT_FOUND;
    fileTable.table[i].fd = 0;
    fileTable.table[i].byteOffset = 0;
    fileTable.table[i].inodeBlock = 0;
    return 0;
}

// sets content of open file on disk to buffer, removes existing content
int tfs_writeFile(fileDescriptor FD, char *buffer, int size){
    int tableIdx = searchFileTable(FD);
    if (tableIdx < 0)
        return ERR_FD_NOT_FOUND;

    else if (checkInodeExists(fileTable.table[tableIdx].inodeBlock) < 1){
        printf("Error: file descriptor points to invalid inode\n");
        return ERR_FILE_NOT_FOUND;
    }

    unsigned char inodeBlock[BLOCKSIZE];
    if (readBlock(mount, fileTable.table[tableIdx].inodeBlock, inodeBlock))
        return ERR_DISK_OPERATION;

    int retVal = deleteFileContent(fileTable.table[tableIdx].inodeBlock);
    if (retVal < 0)
        return retVal;
    
    unsigned char dataBlock[BLOCKSIZE];
    memset(dataBlock, 0, BLOCKSIZE);
    dataBlock[OFFSET_TYPE] = TYPE_D;
    dataBlock[OFFSET_MAGIC] = 0x44;
    int dataBlockSize = BLOCKSIZE-OFFSET_D_DATA;
    int dataStart = 0;

    int linkOffset = OFFSET_I_LINKS;
    while (linkOffset < BLOCKSIZE && dataStart < size){
        if ((size-dataStart) < dataBlockSize){
            dataBlockSize = size-dataStart;
            memset(dataBlock+OFFSET_D_DATA,0,BLOCKSIZE-OFFSET_D_DATA);
        }
        
        memcpy(dataBlock+OFFSET_D_DATA,buffer+dataStart,dataBlockSize);
        int freeIdx = getFreeBlock();
        if (freeIdx < 0)
            return freeIdx;

        if (writeBlock(mount, freeIdx, dataBlock))
            return ERR_DISK_OPERATION;
        
        inodeBlock[linkOffset] = freeIdx;
        dataStart = dataStart + dataBlockSize;
        memcpy(inodeBlock+OFFSET_I_SIZE, &dataStart, LEN_I_SIZE);
        if (writeBlock(mount, fileTable.table[tableIdx].inodeBlock, inodeBlock))
            return ERR_DISK_OPERATION;
        linkOffset++;
    }

    memcpy(inodeBlock+OFFSET_I_SIZE, &dataStart, LEN_I_SIZE);
    if (writeBlock(mount, fileTable.table[tableIdx].inodeBlock, inodeBlock))
        return ERR_DISK_OPERATION;

    if (dataStart < size){
        printf("Error: Inode ran out of space\n");
        return ERR_FILE_SIZE_LIMIT;
    }
    fileTable.table[tableIdx].byteOffset = 0;
    
    return 0;
}

// removes all file content and deletes inode on disk, removes parent directory link to file
int tfs_deleteFile(fileDescriptor FD){
    int tableIdx = searchFileTable(FD);
    if (tableIdx < 0)
        return ERR_FD_NOT_FOUND;

    else if (checkInodeExists(fileTable.table[tableIdx].inodeBlock) < 1){
        printf("Error: file descriptor points to invalid inode\n");
        return ERR_FILE_NOT_FOUND;
    }

    int retVal = deleteFileContent(fileTable.table[tableIdx].inodeBlock);
    if (retVal < 0)
        return retVal;

    retVal = deleteParentLinks(fileTable.table[tableIdx].filename, fileTable.table[tableIdx].inodeBlock);
    if (retVal < 0)
        return retVal;
    retVal = deleteBlock(fileTable.table[tableIdx].inodeBlock);
    if (retVal < 0)
        return retVal;
    return 0;
}

// reads a single byte from open file based on current file pointer
int tfs_readByte(fileDescriptor FD, char *buffer){
    int tableIdx = searchFileTable(FD);
    if (tableIdx < 0)
        return ERR_FD_NOT_FOUND;

    else if (checkInodeExists(fileTable.table[tableIdx].inodeBlock) < 1){
        printf("Error: file descriptor points to invalid inode\n");
        return ERR_FILE_NOT_FOUND;
    }

    unsigned char inodeBlock[BLOCKSIZE];
    if (readBlock(mount, fileTable.table[tableIdx].inodeBlock, inodeBlock))
        return ERR_DISK_OPERATION;
    uint32_t fileSize = 0;
    memcpy(&fileSize, inodeBlock+OFFSET_I_SIZE, LEN_I_SIZE);
    if (fileTable.table[tableIdx].byteOffset >= fileSize){
        printf("Error: end of file reached\n");
        return ERR_EOF;
    }

    int blockOffset = fileTable.table[tableIdx].byteOffset / (BLOCKSIZE-OFFSET_D_DATA);
    int byteOffset = fileTable.table[tableIdx].byteOffset % (BLOCKSIZE-OFFSET_D_DATA);

    unsigned char dataBlock[BLOCKSIZE];
    if (readBlock(mount, inodeBlock[OFFSET_I_LINKS+blockOffset], dataBlock))
        return ERR_DISK_OPERATION;
    
    fileTable.table[tableIdx].byteOffset++;
    buffer[0] = dataBlock[OFFSET_D_DATA + byteOffset];
    return 0;
}

// sets position of open file pointer to offset
int tfs_seek(fileDescriptor FD, int offset){
    int tableIdx = searchFileTable(FD);
    if (tableIdx < 0)
        return ERR_FD_NOT_FOUND;

    else if (checkInodeExists(fileTable.table[tableIdx].inodeBlock) < 0){
        printf("Error: file descriptor points to invalid inode\n");
        return ERR_FILE_NOT_FOUND;
    }

    fileTable.table[tableIdx].byteOffset = offset;
    return 0;
}

// EXTRA INTERFACE FUNCTIONS --------------------------------------------------

// creates a directory using absolute path
int tfs_createDir(char *dirName){
    int retVal = openInode(dirName, 1, 1) < 0;
    if (retVal < 0)
        return retVal;
    return 0;
}

// removes an empty directory inode and parent link to it
int tfs_removeDir(char *dirName){
    int dirIdx = openInode(dirName, 0, 1);
    if (dirIdx < 0)
        return dirIdx;

    unsigned char dirBlock[BLOCKSIZE];
    if (readBlock(mount, dirIdx, dirBlock))
        return ERR_DISK_OPERATION;

    int linkOffset;
    for (linkOffset=0;(linkOffset+OFFSET_I_LINKS)<BLOCKSIZE;linkOffset++){
        if (dirBlock[linkOffset+OFFSET_I_LINKS]){
            printf("Error: tfs_removeDir directory is not empty\n");
            return ERR_DIR_NONEMPTY;
        }
    }

    int retVal = deleteBlock(dirIdx);
    if (retVal < 0)
        return retVal;

    retVal = deleteParentLinks(dirName, dirIdx);
    if (retVal < 0)
        return retVal;
    return 0;
}

// recursively removes directory and all subdirectories/files
int tfs_removeAll(char *dirName){
    int dirIdx = openInode(dirName, 0, 1);
    if (dirIdx < 0)
        return dirIdx;

    // get directory inode
    unsigned char dirBlock[BLOCKSIZE];
    if (readBlock(mount, dirIdx, dirBlock))
        return ERR_DISK_OPERATION;
    if (dirBlock[OFFSET_I_DIR] != 1){
        printf("Error: tfs_removeAll input must be a directory\n");
        return ERR_FILE_NOT_FOUND;
    }

    // traverse directory
    unsigned char blockTemp[BLOCKSIZE];
    int linkOffset;
    int retVal;
    for (linkOffset=0;(linkOffset+OFFSET_I_LINKS)<BLOCKSIZE;linkOffset++){
        if (dirBlock[linkOffset+OFFSET_I_LINKS]){
            if (readBlock(mount, dirBlock[linkOffset+OFFSET_I_LINKS], blockTemp))
                return ERR_DISK_OPERATION;
            
            // traverse subdirectory and delete content recursively
            if (blockTemp[OFFSET_I_DIR]){
                char nameTemp[MAX_FILENAME+1];
                memset(nameTemp, 0, MAX_FILENAME+1);
                memcpy(nameTemp, dirName, strlen(dirName));
                if (nameTemp[strlen(nameTemp)-1] != '/')
                    nameTemp[strlen(nameTemp)] = '/';
                memcpy(nameTemp+strlen(nameTemp), blockTemp+OFFSET_I_NAME, LEN_I_NAME);
                tfs_removeAll(nameTemp);
            }
            // delete file content and inode
            else{
                retVal = deleteFileContent(dirBlock[linkOffset+OFFSET_I_LINKS]);
                if (retVal < 0)
                    return retVal;
                retVal = deleteBlock(dirBlock[linkOffset+OFFSET_I_LINKS]);
                if (retVal < 0)
                    return retVal;
            }
            dirBlock[linkOffset+OFFSET_I_LINKS] = 0;
        }
    }

    if(writeBlock(mount, dirIdx, dirBlock))
        return ERR_DISK_OPERATION;

    // don't delete root
    if (dirIdx != ROOT_BLOCK){
        retVal = tfs_removeDir(dirName);
        if (retVal < 0)
            return -1;
    }
    return 0;
}

// renames an open file, writes name in inode
int tfs_rename(fileDescriptor FD, char* newName){
    if (strlen(newName) > LEN_I_NAME || strlen(newName) == 0){
        printf("Error: invalid name\n");
        return ERR_FILENAME;
    }

    int i = searchFileTable(FD);
    if (i < 0)
        return ERR_FD_NOT_FOUND;

    // get file inode
    unsigned char blockTemp[BLOCKSIZE];
    if (readBlock(mount, fileTable.table[i].inodeBlock, blockTemp))
        return ERR_DISK_OPERATION;
    
    // set file name, write to disk
    memcpy(blockTemp+OFFSET_I_NAME, newName, strlen(newName));
    if (writeBlock(mount, fileTable.table[i].inodeBlock, blockTemp))
        return ERR_DISK_OPERATION;
    return 0;
}

// print filesystem from root
int tfs_readdir(){
    printf("(d)\t/\n");
    return readdir("/");
}

// HELPER FUNCTIONS -----------------------------------------------------------

// updates open file entry with inode location from disk
int updateFileInodeNumber(fileDescriptor fd, int inodeIdx){
    int i = searchFileTable(fd);
    if (i < 0)
        return ERR_FD_NOT_FOUND;
    fileTable.table[i].inodeBlock = inodeIdx;
    return 0;
}

// searches directory on disk for filename
// returns block index if found
// returns 0 if not found
int searchDir(char *filename, unsigned char *dirBlock){
    // search for subpath
    char testName[LEN_I_NAME+1];
    memset(testName, 0, LEN_I_NAME+1);
    unsigned char testBlock[BLOCKSIZE];
    int testBlockNum;
    int i = 0;
    do {
        testBlockNum = dirBlock[i + OFFSET_I_LINKS];
        if (testBlockNum){
            if (readBlock(mount, testBlockNum, testBlock))
                return ERR_DISK_OPERATION;
            memset(testName, 0, LEN_I_NAME+1);
            memcpy(testName, testBlock+OFFSET_I_NAME, LEN_I_NAME);
        }
        i++;
    } while (i+OFFSET_I_LINKS < BLOCKSIZE && strcmp(testName, filename));

    if (strcmp(testName, filename))
        return 0;
    else
        return testBlockNum;
}

// removes parent directory links to filename/block
int deleteParentLinks(char *filename, int blockIdx){
    // move 1 up path, remove link
    char parentPath[MAX_FILENAME+1];
    memset(parentPath, 0, MAX_FILENAME+1);
    memcpy(parentPath, filename, strlen(filename));
    int lastDelim = strlen(parentPath);
    while (lastDelim > 0 && parentPath[lastDelim] != '/')
        lastDelim--;
    
    int parentIdx = ROOT_BLOCK;
    // get path to parent
    if (lastDelim){
        parentPath[lastDelim] = '\0';
        parentIdx = openInode(parentPath, 0, 1);
        if (parentIdx < 0)
            return parentIdx;
    }

    // set references to 0
    unsigned char dirBlock[BLOCKSIZE];
    if (readBlock(mount, parentIdx, dirBlock))
        return ERR_DISK_OPERATION;
    int linkOffset;
    for (linkOffset=0;(linkOffset+OFFSET_I_LINKS)<BLOCKSIZE;linkOffset++){
        if (dirBlock[linkOffset+OFFSET_I_LINKS] == blockIdx)
            dirBlock[linkOffset+OFFSET_I_LINKS] = 0;
    }
    if (writeBlock(mount, parentIdx, dirBlock))
        return ERR_DISK_OPERATION;
    return 0;
}

// traverses directories recursively and prints all files within them
int readdir(char *dirName){
    int dirIdx = openInode(dirName, 0, 1);
    if (dirIdx < 0)
        return dirIdx;

    unsigned char dirBlock[BLOCKSIZE];
    if (readBlock(mount, dirIdx, dirBlock))
        return ERR_DISK_OPERATION;

    // print files with data first
    unsigned char blockTemp[BLOCKSIZE];
    int linkOffset;
    for (linkOffset=0;(linkOffset+OFFSET_I_LINKS)<BLOCKSIZE;linkOffset++){
        if (dirBlock[linkOffset+OFFSET_I_LINKS]){
            if (readBlock(mount, dirBlock[linkOffset+OFFSET_I_LINKS], blockTemp))
                return ERR_DISK_OPERATION;
            if (!blockTemp[OFFSET_I_DIR]){
                char nameTemp[MAX_FILENAME+1];
                memset(nameTemp, 0, MAX_FILENAME+1);
                memcpy(nameTemp, dirName, strlen(dirName));
                if (nameTemp[strlen(nameTemp)-1] != '/')
                    nameTemp[strlen(nameTemp)] = '/';
                memcpy(nameTemp+strlen(nameTemp), blockTemp+OFFSET_I_NAME, LEN_I_NAME);
                printf("(f)\t%s\n", nameTemp);
            }
        }
    }

    // print and recurse directories
    for (linkOffset=0;(linkOffset+OFFSET_I_LINKS)<BLOCKSIZE;linkOffset++){
        if (dirBlock[linkOffset+OFFSET_I_LINKS]){
            if (readBlock(mount, dirBlock[linkOffset+OFFSET_I_LINKS], blockTemp))
                return ERR_DISK_OPERATION;
            if (blockTemp[OFFSET_I_DIR]){
                char nameTemp[MAX_FILENAME+1];
                memset(nameTemp, 0, MAX_FILENAME+1);
                memcpy(nameTemp, dirName, strlen(dirName));
                if (nameTemp[strlen(nameTemp)-1] != '/')
                    nameTemp[strlen(nameTemp)] = '/';
                memcpy(nameTemp+strlen(nameTemp), blockTemp+OFFSET_I_NAME, LEN_I_NAME);
                printf("(d)\t%s\n", nameTemp);
                readdir(nameTemp);
            }
        }
    }

    return 0;
}

// searches open file table for index of file descriptor
int searchFileTable(fileDescriptor FD){
    int i = 0;
    while (i < fileTable.currSize && fileTable.table[i].fd != FD)
        i++;
    if (i >= fileTable.currSize){
        printf("Error: file not found in table\n");
        return ERR_FD_NOT_FOUND;
    }
    return i;
}

// returns inode block index on disk
// if create is set, creates new inode
// if isdir is set, looks for/creates directory inode
int openInode(char *name, int create, int isdir) {
    if (!strcmp(name, "/") && isdir)
        return ROOT_BLOCK;

    // root inode
    unsigned char dirBlock[BLOCKSIZE];
    int dirIdx = ROOT_BLOCK;
    if (readBlock(mount, dirIdx, dirBlock))
        return ERR_DISK_OPERATION;

    int pathLen = strlen(name);
    char subpath[LEN_I_NAME+1];
    int subpathStart;
    int subpathEnd = 0;
    if (name[0] != '/')
        subpathEnd = -1;
    while (subpathEnd < pathLen){
        // get start if path (subpath)
        subpathStart = subpathEnd+1;
        subpathEnd = subpathStart;
        while (name[subpathEnd] && name[subpathEnd] != '/')
            subpathEnd++;
        int subpathLen = subpathEnd-subpathStart;
        if (subpathLen > LEN_I_NAME || subpathLen == 0){
            printf("Error: invalid name\n");
            return ERR_FILENAME;
        }
        memcpy(subpath, name+subpathStart, subpathLen);
        subpath[subpathLen] = '\0';

        // search for subpath
        int pathBlockIdx = searchDir(subpath, dirBlock);
        if (pathBlockIdx < 0)
            return pathBlockIdx;

        // not found
        if (!pathBlockIdx){
            // if end of path not found, create file
            if (subpathEnd == pathLen && create)
                return createInode(subpath, isdir, dirBlock, dirIdx);
    
            printf("Error: path not found\n");
            return ERR_FILE_NOT_FOUND;
        }
        // found
        else {
            // if end of path found, open
            if (subpathEnd == pathLen){
                if (create && isdir){
                    printf("Error: file already exists\n");
                    return ERR_DIR_EXISTS;
                }
                return pathBlockIdx;
            }
            // else traverse path further
            else{
                dirIdx = pathBlockIdx;
                if (readBlock(mount, dirIdx, dirBlock))
                    return ERR_DISK_OPERATION;
                if (!dirBlock[OFFSET_I_DIR]){
                    printf("Error: path not found\n");
                    return ERR_FILE_NOT_FOUND;
                }
            }
        }
    }
    printf("Error: path not found\n");
    return ERR_FILE_NOT_FOUND;
}

// marks inode data blocks as free, removes links to data blocks
int deleteFileContent(int inodeIdx){
    if (checkInodeExists(inodeIdx) < 0){
        printf("Error: deleteFileContent invalid inode\n");
        return ERR_FILE_NOT_FOUND;
    }

    // get file inode
    unsigned char inodeBlock[BLOCKSIZE];
    if (readBlock(mount, inodeIdx, inodeBlock))
        return ERR_DISK_OPERATION;

    // remove all data
    int i;
    for (i=0;(i+OFFSET_I_LINKS)<BLOCKSIZE;i++){
        int dataIdx = inodeBlock[i+OFFSET_I_LINKS];
        if (dataIdx){
            int retVal = deleteBlock(dataIdx);
             if (retVal < 0)
                return retVal;
        }
        inodeBlock[i+OFFSET_I_LINKS] = 0;
    }

    if (writeBlock(mount, inodeIdx, inodeBlock))
        return ERR_DISK_OPERATION;

    return 0;
}

// marks a block as free and replaces free head with its index
int deleteBlock(int deleteIdx){
    if (!deleteIdx){
        printf("Error: can't delete superblock\n");
        return ERR_INVALID_BLOCK;
    }

    // get free block head from superblock
    unsigned char superblock[BLOCKSIZE];
    if (readBlock(mount, 0, superblock))
        return ERR_DISK_OPERATION;
    int freeHeadIdx = superblock[OFFSET_S_FREE];

    // setup a reference free block
    unsigned char freeBlock[BLOCKSIZE];
    memset(freeBlock, 0, BLOCKSIZE);
    freeBlock[OFFSET_TYPE] = TYPE_F;
    freeBlock[OFFSET_MAGIC] = 0x44;

    // replace free head with deleted block
    freeBlock[OFFSET_LINK] = freeHeadIdx;
    superblock[OFFSET_S_FREE] = deleteIdx;

    if (writeBlock(mount, deleteIdx, freeBlock))
        return ERR_DISK_OPERATION;
    if (writeBlock(mount, 0, superblock))
        return ERR_DISK_OPERATION;
    return 0;
}

// returns 0 if inode does not exist on disk
// returns 1 if it exists
int checkInodeExists(int inodeIdx){
    unsigned char inodeBlock[BLOCKSIZE];
    if (readBlock(mount, inodeIdx, inodeBlock))
        return ERR_DISK_OPERATION;

    if (inodeBlock[OFFSET_TYPE] == TYPE_I)
        return 1;
    
    return 0;
}

// removes a free block from the free list, returns its block number
int getFreeBlock(){
    // get free head from superblock
    unsigned char superblock[BLOCKSIZE];
    if (readBlock(mount, 0, superblock))
        return ERR_DISK_OPERATION;
    int freeHeadIdx = superblock[OFFSET_S_FREE];
    if (!freeHeadIdx){
        printf("Error: no more free blocks\n");
        return ERR_FILE_SIZE_LIMIT;
    }

    // replace head
    unsigned char freeBlockHead[BLOCKSIZE];
    if (readBlock(mount, freeHeadIdx, freeBlockHead))
        return ERR_DISK_OPERATION;
    
    superblock[OFFSET_S_FREE] = freeBlockHead[OFFSET_LINK];
    if (writeBlock(mount, 0, superblock))
        return ERR_DISK_OPERATION;
    
    return freeHeadIdx;
}

// creates inode on disk with name, under dirInode/dirIdx directory
// if isdir is set, creates directory
int createInode(char* name, int isdir, unsigned char *dirInode, int dirIdx){
    // get free link in directory
    int i = 0;
    while (i+OFFSET_I_LINKS < BLOCKSIZE && dirInode[i + OFFSET_I_LINKS])
        i++;
    if (i == BLOCKSIZE){
        printf("Error: Inode ran out of space\n");
        return ERR_FILE_SIZE_LIMIT;
    }

    // setup new inode
    unsigned char newInode[BLOCKSIZE];
    memset(newInode, 0, BLOCKSIZE);
    newInode[OFFSET_MAGIC] = 0x44;
    newInode[OFFSET_TYPE] = 2;
    newInode[OFFSET_I_DIR] = isdir;
    memcpy(newInode+OFFSET_I_NAME, name, strlen(name));

    // get a free block for inode
    int freeIdx = getFreeBlock();
    if (freeIdx < 0)
        return freeIdx;

    // write inode to free block, update directory inode
    if (writeBlock(mount, freeIdx, newInode))
        return ERR_DISK_OPERATION;
    dirInode[i + OFFSET_I_LINKS] = freeIdx;
    if (writeBlock(mount, dirIdx, dirInode))
        return ERR_DISK_OPERATION;
    
    return freeIdx;
}

// creates new entry in filetable, returns index into open file table
int appendFileTable(char *name) {
    if (strlen(name) > MAX_FILENAME || strlen(name) == 0){
        printf("Error: invalid name\n");
        return ERR_FILENAME;
    }

    // reallocate space for fileTable if needed
    if (fileTable.currSize >= fileTable.maxSize){
        fileTable.table = realloc(fileTable.table, sizeof(openFileEntry)*(fileTable.maxSize+FT_SIZE_INC));
        if (!fileTable.table){
            perror("realloc");
            return ERR_NO_MEMORY;
        }
        int i;
        for (i=0;i<FT_SIZE_INC;i++){
            fileTable.table[i+fileTable.maxSize].fd = 0;
            fileTable.table[i+fileTable.maxSize].byteOffset = 0;
        }
        fileTable.maxSize = fileTable.maxSize + FT_SIZE_INC;
    }

    // find open entry in table
    int i = 0;
    while (i < fileTable.maxSize && fileTable.table[i].fd)
        i++;

    if (i == fileTable.maxSize){
        printf("Error: No free open file entry\n"); // should never happen
        return ERR_NO_MEMORY;
    }

    // update table entry
    fileTable.table[i].fd = (fileTable.nextFd++);
    memcpy(fileTable.table[i].filename, name, strlen(name)+1);
    fileTable.currSize++;

    return i;
}

// removes an existing entry from filetable
int popFileTable(fileDescriptor fd) {
    int i = searchFileTable(fd);
    if (i < 0)
        return ERR_FD_NOT_FOUND;

    fileTable.table[i].fd = 0;
    fileTable.table[i].byteOffset = 0;
    fileTable.table[i].inodeBlock = 0;

    fileTable.currSize--;

    return 0;
}