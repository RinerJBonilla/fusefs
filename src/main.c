#include "fusefs.h"

static struct fuse_operations fuseops = {
	.getattr = do_getattr,
	.readdir = do_readdir,
	.mknod = do_mknod,
	.rename = do_rename,
	.unlink = do_unlink
};

int main(int argc, char * argv[]){
	double device_size_param = atof(argv[2]);
	deviceSize = device_size_param*(1024*1024*1024);
	bitMapBlocks = deviceSize/BLOCK_SIZE/8/BLOCK_SIZE;

	device_open(argv[1], deviceSize);

	printf("\nDevice Size: %ld\n", deviceSize);
	printf("Blocks for BitMap: %d\n", bitMapBlocks);
	printf("Blocks for FCB: %d\n\n", FCB_BLOCKS);

	for(int i = 0; i<2; i++){
		for(int j = 1; j<argc; j++)
			argv[j] = argv[j+1];
		argc--;
	}

	int fuse_stat = fuse_main(argc, argv, &fuseops, NULL);
	device_close();

	return fuse_stat;
}