#ifndef _FUSEFS_C_
#define _FUSE_FS_C

#include "fusefs.h"

void initBitMap(){
	bitMapBlocks = 0;
	bitMapBlocks = deviceSize/BLOCK_SIZE/8/BLOCK_SIZE;
	bitMap = (uint32_t*)malloc(BLOCK_SIZE*bitMapBlocks);
	unsigned char * read_bitMap = (unsigned char*)malloc(bitMapBlocks*BLOCK_SIZE);
	unsigned char * bitMap_start = read_bitMap;

	for(int i = 1; i<bitMapBlocks+1; i++){
		device_read_block(read_bitMap, i);
		read_bitMap += BLOCK_SIZE;
	}

	read_bitMap = bitMap_start;
	memcpy(bitMap, read_bitMap, bitMapBlocks*BLOCK_SIZE);
	free(read_bitMap);
}

void updateBitMap(){
	unsigned char * write_bitMap = (unsigned char*)malloc(bitMapBlocks*BLOCK_SIZE);
	unsigned char * bitMap_start = write_bitMap;
	memcpy(write_bitMap, bitMap, bitMapBlocks*BLOCK_SIZE);

	for(int i = 1; i < bitMapBlocks+1; i++){
		device_write_block(write_bitMap, i);
		write_bitMap += BLOCK_SIZE;
	}

	write_bitMap = bitMap_start;
	free(write_bitMap);
}

void loadFCB(){
	unsigned char * read_FCB = (unsigned char *)malloc(FCB_BLOCKS*BLOCK_SIZE);
	unsigned char * FCB_start = read_FCB;

	for(int i = bitMapBlocks+1; i<bitMapBlocks+FCB_BLOCKS+1; i++){
		device_read_block(read_FCB, i);
		read_FCB += BLOCK_SIZE;
	}

	read_FCB = FCB_start;
	memcpy(&fcb, read_FCB, FCB_BLOCKS*BLOCK_SIZE);
	free(read_FCB);
	printf("FCB entries\n");

	for(int i = 0; i<MAX_DIR_ENTRIES; i++){
		if(strcmp(fcb.entries[i].name, "\0") == 0)
			break;
		printf("Entry at %d = %s\n", i, fcb.entries[i].name);
	}
}

void updateFCB(){
	unsigned char * write_FCB = (unsigned char*)malloc(FCB_BLOCKS*BLOCK_SIZE);
	unsigned char * FCB_start = write_FCB;
	memcpy(write_FCB, &fcb, FCB_BLOCKS*BLOCK_SIZE);

	for(int i = bitMapBlocks+1; i<bitMapBlocks+FCB_BLOCKS+1; i++){
		device_write_block(write_FCB, i);
		write_FCB += BLOCK_SIZE;
	}

	write_FCB = FCB_start;
	free(write_FCB);
}

void load_root_deviceSize(){
	deviceSize = 0;
	unsigned char * read_root = (unsigned char*)malloc(sizeof(root)+sizeof(long));
	device_read_block(read_root, 0);
	memcpy(&deviceSize, read_root, sizeof(long));
	memcpy(&root, read_root+sizeof(long), sizeof(root));
	free(read_root);
	printf("Entry at 0 of root: %s\n", root.entries[0].name);
}

void update_root_deviceSize(){
	unsigned char * write_root = (unsigned char*)malloc(sizeof(root)+sizeof(long));
	memcpy(write_root, &deviceSize, sizeof(long));
	memcpy(write_root+sizeof(long), &root, sizeof(root));
	device_write_block(write_root, 0);
	free(write_root);
}

directory * loadDir(int n){
	directory * dir = (directory *)malloc(sizeof(directory));

	unsigned char * read_dir = (unsigned char*)malloc(sizeof(*dir));
	device_read_block(read_dir, n);

	memcpy(dir, read_dir, sizeof(*dir));
	free(read_dir);

	return dir;
}

dirEntry * getEntry(const char * name){
	for(int i = 0; i<MAX_DIR_ENTRIES; i++){
		dirEntry * entry = &(fcb.entries[i]);

		if(strcmp(entry->name, name) == 0)
			return entry;
	}

	return NULL;
}

int do_readdir(const char * path, void * buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi){
	if(strcmp(path, "/") == 0)
		_directory = root;

	else{
		dirEntry * entry = getEntry(&path[1]);
		if(entry == NULL)
			return -ENOENT;
		_directory = *(loadDir(entry->idxBlock));
	}

	for(int i = 0; i<MAX_DIR_ENTRIES; i++)
		if(_directory.entries[i].idxBlock != 0)
			if(filler(buffer, _directory.entries[i].name, NULL, 0) != 0)
				return -ENOMEM;

	return 0;
}

int do_getattr(const char * path, struct stat * statbuf){
    printf("%s: %s\n", __FUNCTION__, path);
    
    if ( (strlen(path) == 1) && path[0] == '/') {
        statbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_nlink = 1;
        statbuf->st_ino = 0;
        statbuf->st_size = BLOCK_SIZE;
        statbuf->st_blksize = BLOCK_SIZE;
        statbuf->st_blocks = 1;
    } else {
        dirEntry * entry = getEntry(&path[1]);
        int size, block_count;
        
        if (entry == NULL)
            return -ENOENT;
        
        statbuf->st_mode = (S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO) & ~S_IXUSR & ~S_IXGRP & ~S_IXOTH;
        statbuf->st_nlink = 1;
        statbuf->st_ino = 0;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_size = entry->size;
        statbuf->st_blksize = BLOCK_SIZE;
        statbuf->st_blocks = 0;
    }
    
    return 0;
}

