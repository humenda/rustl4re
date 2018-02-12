#pragma once

unsigned const ISO_BLOCK_SIZE = 2048;

int open_disk_image (char const * const name);

unsigned opendir (char const * const name);
bool readdir (unsigned rec);
char const * filename (unsigned rec);
bool is_file (unsigned rec);

unsigned lookup_file (char const * const name, unsigned* size);
unsigned lookup_file (unsigned rec, unsigned* size);
