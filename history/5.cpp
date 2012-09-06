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
#include <map>
#include <algorithm>
#include <string.h>
#include <ctype.h>
#include <errno.h>

using namespace std;

#define MAX 2000
#define CEILING(x, y) ((x+(y-1))/y) //ripped from libudf

vector<unsigned long long> start_blocks;
map<unsigned long long, unsigned long long> end_blocks;
map<unsigned long long, char *> start_map;

ssize_t my_write(int fd, const void * buf, size_t count)
{
   int ret;
   size_t my_count = count;
   char * my_buf = (char *) buf;
   
   while (my_count) {
      if (my_count != count) {
	 printf("looped in my_write, last write = %d\n", ret);
      }
      if ((ret = write(fd, my_buf, my_count)) == -1) {
	 if (errno != EINTR) {
	    perror("write failed!");
	    goto out;
	 }
      } else {
	 my_count -= ret;
	 my_buf += ret;
      }
   }
   
   ret = count;
  out:
   return ret;
}

static void add_block(const udf_dirent_t *p_udf_dirent)
{
   char *psz_fname=(char *) udf_get_filename(p_udf_dirent);
   char *name = strdup(psz_fname);
   int len = strlen(psz_fname);
   unsigned long long file_length;
   unsigned long long start;
   unsigned long long blocks;
   
   start = udf_get_start_block(p_udf_dirent);
   blocks = CEILING(udf_get_file_length(p_udf_dirent), DVDCSS_BLOCK_SIZE);
   
   start_map[start] = name;
   
   if (blocks == 0) {
      //file length of 0 would result in a blocks of 0, and don't want
      //to subtract one from it.
      end_blocks[start] = start;
   } else {
      //-1 as start block is included in count of blocks
      end_blocks[start] = start - 1 + blocks;
   }
   printf("%s: %llu->%llu (%llu blocks)\n", psz_fname, start, end_blocks[start], blocks);
   
   for(int i=0; i < len; i++) {
      psz_fname[i] = tolower(psz_fname[i]);
   }
   
   if (! strcmp(psz_fname + (len-3), "vob")) {
      if (blocks) {
	 if (find(start_blocks.begin(), start_blocks.end(), start) == start_blocks.end()) {
	    start_blocks.push_back(start);
	 }
      }
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
   int flags = O_RDWR;
   
   off64_t disc_len;
   int total_blocks;
   unsigned long long pos = 0;
   
   udf_t *p_udf;
   udf_dirent_t *p_udf_root;

   char * buffer;
   unsigned long long preamble;
   
   unsigned long long count = 0;
   
   if (argc == 3 && !strcmp(argv[2], "-test")) {
      flags = O_RDONLY;
   }
   
   if (!(buffer = (char *) malloc(MAX*DVDCSS_BLOCK_SIZE))) {
      printf("failed to allocate space for buffer\n");
      return 0;
   }

   if (argc < 2) {
      printf("usage:\n\t %s <input iso>\n", argv[0]);
      return 1;
   }
   
   if ((fd = open(argv[1], flags)) < 0) {
      printf("failed to open input/output file\n");
      return 1;
   } 
   
   /* figure out how big the ISO image is */
   if ((disc_len = lseek64(fd, 0, SEEK_END)) < 0) {
      perror("lseek64 failed");
      return 1;
   }
   
   total_blocks = disc_len / DVDCSS_BLOCK_SIZE;
   if (disc_len != (long long) total_blocks * DVDCSS_BLOCK_SIZE) {
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
   
   for(vector<unsigned long long>::iterator it = start_blocks.begin(); it != start_blocks.end(); it++) {
      printf("end pos = %llu\n", *it);
   }
   
   for(map<unsigned long long, char *>::iterator p = start_map.begin(); p != start_map.end(); p++) {
      printf("%s : %llu\n", p->second, p->first);
   }
   
   if (argc == 3 && !strcmp(argv[2], "-test")) {
      return 0;
   }
   
   /* prep CSS */
   if (!(input = dvdcss_open(argv[1]))) {
      fprintf(stderr, "dvdcss_open failed\n");
      return 1;
   }
   
   /* if not scrambled skip! */
   /* this doesn't do anything on iso input */
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
      
      int cur = pos;
      int end = start_blocks[0];

      int seeked = DVDCSS_NOFLAGS;
      
      start_blocks.erase(start_blocks.begin());
      
      //printf("syncing at position = %llu, next sync point at %u\n", pos, end);
      
      if ( dvdcss_seek(input, pos, DVDCSS_SEEK_KEY) < 0) {
	 fprintf(stderr, "failed to seek to %llu: %s\n", pos, dvdcss_error(input));
	 seeked = DVDCSS_SEEK_KEY;
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
	    char block[2048];
	    
	    if( ((uint8_t*)tmp_buffer)[0x14] & 0x30 ) {
	       int skip=0;
	       if (pos + index > end_blocks[cur]) {
		  printf("should skipping decode of supposed encrypted block (%llu) as not within VOB\n", pos+index);
		  bcopy(tmp_buffer, block, 2048);
		  skip=1;
	       }
	       count++;
	       if (dvdcss_seek(input, pos+index, seeked) < 0) {
		  fprintf(stderr, "failed to seek to %llu (index %d): %s\n", pos+index, index, dvdcss_error(input));
		  return 1;
	       }
	       seeked = DVDCSS_NOFLAGS;
	       
	       if (dvdcss_read(input, tmp_buffer, 1, DVDCSS_READ_DECRYPT) != 1) {
		  fprintf(stderr, "dvdcss_read failed\n");
		  return 1;
	       }
	       if (skip) {
		  printf("testing block we are skipping anyways\n");
		  if( ((uint8_t*)tmp_buffer)[0x14] & 0x30 ) {
		     printf("dvdcss \"decoded\" block still has bit set\n");
		     if (!memcmp(tmp_buffer, block, 2048)) {
			printf("dvdcss \"decoded\" block didn't change!\n");
		     } else {
			printf("dvdcss \"decoded\" block changed!\n");
		     }
		  } else {
		     printf("dvdcss \"decoded\" block got bit removed!\n");
		  }
	       } else {
		  lseek64(fd, (pos+index) * DVDCSS_BLOCK_SIZE, SEEK_SET);
		  if (my_write(fd, tmp_buffer, DVDCSS_BLOCK_SIZE) < 0) {
		     return 1;
		  }
		  reseek = 1;
	       }
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
