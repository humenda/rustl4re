#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>

#include <l4/ocaml/ext2.h>

extern "C" {
#include <mlvalues.h>
#include <memory.h>
#include <alloc.h>
#include <callback.h>
#undef alloc	// a flaw in ocaml/contrib/byterun/compatibility.h: #define alloc caml_alloc, which clashes with L4Re::Util::cap_alloc.alloc - leading to cap_alloc.>>>caml_alloc<<<
}

char* disk_image = 0;
uint32 disk_size = 0;

extern "C"
CAMLprim value
get_dev_block_size (value unit)
{
	(void)unit;
	return Val_int(EXT2_BLOCK_SIZE);
}

extern "C"
CAMLprim value
read_block_ext (value buffer, value block)
{
	signed i = Int_val (block);
	char* s = String_val (buffer);

	// sanity checks, XXX prove needlessness 
	assert (disk_size >= EXT2_BLOCK_SIZE);
	if (i<0)
		return Val_false;

	if ((i+1) * EXT2_BLOCK_SIZE > disk_size)
		return Val_false;

	memcpy (s, disk_image + i * EXT2_BLOCK_SIZE, EXT2_BLOCK_SIZE);

	return Val_true;
}

// 0 - error / 1 - success
int open_disk_image (char const * const filename)
{
	L4Re::Util::Env_ns ns;
	L4::Cap<L4Re::Dataspace> image_ds_cap;

	if (!(image_ds_cap = ns.query<L4Re::Dataspace>(filename))) {
		fprintf (stderr, "ns-query() could not resolve query '%s'.", filename);
		return 0;
	}

	disk_size = image_ds_cap->size();
	if (L4Re::Env::env()->rm()->attach (&disk_image, image_ds_cap->size(), L4Re::Rm::Search_addr, image_ds_cap)) {
		fprintf (stderr, "attach disk image failed.");
		return 0;
	}

	// init ocaml subsystem
	char zero = 0;
	char *args[2];
	args[0] = &zero;
	args[1] = 0;
	caml_startup(args);

	return 1;
}

// 0 - error / >0 - inode number
int open_file (char const * const filename)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("path_to_inode");

	return Int_val (caml_callback (*closure, caml_copy_string (filename)));
}

// 0 - error / 1 - success
int read_block (int const inode, unsigned const block, const char * buffer)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("read_file_block");

	return Int_val (caml_callback3 (*closure, Val_int(inode), Val_int(block), (value)buffer));
}

unsigned opendir (char const * const name)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("opendir");

	value v = caml_callback (*closure, caml_copy_string (name));

	if (Long_val(v) == 0)
		return 0;

	v = Field(v,0);

	assert (v != Int_val(0));

	return v;
}

unsigned readdir(unsigned rec)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("readdir");

	assert(closure);

	value v = caml_callback (*closure, rec);

	if (Long_val(v) == 0)	// None
		return 0;

	v = Field(v,0);

	assert (Is_block(v));

	return v;
}

char const * filename (unsigned rec)
{
	value v = Field (rec, 4);
	return String_val(v);
}

bool is_file (unsigned rec)
{
	unsigned m = Long_val (Field (rec, 5));
	return ((m >> 12) & 0xF) == 8;
}

