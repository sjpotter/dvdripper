/* Copyright (c) 2012 Shaya Potter <spotter@gmail.com */

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include "DVDRipper.h"

int main(int argc, char *argv[])
{
   bool test = false;
   int flags = O_RDWR;
   
   if (argc == 3 && !strcmp(argv[2], "-test")) {
      flags = O_RDONLY;
      test = true;
   }
   
   if (argc < 2) {
      printf("usage:\n\t %s <input iso> [-test]\n", argv[0]);
      return 1;
   }
   
   DVDRipper * dvdripper = new DVDRipper(argv[1], flags);
   dvdripper->open_disc();
   dvdripper->find_start_blocks();
   dvdripper->info();
   
   if (test) {
      return 0;
   }
   
   if (dvdripper->css_open()) {
     return 0;
   }

   return dvdripper->rip();
}
