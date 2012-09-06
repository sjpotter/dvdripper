#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv) {
	int		fd,
			i,
			block,
			blocksize,
			bcount;
	struct stat	st;

	assert(argv[1] != NULL);

	assert(fd=open(argv[1], O_RDONLY));

	assert(ioctl(fd, FIGETBSZ, &blocksize) == 0);

	assert(!fstat(fd, &st));

	bcount = (st.st_size + blocksize - 1) / blocksize;

//	printf("File: %s Size: %d Blocks: %d Blocksize: %d\n", 
//		argv[1], st.st_size, bcount, blocksize);

	block=0;
	if (ioctl(fd, FIBMAP, &block)) {
		printf("FIBMAP ioctl failed - errno: %s\n",
                                        strerror(errno));
	}
	printf("%d\n", block);
	/* for(i=0;i < bcount;i++) {
		block=i;
		if (ioctl(fd, FIBMAP, &block)) {
			printf("FIBMAP ioctl failed - errno: %s\n",
					strerror(errno));
		}
		printf("%3d %10d\n", i, block);
	} */

	close(fd);
}