int do_mknod(const char * path, mode_t mode, dev_t dev){
	printf("%s\n", __FUNCTION__);
	printf("Path: %s\n", path);
	if(S_ISREG(mode)){
		directory * dir;
		dirEntry * entry = getEntry(&path[1]);

		if(entry != NULL)
			return -EEXIST;

		int i, inRoot = 1;

		for(i = 1; i<MAX_NAME_LENGTH+1 && path[i] != 0; i++){
			if(path[i] == '/'){
				inRoot = 0;
				break;
			}
		}

		int block;
		if(inRoot){
			dir = &root;
			i = 0;
		}
		else{
			char dirName[MAX_NAME_LENGTH+1];
			memset(dirName, '\0', MAX_NAME_LENGTH+1);
			memcpy(&dirName, path, i);

			if(entry == NULL)
				return -ENOENT;

			block = entry->idxBlock;
			dir = loadDir(block);
		}

		int j;
		for(j = 0; j<MAX_DIR_ENTRIES; j++)
			if(dir->entries[j].idxBlock == 0)
				break;

		if(j >= MAX_DIR_ENTRIES)
			return -ENOSPC;

		int freeBlock = getFreeBlock();
		printf("\nFree Block: %d\n", freeBlock);
		strcpy(dir->entries[j].name, &path[i+1]);
		dir->entries[j].isDir = 0;
		dir->entries[j].size = 0;
		dir->entries[j].idxBlock = freeBlock;
		setBit(dir->entries[j].idxBlock, 0);
		updateBitMap();

		for(i = 0 ; i < MAX_DIR_ENTRIES; i++){
			if(strcmp(fcb.entries[i].name, "\0") == 0){
				fcb.entries[i] = dir->entries[j];
				updateFCB();
				break;
			}
		}

		fileIndexBlock firstLevel;
		for(int j = 0; j<BLOCK_SIZE/sizeof(int); j++){
			indexBlocks idxBlocks;
			for(int k = 0; k<BLOCK_SIZE/sizeof(int); k++)
				idxBlocks.dataBlocks[k] = 0;
			firstLevel.idxBlocks[j] = idxBlocks;
		}

		if(inRoot)
			update_root_deviceSize();
		else{
			unsigned char * write_dir = (unsigned char*)malloc(sizeof(*dir));
			memcpy(write_dir, dir, sizeof(*dir));
			device_write_block(write_dir, block);
			free(write_dir);
		}

		unsigned char * write_index = (unsigned char *)malloc(sizeof(firstLevel));
		memcpy(write_index, &firstLevel, sizeof(firstLevel));
		device_write_block(write_index, dir->entries[j].idxBlock);
		free(write_index);

		return 0;
	}
	return -EPERM;
}

void freeBlocks(dirEntry * entry){
	fileIndexBlock * firstLevel = (fileIndexBlock*)malloc(sizeof(fileIndexBlock));
	unsigned char * index_write = (unsigned char *)malloc(sizeof(*firstLevel));
	device_read_block(index_write, entry->idxBlock);

	memcpy(firstLevel, index_write, sizeof(*firstLevel));
	free(index_write);

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		indexBlocks db = firstLevel->idxBlocks[i];

		for(int j = 0; j<BLOCK_SIZE/sizeof(int); j++){
			if(db.dataBlocks[j] != 0){
				setBit(db.dataBlocks[j], 1);
				db.dataBlocks[j] = 0;
			}
		}

		firstLevel->idxBlocks[i] = db;
	}

	free(firstLevel);
	setBit(entry->idxBlock, 1);
	updateBitMap();
}

int do_rename(const char * from, const char * to){
	dirEntry * entry = getEntry(&from[1]);

	if(entry == NULL)
		return -ENOENT;
	int posInDir = -1, inRoot = 1;

	for(int i = 1; i<MAX_NAME_LENGTH && from[i] != 0; i++){
		if(from[i] == '/'){
			inRoot = 0;
			break;
		}
	}

	if(inRoot){
		for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
			if(strcmp(root.entries[i].name, &from[1]) == 0){
				posInDir = i;
				break;
			}
		}

		if(posInDir == -1)
			return -ENOENT;

		strncpy(root.entries[posInDir].name, &to[1], strlen(from));
		update_root_deviceSize();
	}
	else{

	}

	strncpy(entry->name, &to[1], strlen(from));
	updateFCB();
}

int do_unlink(const char * path){
	int i, inRoot = 1;
	dirEntry * entry = getEntry(&path[1]);

	for(i = 1; i<MAX_NAME_LENGTH && path[i] != 0; i++){
		if(path[i] == '/'){
			inRoot = 0;
			break;
		}
	}

	if(entry == NULL)
		return -ENOENT;

	memset(entry->name, '\0', MAX_NAME_LENGTH);

	if(inRoot){
		freeBlocks(entry);
		entry->idxBlock = 0;

		for(i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
			if(strcmp(root.entries[i].name, &path[1]) == 0){
				root.entries[i] = *entry;
				break;
			}
		}

		update_root_deviceSize();
		return 0;
	}
	else{

	}

	for(i = 0; i<MAX_DIR_ENTRIES; i++){
		if(strcmp(fcb.entries[i].name, &path[1]) == 0){
			fcb.entries[i] = *entry;
			updateFCB();
			break;
		}
	}
}

#endif /* _FUSEFS_C_ */