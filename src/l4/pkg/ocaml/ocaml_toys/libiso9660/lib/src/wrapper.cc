#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <l4/re/env>
#include <l4/re/dataspace>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/env_ns>

#include <l4/ocaml/iso9660.h>

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
	return Val_int(ISO_BLOCK_SIZE);
}

extern "C"
CAMLprim value
read_block_ext (value buffer, value block)
{
	int i = Int_val (block);
	char* s = String_val (buffer);

	// sanity checks, XXX prove needlessness 
	assert (disk_size >= ISO_BLOCK_SIZE);
	if (i<0)
		return Val_false;

	if ((i+1) * ISO_BLOCK_SIZE > disk_size)
		return Val_false;

	memcpy (s, disk_image + i * ISO_BLOCK_SIZE, ISO_BLOCK_SIZE);

	return Val_true;
}

// 0 - error / 1 - success
int open_disk_image (char const * const filename)
{
	L4Re::Util::Env_ns ns;
	L4::Cap<L4Re::Dataspace> image_ds_cap;

	if (!(image_ds_cap = ns.query<L4Re::Dataspace>(filename))) {
		fprintf (stderr, "l4re_ns_query() could not resolve query '%s'.", filename);
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

unsigned opendir (char const * const name)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("opendir");

	assert(closure);

	value v = caml_callback (*closure, caml_copy_string (name));

	if (Long_val(v) == 0)
		return 0;

	v = Field(v,0);

	assert (v != Int_val(0));

	caml_register_global_root((value*)v);

	return v;
}

bool readdir(unsigned rec)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("readdir");

	assert(closure);

	value v2 = caml_callback (*closure, rec);

	if (v2 == Val_true) 
		return true;

	caml_remove_global_root((value*)rec);
	return false;
}

char const * filename (unsigned rec)
{
	value v1 = Field (rec, 3);
	value v2 = Field (v1, 11);
	return String_val(v2);
}

bool is_file (unsigned rec)
{
	value v1 = Field (rec, 3);
	value v2 = Field (v1, 10);
	return (Int_val(v2) & 2) == 0;
}

unsigned lookup_file (char const * const name, unsigned* size)
{
	static value* closure = 0;

	if (!closure)
		closure = caml_named_value ("lookup_file");

	assert (closure);

	value v = caml_callback (*closure, caml_copy_string (name));

	assert (Is_block(v));

	int sector = Int_val (Field (v,0));

	if (!sector)
		return 0;

	*size = Int_val (Field (v,1));

	return sector;
}

unsigned lookup_file (unsigned rec, unsigned* size)
{
	value v = Field (rec, 3);
	*size = Int_val (Field (v, 2));
	return Int_val (Field (v, 1));
}
