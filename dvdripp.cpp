#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE 1
#define _LARGEFILE_SOURCE 1

#include <cstdio>
#include <unistd.h>
#include <dvdcss/dvdcss.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cdio/cdio.h>
#include <cdio/iso9660.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <strings.h>
#include <cctype>
#include <cerrno>

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

void find_start_blocks(iso9660_t *p_iso)
{
   CdioList_t *p_entlist;
   CdioListNode_t *p_entnode;
   
   if (!p_iso)
      return;
   
   p_entlist = iso9660_ifs_readdir (p_iso, "/video_ts/");
   
   if (p_entlist) {
      _CDIO_LIST_FOREACH (p_entnode, p_entlist) {
	 unsigned long long start;
	 unsigned long long blocks;
	 
	 char filename[4096];
	 int len;
	 
	 iso9660_stat_t *p_statbuf =
	    (iso9660_stat_t *) _cdio_list_node_data (p_entnode);
	 iso9660_name_translate(p_statbuf->filename, filename);
	 len = strlen(filename);
	 
	 if (!(2 == p_statbuf->type) ) {
	    if (! strcmp(filename + (len-3), "vob")) {
	       start = p_statbuf->lsn;
	       blocks = CEILING(p_statbuf->size, DVDCSS_BLOCK_SIZE);
	       
	       start_map[start] = strdup(filename);
	       
	       if (blocks == 0) {
		  //file length of 0 would result in a blocks of 0, and don't want
		  //to subtract one from it.
		  end_blocks[start] = start;
	       } else {
		  //-1 as start block is included in count of blocks
		  end_blocks[start] = start - 1 + blocks;
	       }
	       
	       printf("%s: %llu->%llu (%llu blocks)\n", filename, start, end_blocks[start], blocks);
	       
	       if (blocks) {
		  if (find(start_blocks.begin(), start_blocks.end(), start) == start_blocks.end()) {
		     start_blocks.push_back(start);
		  }
	       }
	    }
	 }
      }
      
      _cdio_list_free (p_entlist, true);
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

   iso9660_t * p_iso;

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
   if ((p_iso = iso9660_open(argv[1])) == NULL) {
      printf("couldn't open %s as UDF\n", argv[1]);
      return 1;
   }
   
   find_start_blocks(p_iso);
   
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
      printf("dvdcss_open failed\n");
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
      unsigned long long end = start_blocks[0];
      int seek_failed = 0;

      fflush(NULL);

      start_blocks.erase(start_blocks.begin());
      
      //printf("syncing at position = %llu, next sync point at %u\n", pos, end);
      
      if ( dvdcss_seek(input, pos, DVDCSS_SEEK_KEY) < 0) {
	 //Q. Can it ever fail to get key here, but have the key
	 //elsewhere?  perhaps have some accounting if this fails,
	 //know no point in trying to dvdcss_read() till next
	 //DVDCSS_SEEK_KEY?
	 printf("failed to seek to %llu: %s\n", pos, dvdcss_error(input));
	 seek_failed = 1;
      } else {
	 seek_failed = 0;
      }
      
      while (pos < end) {
	 int read_size;
	 char * tmp_buffer;
	 int reseek;

         fflush(NULL);
	 
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
	       if (pos + index > end_blocks[cur]) {
		  printf("should skipping decode of supposed encrypted block (%llu) as not within VOB\n", pos+index);
	       }
	       
	       if (! dvdseek_failed) {
		  if (dvdcss_seek(input, pos+index, DVDCSS_NOFLAGS) < 0) {
		     printf("failed to seek to %llu (index %d): %s\n", pos+index, index, dvdcss_error(input));
		     return 1;
		  }
		  
		  if (dvdcss_read(input, tmp_buffer, 1, DVDCSS_READ_DECRYPT) != 1) {
		     printf("dvdcss_read failed\n");
		     return 1;
		  }
		  
		  lseek64(fd, (pos+index) * DVDCSS_BLOCK_SIZE, SEEK_SET);
		  if (my_write(fd, tmp_buffer, DVDCSS_BLOCK_SIZE) < 0) {
		     return 1;
		  }
		  
		  reseek = 1;
		  count++;
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
