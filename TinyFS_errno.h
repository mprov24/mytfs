#ifndef TINYFS_ERRNO_H
#define TINYFS_ERRNO_H

#define ERR_DISK_OPERATION  -2      // error in disk driver

#define ERR_NO_MEMORY       -3      // ran out of memory for allocating

#define ERR_FILE_NOT_FOUND  -4      // inode not found on disk
#define ERR_FD_NOT_FOUND    -5      // file descriptor not in open file table

#define ERR_FILE_SIZE_LIMIT -6      // ran out of space in inode or filesystem
#define ERR_INVALID_FS_SIZE -7      // file system size invalid to mount tinyFS
#define ERR_EOF             -8      // end of file hit while reading

#define ERR_FS_INTEGRITY    -9      // filesystem has invalid blocks, cannot mount
#define ERR_FILENAME        -10     // filename or path is badly formatted
#define ERR_INVALID_BLOCK   -11     // trying to write to invalid block (superblock or root)

#define ERR_DIR_EXISTS      -12     // cannot create directory if it exists
#define ERR_DIR_NONEMPTY    -13     // cannot remove non empty directory

#endif