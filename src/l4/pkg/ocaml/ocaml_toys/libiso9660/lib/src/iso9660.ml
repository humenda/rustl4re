open Printf

external get_dev_block_size : unit -> int = "get_dev_block_size"

external read_block_ext : string -> int -> bool = "read_block_ext"

let sector_size = get_dev_block_size ()

exception Overflow of string

type dir_rec = {
	size    : int;
	sector  : int;
	length  : int;
	year    : int;
	month   : int;
	day     : int;
	hour    : int;
	min     : int;
	sec     : int;
	offset  : int;
	flags   : int;
	name    : string;
}

type read_dir_rec = {
	mutable r_sector	: int;
	mutable r_offset	: int;
	mutable r_length	: int;
	mutable f_rec		: dir_rec;
}

let root_sec = ref 0

let block_cache_rr = ref 0 (* round robin replacement strategy *)
let block_cache_i = Array.make 16 (-1)
let block_cache_s = Array.make 16 (String.create sector_size)

let read_block n =
	let i = ref 0 in
	while (!i < 16 && block_cache_i.(!i) != n) do i := !i + 1 done;
	if !i < 16 then block_cache_s.(!i)
	else begin
		let i = !block_cache_rr in
		let ok = read_block_ext block_cache_s.(!block_cache_rr) n in
		if (not ok) then failwith ("read_block "^(string_of_int n)^" failed") else ();
		block_cache_i.(!block_cache_rr) <- n;
		block_cache_rr := (!block_cache_rr + 1) mod 16;
		block_cache_s.(i)
	end

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
    if ch4 land 128 <> 0 then begin
        if ch4 land 64 = 0 then raise (Overflow "read4");
        ch1 lor (ch2 lsl 8) lor (ch3 lsl 16) lor ((ch4 land 127) lsl 24)
    end else begin
        if ch4 land 64 <> 0 then raise (Overflow "read4");
        ch1 lor (ch2 lsl 8) lor (ch3 lsl 16) lor (ch4 lsl 24)
    end

let read_record s i =
    let r1 j = read1 s (i+j) in
    let r4 j = read4 s (i+j) in
    let size = r1 0 in
    let strlen = r1 32 in
    { size    = size;
		sector  = r4 2;
		length  = r4 10;
		year    = r1 18 + 1900;
		month   = r1 19;
		day     = r1 20;
		hour    = r1 21;
		min     = r1 22;
		sec     = r1 23;
		offset  = r1 24;
		flags   = r1 25;
		name    = if strlen != 1 then String.sub s (i+33) strlen else if (r1 33) = 0 then "." else (assert ((r1 33)=1);"..")
	}

(* searches pvd, returns sector number of root record *)
let read_primary_volume_descriptor () =
	let rec read_vol_desc n =
		let data = read_block n in
		let t = read1 data 0 in
		if (t = 255) then failwith "primary volume descriptor not found" else
		if (t != 1) then read_vol_desc (n+1) else
		(* we found pvd *)
		if ((read2 data 128) != sector_size) then failwith "logical blocksize != 2048, not supported" else
		let root = read_record data 156 in (* copy of directory root record *)
		assert (root.size = 34);
		root.sector

	in read_vol_desc 16

(* (string, next index or -1) *)
let next_name s i =
	let (j,k) = if String.contains_from s i '/'
	then let j = String.index_from s i '/' in (j-i,j+1)
	else (String.length s - i,-1) (* -1 : leaf, no more parts *)
	in (String.sub s i j, k)

(* path: must start with '/'; returns None or Some rec *)
let path_to_record path =
	if (path.[0] != '/') then begin eprintf "path_to_sector: path.[0] != '/'\n"; None end else
	if String.length path = 1 then Some (read_record (read_block !root_sec) 0) (* opendir '/' *) else
	let data = ref "" in

	let rec search sector path_index =
		let (name,i) = next_name path path_index in
		if String.length name = 0 then None else begin
			data := read_block sector;
			let r = read_record !data 0 in
			(* iterates through dir record list at one level, step down via calling 'search' *)
			let rec scan sector index length =
				if (read1 !data index = 0) then (* no more records in this sector *)
					if (length < sector_size) then begin (* no more records at all *)
						eprintf "path_to_sector: end of record list, but file '%s' not found\n" name; None
					end else begin (* read next sector, continue search *)
						let next_sector = sector + 1 in
						data := read_block next_sector;
						scan next_sector 0 (length-sector_size) 
					end
				else begin
					let r = read_record !data index in
					if String.compare name r.name = 0 then
						if i = -1 then Some r else search r.sector i
					else
						scan sector (index + r.size) length
				end

			in
				if (r.flags land 2 == 0) then None else scan sector 0 r.length
		end
	in
		search !root_sec 1

let lookup_file path = 
	match path_to_record path with
	| None -> (0,0)
	| Some r -> (r.sector,r.length)

let opendir path =
	match path_to_record path with
	| None -> None
	| Some r ->
		let data = read_block r.sector in
		let f_r = read_record data 0 in
		Some { r_sector = r.sector; r_offset = 0; r_length = r.length; f_rec = f_r }

let readdir r =
	let data = read_block r.r_sector in
	r.r_offset <- r.r_offset + r.f_rec.size;
	if (read1 data r.r_offset = 0) then begin
		if (r.r_length = sector_size) then false else begin
			r.r_sector <- r.r_sector + 1;
			r.r_offset <- 0;
			r.r_length <- r.r_length - sector_size;
			r.f_rec <- read_record (read_block r.r_sector) 0;
			true
		end
	end else begin
		r.f_rec <- read_record data r.r_offset;
		true
	end

let () =
	root_sec := read_primary_volume_descriptor ();

	Callback.register "lookup_file" lookup_file;
	Callback.register "opendir" opendir;
	Callback.register "readdir" readdir
