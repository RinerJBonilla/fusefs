SRC='./src'
gcc $SRC/fusefs.c $SRC/device.c $SRC/bitmap.c $SRC/main.c -o ./bin/fusefs `pkg-config fuse --cflags --libs`