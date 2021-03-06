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

char * getFileName(const char * path){
	char * fileName = (char*)malloc(MAX_NAME_LENGTH);
	int posInName = 0;

	for(int i = 1; i<MAX_NAME_LENGTH && path[i] != 0; i++)
		if(path[i] == '/')
			posInName = i+1;

	memset(fileName, '\0', MAX_NAME_LENGTH);
	memcpy(fileName, &path[posInName], MAX_NAME_LENGTH-posInName);

	return fileName;
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
	dirEntry * entry;
	if(strcmp(path, "/") == 0)
		_directory = root;
	else{
		entry = getEntry(&path[1]);
		if(entry == NULL)
			return -ENOENT;
		printf("\nReading Directory with path: %s at block: %d\n", &path[1], entry->idxBlock);
		_directory = *(loadDir(entry->idxBlock));
	}

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(_directory.entries[i].idxBlock > 0){
			printf("File entry of folder: %s at pos %d = %s\n", entry->name, i, _directory.entries[i].name);
			printf("File Name without path: %s\n", getFileName(_directory.entries[i].name));
			if(filler(buffer, getFileName(_directory.entries[i].name), NULL, 0) != 0){
				printf("Error in filler at readdir\n");
				return -ENOMEM;
			}
		}
	}
	printf("Before returning 0 in readdir\n");
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

    	if(entry->isDir){
    		statbuf->st_mode = S_IFDIR|0777;
    		statbuf->st_blocks = 1;
    	}
    	else{
    		statbuf->st_mode = S_IFREG|0777;
    		statbuf->st_blocks = (getFileBlocks(entry))[0];
    	}
    	statbuf->st_size = getFileSize(entry);
    }
    
    return 0;
}

int * getFileBlocks(dirEntry * entry){
	int * blockInfo = (int*)malloc(2);
	int blocks = 0, dataBlock = 0;

	fileIndexBlock * fib = (fileIndexBlock*)malloc(sizeof(fileIndexBlock));
	indexBlocks * db = (indexBlocks*)malloc(sizeof(indexBlocks));

	unsigned char * read_fib = (unsigned char*)malloc(BLOCK_SIZE);
	device_read_block(read_fib, entry->idxBlock);
	memcpy(fib, read_fib, BLOCK_SIZE);

	for(int i = 0; i<BLOCK_SIZE/sizeof(int) && fib->idxBlocks[i] > 0; i++){
		unsigned char * read_index = (unsigned char *)malloc(BLOCK_SIZE);
		device_read_block(read_index, fib->idxBlocks[i]);
		memcpy(db, read_index, BLOCK_SIZE);

		for(int j = 0; j<BLOCK_SIZE/sizeof(int) && db->dataBlocks[j] > 0; j++){
			blocks++;
			dataBlock = db->dataBlocks[j];
		}
	}

	blockInfo[0] = blocks;
	blockInfo[1] = dataBlock;

	free(fib);
	free(db);

	return blockInfo;
}

int getFileSize(dirEntry * entry){
	int * blockInfo = getFileBlocks(entry);
	printf("BLOCKS USED: %d\n", blockInfo[0]);
	int size = blockInfo[0] > 0 ? (blockInfo[0]-1)*BLOCK_SIZE : 0;

	unsigned char * read_block = (unsigned char*)malloc(BLOCK_SIZE);
	device_read_block(read_block, blockInfo[1]);

	size += strlen(read_block);
	printf("STRLEN OF READBLOCK (SIZE) = %d\n", size);
	return size;
}

int containsChar(const char * buf, char c){
	for(int i = 0; i<MAX_NAME_LENGTH && buf[i] != 0; i++)
		if(buf[i] == c)
			return 0;

	return 1;
}

