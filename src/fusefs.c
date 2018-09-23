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

void updateDir(directory * dir, int block){
	unsigned char * write_dir = (unsigned char *)malloc(sizeof(*dir));
	memcpy(write_dir, dir, sizeof(*dir));
	device_write_block(write_dir, block);
	free(write_dir);
}

char * getDirName(const char * path){
	int dirNameLength = -1;

	for(int i = 1; i<MAX_NAME_LENGTH && path[i] != 0; i++)
		if(path[i] == '/')
			dirNameLength = i;

	if(dirNameLength == -1)
		return NULL;

	char * dirName = (char*)malloc(MAX_NAME_LENGTH);
	memset(dirName, '\0', MAX_NAME_LENGTH);
	memcpy(dirName, &path[1], dirNameLength-1);
	printf("\nDirName = %s\n", dirName);

	return dirName;
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
		printf("\nReading Directory with path: %s at block: %d\n", &path[1], entry->idxBlock);
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
    dirEntry * entry;

    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_ino = 0;
    statbuf->st_nlink = 1;
    statbuf->st_blksize = BLOCK_SIZE;

    if(((strlen(path) == 1) && path[0] == '/')){
    	statbuf->st_mode = S_IFDIR|0777;
    	statbuf->st_size = BLOCK_SIZE;
    	statbuf->st_blocks = 1;
    }
    else{
    	entry = getEntry(&path[1]);

    	if(entry == NULL)
    		return -ENOENT;

    	if(entry->isDir)
    		statbuf->st_mode = S_IFDIR|0777;
    	else
    		statbuf->st_mode = S_IFREG|0777;
    	statbuf->st_size = entry->size;
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
		dirEntry * entry_of_dir;
		if(inRoot){
			dir = &root;
			i = 0;
		}
		else{
			char * dirName = getDirName(path);
			entry_of_dir = getEntry(dirName);

			if(entry_of_dir == NULL)
				return -ENOENT;

			block = entry_of_dir->idxBlock;
			printf("Index Block of dir: %d\n", block);
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
		strcpy(dir->entries[j].name, &path[1]);
		dir->entries[j].isDir = 0;
		dir->entries[j].size = 0;
		dir->entries[j].idxBlock = freeBlock;
		setBit(dir->entries[j].idxBlock, 0);
		updateBitMap();

		printf("Created file %s at block %d in folder %s\n", dir->entries[j].name, freeBlock, entry_of_dir->name);

		for(i = 0 ; i < MAX_DIR_ENTRIES; i++){
			if(strcmp(fcb.entries[i].name, "\0") == 0){
				fcb.entries[i] = dir->entries[j];
				updateFCB();
				break;
			}
		}

		fileIndexBlock firstLevel;
		for(int j = 0; j<BLOCK_SIZE/sizeof(int); j++)
			firstLevel.idxBlocks[j] = 0;

		if(inRoot)
			update_root_deviceSize();
		else
			updateDir(dir, block);

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

	for(int i = 0; i<BLOCK_SIZE/sizeof(int); i++){
		if(firstLevel->idxBlocks[i] == 0)
			break;

		indexBlocks * db = (indexBlocks*)malloc(sizeof(indexBlocks));
		unsigned char * read_index = (unsigned char*)malloc(sizeof(*db));
		device_read_block(read_index, firstLevel->idxBlocks[i]);
		memcpy(db, read_index, sizeof(*db));
		free(read_index);

		for(int j = 0; j<BLOCK_SIZE/sizeof(int); j++){
			if(db->dataBlocks[j] != 0){
				setBit(db->dataBlocks[j], 1);
				db->dataBlocks[j] = 0;
			}
		}

		firstLevel->idxBlocks[i] = 0;
		free(db);
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

int do_read(const char * path, char * buf, size_t size, off_t offset, struct fuse_file_info * fileInfo){
	dirEntry * entry = getEntry(&path[1]);
	printf("Reading file %s writen at %d\n", entry->name, entry->idxBlock);

	if(entry == NULL)
		return -ENOENT;

	unsigned char * info = (unsigned char*)calloc(1, (int)size);
	fileIndexBlock * fib = (fileIndexBlock *)malloc(sizeof(fileIndexBlock));
	indexBlocks * dataBlock = (indexBlocks *)malloc(sizeof(indexBlocks));

	unsigned char * read_fib = (unsigned char*)malloc(sizeof(*fib));
	device_read_block(read_fib, entry->idxBlock);
	memcpy(fib, read_fib, sizeof(*fib));
	free(read_fib);

	int firstBlock = floor(((double)offset)/BLOCK_SIZE);
	int numBlocks = ceil(((double)size)/BLOCK_SIZE);
	int readBlocks = 0, offsetInFile = 0, finished = 0;


	for(int i = 0; i<BLOCK_SIZE/sizeof(int); i++){
		if(fib->idxBlocks[i] != 0){
			unsigned char * read_index = (unsigned char*)malloc(sizeof(*dataBlock));
			device_read_block(read_index, fib->idxBlocks[0]);
			memcpy(dataBlock, read_index, sizeof(*dataBlock));
			free(read_index);

			for(int j = 0; readBlocks<numBlocks && j<BLOCK_SIZE/sizeof(int); j++, readBlocks++){
				if(dataBlock->dataBlocks[j] != 0){
					unsigned char * blockInfo = (unsigned char*)malloc(BLOCK_SIZE);
					device_read_block(blockInfo, dataBlock->dataBlocks[j]);
					memcpy(&info[offsetInFile], blockInfo, BLOCK_SIZE);
					free(blockInfo);

					offsetInFile += BLOCK_SIZE;
				}
				else{
					finished = 1;
					break;
				}
			}

			if(finished)
				break;
		}
		else
			break;
	}

	memcpy(buf, info, size);
	free(info);
	free(fib);
	free(dataBlock);

	return size;
}

int do_write(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fileInfo){
	dirEntry * entry = getEntry(&path[1]);
	printf("WRITE PATH: %s\n", path);
	printf("Writing to File %s at %d\n", entry->name, entry->idxBlock);

	if(entry == NULL)
		return -ENOENT;

	fileIndexBlock * fib = (fileIndexBlock *)malloc(sizeof(fileIndexBlock));
	indexBlocks * db = (indexBlocks *)malloc(sizeof(indexBlocks));

	unsigned char * read_fib = (unsigned char*)malloc(sizeof(*fib));
	device_read_block(read_fib, entry->idxBlock);
	memcpy(fib, read_fib, sizeof(*fib));
	free(read_fib);

	int firstBlock = floor(((double)offset)/BLOCK_SIZE);
	int numBlocks = ceil(((double)size)/BLOCK_SIZE);

	printf("\nFIRST BLOCK: %d\n", firstBlock);
	printf("NUMBLOCKS: %d\n", numBlocks);
	int writtenBlocks = 0;

	for(int i = 0; i<BLOCK_SIZE/sizeof(int); i++){
		if(fib->idxBlocks[i] == 0 && writtenBlocks < numBlocks){
			int freeBlock = getFreeBlock();

			if(freeBlock == -1)
				return -ENOSPC;

			setBit(freeBlock, 0);
			updateBitMap();
			fib->idxBlocks[i+firstBlock] = freeBlock;

			unsigned char * write_fib = (unsigned char*)malloc(sizeof(*fib));
			memcpy(write_fib, fib, sizeof(*fib));
			device_write_block(write_fib, entry->idxBlock);
			free(write_fib);
			printf("Allocated new FreeBlock for FIB for file at %d in position %d at block %d\n", entry->idxBlock, i, freeBlock);
		}

		if(fib->idxBlocks[i] != 0){
			printf("Not null\n");
			unsigned char * read_index = (unsigned char*)malloc(sizeof(*db));
			device_read_block(read_index, fib->idxBlocks[i]);
			memcpy(db, read_index, sizeof(*db));
			free(read_index);
			printf("Reading IndexBlocks in pos %d of FIB at %d\n", i, entry->idxBlock);

			for(int j = 0; j < BLOCK_SIZE/sizeof(int) && writtenBlocks < numBlocks; j++, writtenBlocks++){
				if(db->dataBlocks[j+firstBlock] == 0){
					int freeBlock = getFreeBlock();

					if(freeBlock == -1)
						return -ENOSPC;

					setBit(freeBlock, 0);
					updateBitMap();
					db->dataBlocks[j+firstBlock] = freeBlock;

					unsigned char * write_data = (unsigned char*)sizeof(*db);
					memcpy(write_data, db, sizeof(*db));
					device_write_block(write_data, fib->idxBlocks[i]);
					free(write_data);
					printf("Allocated space for Indexblock in position %d at %d\n", j, fib->idxBlocks[i]);
				}

				unsigned char * blockInfo = (unsigned char*)malloc(BLOCK_SIZE);
				memset(blockInfo, '\0', BLOCK_SIZE);

				device_read_block(blockInfo, db->dataBlocks[j+firstBlock]);
				int offsetInFile = (int)offset;

				if(offsetInFile >= (BLOCK_SIZE * (j+firstBlock)))
					offsetInFile -= BLOCK_SIZE * (j+firstBlock);

				memcpy(&blockInfo[offsetInFile], buf, size);
				device_write_block(blockInfo, db->dataBlocks[j+firstBlock]);
				free(blockInfo);
			}

			if(writtenBlocks >= numBlocks)
				break;
		}
		else
			break;
	}

	free(fib);
	free(db);
	return size;
}

int do_mkdir(const char * path, mode_t mode){
	printf("\ndo_mkdir(path = %s)\n", path);
	int inRoot = 1, posInDir = -1, parentIdxBlock = 0;
	directory * dir;

	for(int i = 1; i<path[i]; i++){
		if(path[i] == '/'){
			inRoot = 0;
			break;
		}
	}

	if(inRoot){
		printf("\nDIR IS ROOT\n");
		dir = &root;
		parentIdxBlock = 0;
	}
	else{

	}

	for(int i = 0; i < MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(dir->entries[i].idxBlock == 0){
			posInDir = i;
			printf("PosInDir = %d\n", posInDir);
			break;
		}
	}

	int freeBlock = getFreeBlock();
	printf("FREE BLOCK FOR DIR = %d\n", freeBlock);
	if(posInDir == -1 || freeBlock == -1){
		printf("NO SPACE LEFT\n");
		return -ENOSPC;
	}

	strcpy(dir->entries[posInDir].name, &path[1]);
	dir->entries[posInDir].isDir = 1;
	dir->entries[posInDir].size = 0;
	dir->entries[posInDir].idxBlock = freeBlock;
	setBit(dir->entries[posInDir].idxBlock, 0);

	printf("ENTRY OF ROOT AT %d is %s\n", posInDir, root.entries[posInDir].name);

	directory newDir;
	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++)
		newDir.entries[i].idxBlock = 0;

	updateDir(&newDir, dir->entries[posInDir].idxBlock);

	for(int i = 0 ; i < MAX_DIR_ENTRIES; i++){
		if(fcb.entries[i].idxBlock == 0){
			fcb.entries[i] = dir->entries[posInDir];
			updateFCB();
			break;
		}
	}

	printf("WROTE NEW DIR AT BLOCK %d\n", dir->entries[posInDir].idxBlock);

	if(inRoot){
		update_root_deviceSize();
		printf("UPDATED ROOT\n");
	}
	else{

	}

	updateBitMap();
	printf("UPDATED BIT MAP\n");
	return 0;
}

#endif /* _FUSEFS_C_ */