#ifndef _DEVICE_C_
#define _DEVICE_C_

#include "device.h"

void create_new_disk(const char * path, long deviceSize){
	f = fopen(path, "w+");
	int i = 0, emptyBlocksSize = bitMapBlocks*BLOCK_SIZE/sizeof(uint32_t);
	uint32_t empty_blocks[emptyBlocksSize];
	int temp_bitMapBlocks = bitMapBlocks;

	while(temp_bitMapBlocks >= 32){
		empty_blocks[i++] = 0x0;
		temp_bitMapBlocks -= 32;
	}

	for(int j = 0; j<160; j++)
		empty_blocks[j+i] = 0x0;
	i += 160;

	if(temp_bitMapBlocks != 0){
		empty_blocks[i] = 0xFFFFFFFF;
		empty_blocks[i] = empty_blocks[i] << (temp_bitMapBlocks+1);
		i++;
	}

	for(int j = i; j<emptyBlocksSize; j++)
		empty_blocks[j] = 0xFFFFFFFF;

	unsigned char * write_root = (unsigned char*)malloc(sizeof(root));
	memcpy(write_root, &deviceSize, sizeof(long));
	memcpy(write_root+sizeof(long), &root, sizeof(root));
	device_write_block(write_root, 0);
	free(write_root);

	unsigned char * write_map = (unsigned char*)malloc(emptyBlocksSize*sizeof(uint32_t));
	unsigned char * map_start = write_map;
	memcpy(write_map, empty_blocks, emptyBlocksSize*sizeof(uint32_t));

	for(int j = 1; j<bitMapBlocks+1; j++){
		device_write_block(write_map, j);
		write_map += BLOCK_SIZE;
	}

	write_map = map_start;
	free(write_map);

	unsigned char * write_FCB = (unsigned char*)malloc(sizeof(fcb));
	unsigned char * FCB_start = write_FCB;
	memcpy(write_FCB, &fcb, sizeof(fcb));

	for(int j = bitMapBlocks+1; j < bitMapBlocks+FCB_BLOCKS+1; j++){
		device_write_block(write_FCB, j);
		write_FCB += BLOCK_SIZE;
	}

	write_FCB = FCB_start;
	free(write_FCB);
}

void device_open(const char * path, long deviceSize){
	f = fopen(path, "r+");

	if(f == NULL){
		printf("\nCreating new disk with size: %ld\n", deviceSize);
		create_new_disk(path, deviceSize);
	}
	else
		printf("\nReading from device %s\n", path);

	load_root_deviceSize();
	initBitMap();
	loadFCB();
}

void device_close(){
	fflush(f);
	fclose(f);
}

int device_read_block(unsigned char buffer[], int block){
	fseek(f, block*BLOCK_SIZE, SEEK_SET);
	return (fread(buffer, 1, BLOCK_SIZE, f) == BLOCK_SIZE);
}

int device_write_block(unsigned char buffer[], int block){
	fseek(f, block*BLOCK_SIZE, SEEK_SET);
	return (fwrite(buffer, 1, BLOCK_SIZE, f) == BLOCK_SIZE);
}

void device_flush(){
	fflush(f);
}

#endif /* _DEVICE_C_ */
