open Printf

external get_dev_block_size : unit -> int = "get_dev_block_size"

external read_block_ext : string -> int -> bool = "read_block_ext"

let dev_block_size = get_dev_block_size ()

exception Overflow of string

type inode_rec = {
	id		: int; (* debug : number of this very inode *)
	mode	: int;
	uid		: int;
	size	: int;
	atime	: int32;
	ctime	: int32;
	mtime	: int32;
	dtime	: int32;
	gid		: int;
	links	: int;
	blocks	: int;
	flags	: int;
	block	: int array;
}

type sb_rec = {
	inode_count			: int;
	block_count			: int;
	group_count			: int;
	blocks_per_group	: int;
	inodes_per_group	: int;
	inode_size			: int;
	block_size			: int;
	group_desc_size		: int;
	backup_list			: int list;
}

type read_dir_rec = {
	dir_inode	: inode_rec;
	block_off	: int;
	byte_off	: int;
	length		: int;
	name		: string;
	f_mode		: int;
	inode		: int;
}

(* superblock *)
let sb = ref {inode_count=0; block_count=0; group_count=0; blocks_per_group=0; inodes_per_group=0; inode_size=0; block_size=1024; group_desc_size=32; backup_list=[]}

(* root inode *)
let ri = ref {id=0; mode=0; uid=0; size=0; atime=Int32.zero; ctime=Int32.zero; mtime=Int32.zero; dtime=Int32.zero; gid=0; links=0; blocks=0; flags=0; block=[||]}

let read_block s n =
	let r = read_block_ext s n in
	if (not r) then printf "read_block failed\n" else ()

(* byte accessors *)

let read_byte s i = int_of_char s.[i]

let read1 s i = read_byte s i

let read2 s i =
    let ch1 = read_byte s i in
    let ch2 = read_byte s (i+1) in
    ch1 lor (ch2 lsl 8)

let read4 s i =
    let ch1 = read_byte s i in
    let ch2 = read_byte s (i+1) in
    let ch3 = read_byte s (i+2) in
    let ch4 = read_byte s (i+3) in
(*	printf "read4 %x %x %x %x\n" ch1 ch2 ch3 ch4;*)
    if ch4 land 128 <> 0 then begin
        if ch4 land 64 = 0 then raise (Overflow "read4");
        ch1 lor (ch2 lsl 8) lor (ch3 lsl 16) lor ((ch4 land 127) lsl 24)
    end else begin
        if ch4 land 64 <> 0 then raise (Overflow "read4");
        ch1 lor (ch2 lsl 8) lor (ch3 lsl 16) lor (ch4 lsl 24)
    end

let read4_32 s i =
    let ch1 = read_byte s i in
    let ch2 = read_byte s (i+1) in
    let ch3 = read_byte s (i+2) in
    let ch4 = read_byte s (i+3) in
	Int32.logor (Int32.of_int (ch1 lor (ch2 lsl 8) lor (ch3 lsl 16))) (Int32.shift_left (Int32.of_int ch4) 24)

let div_up a b = (a + b - 1) / b

let create_backup_list max =
	let rec exp i j max l = if i > max then l else exp (i*j) j max (i::l) in
	let f i = exp i i max [] in
	let (l1,l2,l3) = (f 3,f 5,f 7) in
	0::1::(List.rev_append (List.rev_append l1 l2) l3)

(* param i must be a valid inode index *)
let read_inode_from_block b i =
	let r2 j = read2 b (i + j) in
	let r4 j = read4 b (i + j) in
	let rx j = read4_32 b (i + j) in
	let a = Array.make 15 0 in
	let rec ra i = if i < 15 then begin a.(i) <- r4 (40 + i * 4); ra (i+1); end else a in
	let i = {
		id		= i;
		mode	= r2 0;
		uid		= r2 2;
		size	= r4 4;
		atime	= rx 8;
		ctime	= rx 12;
		mtime	= rx 16;
		dtime	= rx 20;
		gid		= r2 24;
		links	= r2 26;
		blocks	= r4 28;
		flags	= r4 32;
		block	= ra 0;
	} in i

let read_inode inode =
	assert (inode > 0);
	let inode = inode - 1 in
	let group = (inode / !sb.inodes_per_group) in
	let block = 2 + group * !sb.blocks_per_group in
	let block = if not (List.mem group !sb.backup_list)
		then block
		else block + div_up (!sb.group_count * !sb.group_desc_size) !sb.block_size in
	let data = String.create !sb.block_size in
	read_block data block;	(* read inode bitmap *)
	assert (0 != ((read1 data (inode lsr 3)) land (1 lsl (inode land 7))));
	read_block data (1 + block + inode * !sb.inode_size / !sb.block_size);
	read_inode_from_block data (inode mod !sb.inodes_per_group * !sb.inode_size)