int do_mknod(const char * path, mode_t mode, dev_t dev){
	printf("%s\n", __FUNCTION__);
	printf("Path: %s\n", path);
	if(S_ISREG(mode)){
		directory * dir;
		dirEntry * entry = getEntry(&path[1]);
		dirEntry * entry_of_dir;

		if(entry != NULL)
			return -EEXIST;

		int block;
		int inRoot = containsChar(&(path[1]), '/');

		if(inRoot)
			dir = &root;
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

		for(int i = 0 ; i < MAX_DIR_ENTRIES; i++){
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
			if(db->dataBlocks[j] > 0){
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
	dirEntry * entry_of_dir;
	directory * dir;
	int block, posInDir = -1, inRoot;

	if(entry == NULL)
		return -ENOENT;

	inRoot = containsChar(&(from[1]), '/');

	if(inRoot)
		dir = &root;
	else{
		char * dirName = getDirName(from);
		entry_of_dir = getEntry(dirName);

		if(entry_of_dir == NULL)
			return -ENOENT;

		block = entry_of_dir->idxBlock;
		printf("Index Block of dir: %d\n", block);
		dir = loadDir(block);
	}

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(strcmp(dir->entries[i].name, &from[1]) == 0){
			posInDir = i;
			break;
		}
	}

	if(posInDir == -1)
		return -ENOENT;

	memset(dir->entries[posInDir].name, '\0', MAX_NAME_LENGTH);
	strncpy(dir->entries[posInDir].name, &to[1], strlen(to)-1);

	if(inRoot)
		update_root_deviceSize();
	else
		updateDir(dir, block);

	memset(entry->name, '\0', MAX_NAME_LENGTH);
	strncpy(entry->name, &to[1], strlen(to)-1);

	if(entry->isDir){
		renameFilesInFolder(entry->name);
		renameInFCB(from, to);
	}

	updateFCB();
	return 0;
}

void renameInFCB(const char * from, const char * to){
	printf("\n%s\n", __FUNCTION__);
	for(int i = 0; i<MAX_DIR_ENTRIES; i++){
		char * restOfName = (char*)malloc(MAX_NAME_LENGTH);
		char * newName = (char*)malloc(MAX_NAME_LENGTH);

		memset(restOfName, '\0', MAX_NAME_LENGTH);
		memset(newName, '\0', MAX_NAME_LENGTH);

		char * substrWithPath = strstr(fcb.entries[i].name, &from[1]);
		if(substrWithPath != NULL){
			printf("SUBSTR: %s\n", substrWithPath);
			memcpy(newName, &to[1], strlen(to)-1);
			memcpy(&(newName[strlen(to)-1]), &(substrWithPath[strlen(from)-1]), strlen(substrWithPath)-strlen(from)+1);
			printf("NEW NAME: %s\n", newName);

			memset(fcb.entries[i].name, '\0', MAX_NAME_LENGTH);
			memcpy(fcb.entries[i].name, newName, strlen(newName));
		}

		free(restOfName);
		free(newName);
	}

	updateFCB();
}

void renameFilesInFolder(const char * path){
	dirEntry * entry = getEntry(path);

	if(entry == NULL)
		return;

	directory * dir = loadDir(entry->idxBlock);

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(dir->entries[i].idxBlock > 0){
			printf("Renaming some entry in folder\n");
			printf("Index Block of entry: %d\n", dir->entries[i].idxBlock);
			char * fileName = getFileName(dir->entries[i].name);
			char * newName = (char*)malloc(MAX_NAME_LENGTH);

			memset(newName, '\0', MAX_NAME_LENGTH);
			memcpy(newName, entry->name, strlen(entry->name));
			memcpy(&(newName[strlen(entry->name)]), "/", 1);
			memcpy(&(newName[strlen(entry->name)+1]), fileName, strlen(fileName));

			memcpy(dir->entries[i].name, newName, MAX_NAME_LENGTH);

			if(entry->isDir)
				renameFilesInFolder(dir->entries[i].name);

			free(fileName);
			free(newName);
		}
	}

	updateDir(dir, entry->idxBlock);
	printf("Updated dir after renaming it\n");
}

int do_unlink(const char * path){
	int i, inRoot = 1;
	dirEntry * entry = getEntry(&path[1]);
	dirEntry * entry_of_dir;
	directory * dir;

	if(entry == NULL)
		return -ENOENT;

	inRoot = containsChar(&(path[1]), '/');

	memset(entry->name, '\0', MAX_NAME_LENGTH);

	if(inRoot)
		dir = &root;
	else{
		char * dirName = getDirName(path);
		entry_of_dir = getEntry(dirName);

		if(entry_of_dir == NULL)
			return -ENOENT;

		printf("Index Block of dir: %d\n", entry_of_dir->idxBlock);
		dir = loadDir(entry_of_dir->idxBlock);
	}

	
	freeBlocks(entry);
	printf("AFTER FREE BLOCKS IN UNLINK\n");
	entry->idxBlock = 0;

	for(i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(strcmp(dir->entries[i].name, &path[1]) == 0){
			dir->entries[i] = *entry;
			break;
		}
	}

	if(inRoot)
		update_root_deviceSize();
	else
		updateDir(dir, entry_of_dir->idxBlock);

	return 0;
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

	int posInFIB = firstBlock/(BLOCK_SIZE/sizeof(int));
	int posInDB = firstBlock%(BLOCK_SIZE/sizeof(int));

	printf("POS IN FIB %d\n", posInFIB);
	printf("POS IN DB %d\n", posInDB);

	if(posInFIB >= BLOCK_SIZE/sizeof(int) || posInDB >= BLOCK_SIZE/sizeof(int))
		return -EPERM;

	unsigned char * read_index = (unsigned char*)malloc(sizeof(*dataBlock));
	device_read_block(read_index, fib->idxBlocks[posInFIB]);
	memcpy(dataBlock, read_index, sizeof(*dataBlock));
	free(read_index);

	unsigned char * blockInfo = (unsigned char*)malloc(BLOCK_SIZE);
	device_read_block(blockInfo, dataBlock->dataBlocks[posInDB]);
	memcpy(buf, blockInfo, BLOCK_SIZE);
	free(blockInfo);

	return size;
}

int do_write(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fileInfo){
	dirEntry * entry = getEntry(&path[1]);

	if(entry == NULL)
		return -ENOENT;

	fileIndexBlock * fib = (fileIndexBlock *)malloc(sizeof(fileIndexBlock));
	indexBlocks * db = (indexBlocks *)malloc(sizeof(indexBlocks));

	unsigned char * read_fib = (unsigned char*)malloc(BLOCK_SIZE);
	device_read_block(read_fib, entry->idxBlock);
	memcpy(fib, read_fib, BLOCK_SIZE);
	free(read_fib);

	int firstBlock = floor(((double)offset)/BLOCK_SIZE);
	int numBlocks = ceil(((double)size)/BLOCK_SIZE);

	
	int posInFIB = firstBlock/(BLOCK_SIZE/sizeof(int));
	if(posInFIB == 0){
		printf("\nFIRST BLOCK: %d\n", firstBlock);
		printf("NUMBLOCKS: %d\n", numBlocks);
		printf("SIZE: %ld\n", size);
	}
	
	
	int posInDB = firstBlock%(BLOCK_SIZE/sizeof(int));

	if(posInFIB == 0){
		printf("POS IN FIB %d\n", posInFIB);
		printf("POS IN DB %d\n", posInDB);
	}

	if(posInFIB >= BLOCK_SIZE/sizeof(int) || posInDB >= BLOCK_SIZE/sizeof(int))
		return -ENOSPC;

	if(fib->idxBlocks[posInFIB] == 0){
		int freeBlock = getFreeBlock();

		if(freeBlock == -1)
			return -ENOSPC;

		setBit(freeBlock, 0);
		updateBitMap();
		fib->idxBlocks[posInFIB] = freeBlock;

		unsigned char * write_fib = (unsigned char*)malloc(BLOCK_SIZE);
		memcpy(write_fib, fib, BLOCK_SIZE);
		device_write_block(write_fib, entry->idxBlock);
		free(write_fib);
	}

	unsigned char * read_index = (unsigned char*)malloc(BLOCK_SIZE);
	device_read_block(read_index, fib->idxBlocks[posInFIB]);
	memcpy(db, read_index, BLOCK_SIZE);
	free(read_index);

	if(db->dataBlocks[posInDB] == 0){
		int freeBlock = getFreeBlock();

		if(freeBlock == -1)
			return -ENOSPC;

		setBit(freeBlock, 0);
		updateBitMap();
		db->dataBlocks[posInDB] = freeBlock;

		unsigned char * write_data = (unsigned char*)malloc(BLOCK_SIZE);
		memcpy(write_data, db, BLOCK_SIZE);
		device_write_block(write_data, fib->idxBlocks[posInFIB]);
		free(write_data);
	}

	unsigned char * blockInfo = (unsigned char*)malloc(BLOCK_SIZE);
	memset(blockInfo, '\0', BLOCK_SIZE);
	device_read_block(blockInfo, db->dataBlocks[posInDB]);

	memcpy(blockInfo, buf, size);
	device_write_block(blockInfo, db->dataBlocks[posInDB]);
	free(blockInfo);
	free(fib);
	free(db);

	return size;
}

int do_mkdir(const char * path, mode_t mode){
	if(S_ISDIR(mode)){
		printf("\ndo_mkdir(path = %s)\n", path);
		int inRoot = 1, posInDir = -1, parentIdxBlock = 0;
		dirEntry * entry_of_dir;
		directory * dir;

		inRoot = containsChar(&(path[1]), '/');

		if(inRoot){
			printf("\nDIR IS ROOT\n");
			dir = &root;
		}
		else{
			char * dirName = getDirName(path);
			entry_of_dir = getEntry(dirName);

			if(entry_of_dir == NULL)
				return -ENOENT;

			printf("Index Block of dir: %d\n", entry_of_dir->idxBlock);
			dir = loadDir(entry_of_dir->idxBlock);
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
			updateDir(dir, entry_of_dir->idxBlock);
		}

		updateBitMap();
		printf("UPDATED BIT MAP\n");
		return 0;
	}
}

int do_rmdir(const char * path){
	int inRoot;
	dirEntry * entry_of_dir = getEntry(&path[1]);
	dirEntry * entry_of_parent_dir;
	directory * dir, * parentDir;

	printf("INICIALIZANDO VARIABLES\n");

	if(entry_of_dir == NULL)
		return -ENOENT;

	inRoot = containsChar(&(path[1]), '/');

	printf("VERIFICANDO SI ES ROOOT O NO\n");
	dir = loadDir(entry_of_dir->idxBlock);

	if(inRoot)
		parentDir = &root;
	else{
		char * dirName = getDirName(path);
		entry_of_parent_dir = getEntry(dirName);

		if(entry_of_parent_dir == NULL)
			return -ENOENT;

		parentDir = loadDir(entry_of_parent_dir->idxBlock);
	}

	printf("CARGANDO PARENTDIR\n");

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		printf("Posicion: %d\n", i);
		if(dir->entries[i].idxBlock > 0){
			printf("CONTIENE ARCHIVO: %s en pos: %d\n", dir->entries[i].name, i);
			freeBlocks(&dir->entries[i]);
		}
	}

	printf("ELIMANANDO LO QUE CONTENIA EL FOLDER\n");

	setBit(entry_of_dir->idxBlock, 1);
	entry_of_dir->idxBlock = 0;
	printf("ANTED DE MEMSET EN RMDIR\n");

	memset(entry_of_dir->name, '\0', MAX_NAME_LENGTH);

	for(int i = 0; i<MAX_DIR_ENTRIES/BLOCK_SIZE; i++){
		if(strcmp(parentDir->entries[i].name, &path[1]) == 0){
			parentDir->entries[i] = *entry_of_dir;
			break;
		}
	}

	if(inRoot)
		update_root_deviceSize();
	else
		updateDir(dir, entry_of_dir->idxBlock);

	updateBitMap();
	updateFCB();

	return 0;
}

int do_truncate(const char *path, off_t newSize){ return 0; }
#endif /* _FUSEFS_C_ */