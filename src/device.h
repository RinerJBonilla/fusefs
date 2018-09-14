#ifndef _DEVICE_H_
#define _DEVICE_H_

#define BLOCK_SIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fusefs.h"

static FILE * f;

void create_new_disk(const char * path, long deviceSize);
void device_open(const char * path, long deviceSize);
void device_close();
int device_read_block(unsigned char buffer[], int block);
int device_write_block(unsigned char buffer[], int block);
void device_flush();

#endif /* _DEVICE_H_ */