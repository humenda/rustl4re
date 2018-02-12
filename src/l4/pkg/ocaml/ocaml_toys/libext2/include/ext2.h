#pragma once

unsigned const EXT2_BLOCK_SIZE = 4096;

int open_disk_image (char const * const filename);
int open_file (char const * const filename);
int read_block (int const inode, unsigned const block, const char * buffer);

// XXX WARNING XXX : The values return by opendir and readdir are only valid
// until the next call to the ocaml runtime, afterwards they might have been
// garbage collected and are therefore dangling pointers!
unsigned opendir (char const * const name);
unsigned readdir (unsigned rec);

char const * filename (unsigned rec);
bool is_file (unsigned rec);
