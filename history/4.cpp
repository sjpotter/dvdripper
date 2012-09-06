#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE 1
#define _LARGEFILE_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <dvdcss/dvdcss.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cdio/cdio.h>
#include <cdio/udf.h>
#include <vector>
#include <algorithm>
#include <string.h>
#include <ctype.h>

using namespace std;

vector<int> start_blocks;

#define MAX 2000

static void add_block(const udf_dirent_t *p_udf_dirent)
{
   char *psz_fname=(char *) udf_get_filename(p_udf_dirent);
   int len = strlen(psz_fname);

   unsigned int start;

   for(int i=0; i < len; i++) {
      psz_fname[i] = tolower(psz_fname[i]);
   }
   if (! strcmp(psz_fname + (len-3), "vob")) {
      start = udf_get_start_block(p_udf_dirent);
      start_blocks.push_back(start);
      //printf("start block for %s = %u\n", psz_fname, start);
   } else {
      //printf("didn't match on name %s\n", psz_fname);
   }
}

void find_start_blocks(udf_t *p_udf, udf_dirent_t *p_udf_dirent)
{
   if (!p_udf_dirent)
      return;
   
   while(udf_readdir(p_udf_dirent)) {
      if (udf_is_dir(p_udf_dirent)) {
	 udf_dirent_t *p_udf_dirent2 = udf_opendir(p_udf_dirent);
	 if (p_udf_dirent) {
	    find_start_blocks(p_udf, p_udf_dirent2);
	 }
      } else {
	 add_block(p_udf_dirent);
      }
   }
}

int main(int argc, char *argv[])
{
   dvdcss_t input;
   int fd;
   
   struct stat stat_buf;
   int total_blocks;
   unsigned long long pos = 0;
   
   udf_t *p_udf;
   udf_dirent_t *p_udf_root;

   char * buffer;
   unsigned long long preamble;

   unsigned long long count = 0;

   if (!(buffer = (char *) malloc(MAX*DVDCSS_BLOCK_SIZE))) {
      printf("failed to allocate space for buffer\n");
      return 0;
   }

   if (argc != 2) {
      printf("usage:\n\t %s <input iso>\n", argv[0]);
      return 1;
   }

   /* figure out how big the ISO image is */
   if (stat(argv[1], &stat_buf) < 0) {
      perror("failed to stat input iso");
      return 1;
   }
   
   total_blocks = stat_buf.st_size / DVDCSS_BLOCK_SIZE;
   if (stat_buf.st_size != (long long) total_blocks * DVDCSS_BLOCK_SIZE) {
      printf("partial block?????\n");
      return 1;
   }
   printf("total_blocks = %d\n", total_blocks);
   start_blocks.push_back(total_blocks);
     
   /* find locations where have to rekey CSS */
   if ((p_udf = udf_open(argv[1])) == NULL) {
      fprintf(stderr, "couldn't open %s as UDF\n", argv[1]);
      return 1;
   }
   
   if (!(p_udf_root = udf_get_root(p_udf, true, 0))) {
      fprintf(stderr, "couldn't find / in %s\n", argv[1]);
      return 1;
   }

   find_start_blocks(p_udf, p_udf_root);

   sort(start_blocks.begin(), start_blocks.end());

   /* prep CSS */
   if (!(input = dvdcss_open(argv[1]))) {
      fprintf(stderr, "dvdcss_open failed\n");
      return 1;
   }

   if ((fd = open(argv[1], O_RDWR)) < 0) {
      printf("failed to open input/output file\n");
      return 1;
   } 

   /* if not scrambled skip! */
   if (! dvdcss_is_scrambled(input)) {
      printf("dvd isn't scrambled\n");
      return 0;
   }

   preamble = start_blocks[0];
   start_blocks.erase(start_blocks.begin());

   lseek64(fd, preamble*DVDCSS_BLOCK_SIZE, SEEK_SET);
   
   pos = preamble;
   
   while (! start_blocks.empty()) {
      int len = MAX;
      int blocks_read;
      
      int end = start_blocks[0];
      start_blocks.erase(start_blocks.begin());
      
      //printf("syncing at position = %llu, next sync point at %u\n", pos, end);
      
      if ( dvdcss_seek(input, pos, DVDCSS_SEEK_KEY) < 0) {
	 fprintf(stderr, "failed to seek to %llu: %s\n", pos, dvdcss_error(input));
      }
      
      while (pos < end) {
	 int read_size;
	 char * tmp_buffer;
	 int reseek;

	 if (pos + len > end) {
	    len = end - pos;
	 }

	 read_size = len * DVDCSS_BLOCK_SIZE;
	 
	 if ((blocks_read = read(fd, buffer, read_size)) != read_size) {
	    printf("short read, not handled yet\n");
	    return 1;
	 }

	 tmp_buffer = buffer;
	 reseek = 0;

	 for(int index = 0; index < len; index++) {
	    if( ((uint8_t*)tmp_buffer)[0x14] & 0x30 ) {
	       count++;
	       if (dvdcss_seek(input, pos+index, DVDCSS_NOFLAGS) < 0) {
		  fprintf(stderr, "failed to seek to %llu (index %d): %s\n", pos+index, index, dvdcss_error(input));
		  return 1;
	       }
	       if (dvdcss_read(input, tmp_buffer, 1, DVDCSS_READ_DECRYPT) != 1) {
		  fprintf(stderr, "dvdcss_read failed\n");
		  return 1;
	       }
	       lseek64(fd, (pos+index) * DVDCSS_BLOCK_SIZE, SEEK_SET);
	       if (write(fd, tmp_buffer, DVDCSS_BLOCK_SIZE) != DVDCSS_BLOCK_SIZE)
	       {
		  printf("failed to write block correctly\n");
		  return 1;
	       }
	       reseek = 1;
	    }
	    tmp_buffer = tmp_buffer + DVDCSS_BLOCK_SIZE;
	 }
	 
	 pos += len;
	 if (reseek) {
	    lseek64(fd, pos * DVDCSS_BLOCK_SIZE, SEEK_SET);
	 }
      }
   }
   printf("descrambled %llu blocks\n", count);
   printf("\n");

   return 0;
}
