#ifndef _BITMAP_C_
#define _BITMAP_C_

#include "bitmap.h"

int wordOffset(int n){
	return n/BITS_PER_WORD;
}

int bitOffset(int n){
	return n%BITS_PER_WORD;
}

int checkBit(int word, int n){
	return (word >> n) & 1;
}

void setBit(int n, int value){
	if(value)
		bitMap[wordOffset(n)] |= (1 << bitOffset(n));
	else
		bitMap[wordOffset(n)] &= ~(1 << bitOffset(n));
}

int getFreeBlock(){
	int i = 0, j = 0;

	while(bitMap[i] == 0 && i < BLOCK_SIZE*bitMapBlocks) i++;
	while(!checkBit(bitMap[i], j) && j < BITS_PER_WORD) j++;

	if(!checkBit(bitMap[i], j) || (BITS_PER_WORD*i+j) >= floor(deviceSize/BLOCK_SIZE))
		return -1;

	return BITS_PER_WORD*i + j;
}

#endif /* _BITMAP_C_ */