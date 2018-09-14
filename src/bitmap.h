#ifndef _BITMAP_H_
#define _BITMAP_H_

#define BITS_PER_WORD 32

#include <math.h>
#include "device.h"

extern uint32_t * bitMap;
extern long deviceSize;

int wordOffset(int n);
int bitOffset(int n);
int checkBit(int word, int n);
void setBit(int n, int value);
int getFreeBlock();

#endif /* _BITMAP_H_ */