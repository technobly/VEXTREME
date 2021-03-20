#ifndef MENU_H
#define MENU_H

#include "fatfs/ff.h"

#define MENU_TEXT_LEN 16

typedef struct {
	int is_dir;
	char fname[_MAX_LFN + 1];
} file_entry;

typedef struct {
	int num_files;
	file_entry f_entry[80];
} dir_listing;

int checkExtension(char* filename, const char** extlist, int size, bool modify);
void sortDirectory(char *fdir, dir_listing *listing);
void loadListing(char *fdir, dir_listing *listing, const int fnptrs, const int strptrs, char *romData); 
bool isDirectoryExist(char *fdir);

#endif