let read_superblock () =
	let data = String.create dev_block_size in
	let offset = match dev_block_size with
	| 1024 -> read_block data 1; 0
	| 2048 -> read_block data 0; 1024
	| 4096 -> read_block data 0; 1024
	| _ -> assert false
	in

	let r2 i = read2 data (offset+i) in
	let r4 i = read4 data (offset+i) in

	assert ((r2 56) = 0xEF53);	(* magic *)
	assert ((r2 58) = 1);		(* state *)
	assert (0 = ((r4 92) lor (r4 96) lor (r4 100)));	(* no fancy features *)

	let b = {
		inode_count			= r4 0;
		block_count			= r4 4;
		group_count			= 0;
		blocks_per_group	= r4 32;
		inodes_per_group	= r4 40;
		inode_size			= r2 88;
		block_size			= 1024 * (1 lsl r4 24);
		group_desc_size		= 32;	(* hard coded, sizeof (struct ext2_group_desc) *)
		backup_list			= [];
	} in sb := {
		b with
		backup_list = create_backup_list b.block_count;
		group_count = div_up b.block_count b.blocks_per_group
	};

	ri := read_inode 2	(* read root inode *)

(* inode number or 0 if invalid *)
let block_rel_to_abs inode block =
	let data = String.create !sb.block_size in

	let rec f addr block level =
		if block = 0 then 0 else begin
			read_block data block;
			let block = read4 data (addr lsr level land 0x3ff * 4) in
			if level = 0 then block else f addr block (level-10)
		end
	in

	let l = [|12; 12+1024; 12+1024+1024*1024|] in
	if block < l.(0) then inode.block.(block) else
	if block < l.(1) then f (block-l.(0)) inode.block.(12) 0 else
	if block < l.(2) then f (block-l.(1)) inode.block.(13) 10 else
						  f (block-l.(2)) inode.block.(14) 20

(* returns index or -1 *)
let inode_iter inode buffer func =
	let last_block = (inode.size - 1) / !sb.block_size in

	let rec f index block_number =
		if func buffer index then index else
		let rec_len = read2 buffer (index+4) in
		if rec_len + index < !sb.block_size
		then f (index+rec_len) block_number
		else if block_number = last_block then 0
		else let i = block_rel_to_abs inode (block_number+1) in
		if i = 0 then failwith "inode_iter: block_rel_to_abs returned None" (* this means an error in the metadata on disk *)
		else read_block buffer i; f 0 (block_number+1)

	in let i = block_rel_to_abs inode 0 in
	if i = 0 then -1 else (read_block buffer i; f 0 0)

let next_name s i =
	let (j,k) = if String.contains_from s i '/'
	then let j = String.index_from s i '/' in (j-i,j+1)
	else (String.length s - i,-1)
	in (String.sub s i j, k)

(* 0 or valid inode number *)
let path_to_inode path =
	assert (path.[0] = '/');
	let buffer = String.create !sb.block_size in

	let search name buffer index =
		let name_len = read1 buffer (index+6) in
		let sub = String.sub buffer (index+8) name_len in
		String.compare name sub = 0

	(* returns the inode number of the searched string or 0 if not found *)
	in let scan_name name inode =
		let index = inode_iter inode buffer (search name) in
		if index = 0 then 0 else read4 buffer index

	in let rec f i inode =
		let (s, i) = next_name path i in
		let j = scan_name s inode in
		if (i = -1 || j = 0) then j else f i (read_inode j)

	in
		assert (String.length path > 0);
		assert (path.[0] = '/');
		if String.length path = 1 then 2 (* root inode is hard coded number 2 *) else
		f 1 !ri

let opendir path =
	let inode_num = path_to_inode path in
	if inode_num = 0 then None else begin
		let dir_inode = read_inode inode_num in
		let buffer = String.create !sb.block_size in
		read_block buffer (block_rel_to_abs dir_inode 0);
		let inode = read4 buffer 0 in
		let length = read2 buffer 4 in
		let file_inode = read_inode inode in
		let name = String.sub buffer 8 (read2 buffer 6) in
		Some { dir_inode = dir_inode; block_off = 0; byte_off = 0; length = length; name = name; f_mode = file_inode.mode; inode = inode; }
	end

let readdir r =
	let buffer = String.create !sb.block_size in
	let byte_off = r.byte_off + r.length in

	if (byte_off == !sb.block_size) && (r.dir_inode.size = !sb.block_size) then None else begin
		let (block_off,byte_off,length) =
			if r.byte_off == !sb.block_size
			then (r.block_off + 1,0,r.length - !sb.block_size) 
			else (r.block_off,byte_off,r.length) in
		read_block buffer (block_rel_to_abs r.dir_inode block_off);
		let inode = read4 buffer byte_off in
		let length = read2 buffer (byte_off + 4) in
		let file_inode = read_inode inode in
		let name = String.sub buffer (byte_off + 8) (read2 buffer (byte_off + 6)) in
		Some { dir_inode = r.dir_inode; block_off = block_off; byte_off = byte_off; length = length; name = name; f_mode = file_inode.mode; inode = inode; }
	end

let read_file_block inode block buffer =
	let inode = read_inode inode in
	let block = block_rel_to_abs inode block in
	read_block_ext buffer block

let () =
	read_superblock ();

	Callback.register "path_to_inode" path_to_inode;
	Callback.register "read_file_block" read_file_block;
	Callback.register "opendir" opendir;
	Callback.register "readdir" readdir;
