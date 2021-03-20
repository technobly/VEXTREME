#define _GNU_SOURCE // required for strcasestr
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "menu.h"

#include "xprintf.h"
#include "fatfs/ff.h"

//Get a listing of the roms in the 'roms/' directory and
//poke them into the menu cartridge space
void loadListing(char *fdir, dir_listing *listing, const int fnptrs, const int strptrs, char *romData) {
	char buff[sizeof(listing->f_entry->fname)];
	char *name;

	int ptrpos = fnptrs;  // fixed location in menu.bin for 512 filename pointers (from &ptrpos ~ &strpos)
	int strpos = strptrs; // filename data starts here

	int i;
	int is_dir;
	int idx;
	file_entry *f_entry;

	xprintf("Reading dir: %s...\n", fdir);
	sortDirectory(fdir, listing);
	for (idx = 0; idx < listing->num_files; idx++) {
		f_entry=&(listing->f_entry[idx]);

		strncpy(buff, f_entry->fname, sizeof(buff)-1);
		buff[sizeof(buff)-1] = '\0';
		name=buff;

		is_dir=f_entry->is_dir;

		// xprintf("Found file %s (%d), file %d\n", name, is_dir, idx);
		romData[ptrpos++]=strpos>>8;
		romData[ptrpos++]=strpos&0xff;

		// remove extensions
		const char* extList[2] = { ".bin", ".vec" };
		int is_game = checkExtension(name, extList, 2, true); // check and modify
		if (!is_game && !is_dir) {
			continue; // If not a game, don't list it in the menu
		}

		i = is_dir ? (MENU_TEXT_LEN-2) : MENU_TEXT_LEN;
		if (is_dir) romData[strpos++]='<';
		while (*name!=0 && i>0) {
			if (*name<32) {
				romData[strpos++]=' ';
			} else if (*name>=32 && *name<95) {
				romData[strpos++]=*name;
			} else if (*name>='a' && *name<='z') {
				romData[strpos++]=(*name-'a')+'A'; //convert to caps
			} else {
				romData[strpos++]='_';
			}
			name++;
			i--;
		}
		if (is_dir) romData[strpos++]='>';
		romData[strpos++]=0x80; //end of string
	}
	//finish with zero ptr
	romData[ptrpos++]=0;
	romData[ptrpos++]=0;
	xprintf("Done.\n");
}

int checkExtension(char* filename, const char** extlist, int size, bool modify) {
	for (int i = 0; i < size; i++ ) {
		char* ptr = strcasestr(filename, extlist[i]);
		if (ptr) {
			if (modify) {
				*ptr = 0;
			}
			return 1;
		}
	}
	return 0;
}

static int f_entry_compare(const void* a, const void* b)
{
	// setting up rules for comparison
	file_entry *f_entryA = (file_entry *)a;
	file_entry *f_entryB = (file_entry *)b;

	int dirA = f_entryA->is_dir;
	int dirB = f_entryB->is_dir;

	if (dirA == dirB) {
		return (strcmp(f_entryA->fname, f_entryB->fname));
	} else {
		 return dirB - dirA;
	}
}


void sortDirectory(char *fdir, dir_listing *listing) {
	DIR d;
	FILINFO fi;
	char lfn[_MAX_LFN + 1];
	fi.lfname=lfn;
	fi.lfsize=sizeof(lfn);

	int idx = 0;
	file_entry *f_entry;
	char *name;
	int is_dir;

	// initial unsorted directory read
	f_opendir(&d, fdir);
	while (f_readdir(&d, &fi)==FR_OK) {
		// xprintf("Found file %s (%s)\n", fi.lfname, fi.fname);
		f_entry=&(listing->f_entry[idx]);

		if (fi.fname[0]==0) break;
		if (fi.fname[0]=='.') {
			if (fi.fname[1] != '.')
				continue;
			if (strcmp(fdir, "/roms") == 0)
				continue;
		}

		is_dir = (fi.fattrib & AM_DIR) ? 1 : 0;


		if (fi.lfname[0]=='.') continue; // ignore MacOS dotfiles
		name=fi.lfname;
		if (name==NULL || name[0]==0) name=fi.fname; //use short name if no long name available

		// check extensions and ignore all non case insensitive .bin/.vec files
		const char* extList[2] = { ".bin", ".vec" };
		int is_game = checkExtension(name, extList, 2, false); // check only
		if (!is_game && !is_dir) {
			continue; // If not a game or directory, don't list it in the menu
		}

		// add file/dir to listing
		f_entry->is_dir = is_dir;
		strcpy(f_entry->fname, name);
		idx++;
	}
	f_closedir(&d);

	listing->num_files = idx;

	xprintf("Found %d files in %s\n", listing->num_files, fdir);

	// xprintf("About to sort\n");
	qsort(listing->f_entry, listing->num_files, sizeof(file_entry), f_entry_compare);
	// xprintf("Done sorting\n");
}

bool isDirectoryExist(char *fdir) {
	DIR     d;
	FRESULT res;

	res = f_opendir(&d, fdir);

	if (res == FR_OK) {
		f_closedir(&d);
	}

	return res == FR_OK;
}