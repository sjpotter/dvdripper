/* Copyright (c) 2012 Shaya Potter <spotter@gmail.com> */

#pragma once
#ifndef DVDRIPPER_H
#define DVDRIPPER_H

#include <vector>
#include <map>
#include <cdio/iso9660.h>
#include <dvdcss/dvdcss.h>

class DVDRipper {
   std::vector<unsigned long long> start_blocks;
   std::map<unsigned long long, unsigned long long> end_blocks;
   std::map<unsigned long long, char *> start_map;

   iso9660_t *p_iso;
   int flags;
   int total_blocks;
   int fd;
   char * path;

   dvdcss_t input;

   unsigned long long preamble;

public:
   DVDRipper(char *, int);
   int open_disc();
   void find_start_blocks();
   void info();
   int rip();
};

#endif /* DVDRIPPER_H */
