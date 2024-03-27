# Tiny File System

A hierarchical file system implementation utilizing standard UNIX files for mounting.

A client program will call tiny file system functions "tfs_[function]" to interact with the file system. These functions call block device driver functions for reading and writing blocks to the mounted UNIX file.

![image](https://github.com/mprov24/mytfs/assets/149441123/6b2f7a35-c4eb-4aba-afa3-89853c7fce88)

Files:
- TFS: libTinyFS.c
- Block driver: libDisk.c
- Tests: tinyFSDemo.c

## Implementation notes
- The superblock and root inode are the only required blocks
- When a function is used incorrectly and causes a return value of an error number, a message will print to stdout on what caused that error
- Free blocks are implemented as a chain of blocks starting at the superblock to reduce external fragmentation
- The superblock contains the maximum block size of the file system, so that tfs_mount can verify all blocks
- File inodes contain direct indexes to file extent blocks so that all file data can be quickly accessed
- The open file table dynamically grows by increments of 100 entries and is deallocated upon tfs_unmount() for unlimited opens
- Opening a file multiple times will create new open file entries and new file descriptors, but will point to the same inode on the disk
- tfs_deleteFile will delete an inode and all the data associated with it, setting them as free
- Seeking past the end of the file is allowed, but reading past EOF will return an errno

![blocks drawio](https://github.com/mprov24/mytfs/assets/149441123/4411828a-0533-4e16-b133-35a9c90be518)

## Additional features
- Inodes have a byte for if they are a directory or not. If it is a directory, direct blocks point to other inodes, otherwise they point to file extent blocks
- All functions use absolute paths, except tfs_rename because it is just setting the 8 name bytes in an inode block
- All paths can optionally start with "/"
- tfs_removeDir will not remove nonempty directories
- tfs_removeAll deletes a directory and everything under it. tfs_removeAll("/") will delete all blocks except the root inode and superblock which are required
- tfs_readdir recursively prints all file paths and then directory paths for ease of viewing. (f) indicates a file and (d) indicates a directory

## Limitations
- Making and mounting tinyFS requires a size of 2 blocks to 255 blocks. Two blocks are needed for the superblock and root inode, more than 255 blocks would require more than 1 byte to index other blocks
- You can open a directory as a file to rename it, but must not write or read a directory inode
