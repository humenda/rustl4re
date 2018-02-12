SECTIONS
{
	.text :
	{
		. = ALIGN(4096);
		vgPlain_l4re_trampoline_stuff_start = . ;
		KEEP(*(.l4re_vg_trampoline))
		. = ALIGN(4096);
		vgPlain_l4re_trampoline_stuff_end = . ;
	}
}
