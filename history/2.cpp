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

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE 1
#define _LARGEFILE_SOURCE 1

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
      printf("start block for %s = %u\n", psz_fname, start);
   } else {
      printf("didn't match on name %s\n", psz_fname);
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
   int fd_in;
   
   struct stat stat_buf;
   int total_blocks;
   unsigned int pos = 0;
   
   udf_t *p_udf;
   udf_dirent_t *p_udf_root;

   char * buffer;
   unsigned int preamble;

   if (!(buffer = (char *) malloc(MAX*DVDCSS_BLOCK_SIZE))) {
      printf("failed to allocate space for buffer\n");
      return 0;
   }

   if (argc != 3) {
      printf("usage:\n\t %s <input iso> <output iso>\n", argv[0]);
      return 1;
   }

   /* figure out how big the ISO image is */
   if (stat(argv[1], &stat_buf) < 0) {
      perror("failed to stat input iso");
      return 1;
   }
   
   total_blocks = stat_buf.st_size / 2048;
   if (stat_buf.st_size != (long long) total_blocks * 2048) {
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

/*   for(vector<int>::iterator it = start_blocks.begin(); it != start_blocks.end(); it++) {
      printf("end pos = %d\n", *it);
      } */

   /* prep CSS */
   if (!(input = dvdcss_open(argv[1]))) {
      fprintf(stderr, "dvdcss_open failed\n");
      return 1;
   }

   /* prep output file */
   if ((fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
      printf("failed to open output file\n");
      return 1;
   }
   
   if ((fd_in = open(argv[1], O_RDONLY)) < 0) {
      printf("failed to open output file\n");
      return 1;
   } 

   /* if not scrambled skip! */
   if (! dvdcss_is_scrambled(input)) {
      printf("dvd isn't scrambled\n");
      return 0;
   }

   preamble = start_blocks[0];
   start_blocks.erase(start_blocks.begin());

   if (preamble > MAX) {
      printf("preamble is too big (%u)\n", preamble);
      return 0;
   }

   pos = read(fd_in, buffer, preamble*2048);
   if (pos != preamble * 2048) {
      printf("didn't read enough (%u vs %d)\n", preamble*2048, pos);
      return 0;
   }

   if (write(fd, buffer, pos) != pos) {
      printf("didn't write enough\n");
      return 0;
   }

   pos = pos / 2048;
   
   while (! start_blocks.empty()) {
      int len = MAX;
      int blocks_read;
      
      int end = start_blocks[0];
//      start_blocks.pop_back();
      start_blocks.erase(start_blocks.begin());
      
      printf("current position = %u, next sync point at %u\n", pos, end);
      
      // don't dvdcss_seek first time around or last element (last block of dvd
      printf("syncing at position %u\n", pos);
      if ( dvdcss_seek(input, pos, DVDCSS_SEEK_KEY) < 0) {
	 fprintf(stderr, "failed to seek to %d: %s\n", pos, dvdcss_error(input));
/*      } else {
	printf("not seeking as at beginning, no sync points yet\n"); */
      }
      
      while (pos < end) {
	 //printf("current pos = %u\n", pos);
	 if (pos + len > end) {
	    //printf("going to a read shorter then %d as end is near\n", len);
	    len = end - pos;
	 }
	 //printf("going to read %d\n", len);
	 
	 if ((blocks_read = dvdcss_read(input, buffer, len, DVDCSS_READ_DECRYPT)) != len) {
	    fprintf(stderr, "didn't read %d blocks! read %d blocks\n", len, blocks_read);
	 }
	 //printf("read %d\n", blocks_read);
	 pos += blocks_read;
	 
	 if (write(fd, buffer, 2048 * blocks_read) != 2048 * blocks_read) {
	    fprintf(stderr, "write didn't write enough\n");
	    return 0;
	 }
      }
   }
}
