#include <stdio.h>
#include <dvdcss/dvdcss.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE 1
#define _LARGEFILE_SOURCE 1

int my_fwrite(void * buffer, size_t size, size_t number, FILE * stream)
{
   int wrote = number;
   int count;
   
   while (wrote > 0) {
      count = fwrite(buffer, size, number, stream);
      if (count < 0) {
	 return count;
      }
      if (count != wrote) {
	 printf("handling short read\n");
      }
      wrote -= count;
      buffer += size * count;
   }
   return number;
}

int main(int argc, char *argv[])
{
   dvdcss_t file;
   int fd;
   char buffer[540000*DVDCSS_BLOCK_SIZE];
   long long blocks_read;
   int count = 0;
   int block_count = 0;
   int loop = 1;
   int len;
   struct stat buf;
   int blocks;
   int i;
   
   if (stat(argv[1], &buf) < 0) {
      perror("failed to stat input iso\n");
      return 1;
   }
   
   blocks = buf.st_blocks / 4;
   
   if (!(file = dvdcss_open(argv[1]))) {
      printf("failed to open iso\n");
      return 0;
   }
   
   if ((fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC)) < 0) {
      printf("failed to open output file\n");
      return 0;
   }
   
   if (! dvdcss_is_scrambled(file)) {
      printf("dvd isn't scrambled\n");
      return 0;
   }
   
   for(i=3; i <= argc; i++) {
      len = atoi(argv[i]) - block_count; //-1?
      
      if ((blocks_read = dvdcss_read(file, buffer, len, DVDCSS_READ_DECRYPT)) != len) {
	 printf("didn't read %d blocks, read %lld blocks\n", len, blocks_read);
      }
      if (blocks_read < 0) {
	 printf("blocks_read is < 0: %s\n", dvdcss_error(file));
	 return 0;
      }
      block_count += blocks_read;
      if (write(fd, buffer, 2048 * blocks_read) != 2048 * blocks_read) {
	 printf("write didn't write enough\n");
	 return 0;
      }
      if ( dvdcss_seek(file, block_count, DVDCSS_SEEK_KEY) < 0) {
	 printf("failed to seek to %d: %s\n", block_count, dvdcss_error(file));
      }
   }

   len = blocks - block_count;
   if ((blocks_read = dvdcss_read(file, buffer, len, DVDCSS_READ_DECRYPT)) != len) {
      printf("didn't read %d blocks, read %lld blocks\n", len, blocks_read);
   }
   if (blocks_read < 0) {
      printf("blocks_read is < 0: %s\n", dvdcss_error(file));
      return 0;
   }
   block_count += blocks_read;
   if (write(fd, buffer, 2048 * blocks_read) != 2048 * blocks_read) {
      printf("write didn't write enough\n");
      return 0;
   }
}
