#ifndef _FUSEFS_H_
#define _FUSEFS_H_

#define ENTRY_SIZE (int)sizeof(dirEntry)
#define MAX_NAME_LENGTH 256

#define MAX_BLOCKS_PER_FILE (int)((BLOCK_SIZE*BLOCK_SIZE)/(sizeof(int)*sizeof(int)))
#define MAX_FILE_SIZE (int)(MAX_BLOCK_PER_FILE*BLOCK_SIZE)
#define MAX_DIR_ENTRIES (int)((20480 * 1024)/ENTRY_SIZE)
#define FCB_BLOCKS (int)((20480 * 1024)/BLOCK_SIZE)

#define FUSE_USE_VERSION 30

#include "device.h"
#include "bitmap.h"
#include <fuse.h>
#include <errno.h>

typedef struct{
	char name[MAX_NAME_LENGTH];
	char isDir;
	int size;
	long createdAt;
	long updatedAt;
	int idxBlock;
} dirEntry;

typedef struct{
	dirEntry entries[MAX_DIR_ENTRIES];
} FCB;

typedef struct{
	dirEntry entries[MAX_DIR_ENTRIES/BLOCK_SIZE];
} directory;

typedef struct {
	int dataBlocks[BLOCK_SIZE/sizeof(int)];
} indexBlocks;

typedef struct {
	indexBlocks idxBlocks[BLOCK_SIZE/sizeof(int)];
} fileIndexBlock;

int bitMapBlocks;
long deviceSize;
uint32_t * bitMap;
directory root;
directory _directory;
FCB fcb;

void initBitMap();
void updateBitMap();
void loadFCB();
void updateFCB();
void load_root_deviceSize();
void update_root_deviceSize();
void freeBlocks(dirEntry * entry);
directory * loadDir(int n);
dirEntry * getEntry(const char * name);

int do_readdir(const char * path, void * buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi);
int do_getattr(const char * path, struct stat * statbuf);
int do_mknod(const char * path, mode_t mode, dev_t dev);
int do_unlink(const char * path);
int do_rename(const char * from, const char * to);
int do_unlink(const char * path);

#endif /* _FUSEFS_H_ */