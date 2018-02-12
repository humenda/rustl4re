ENTRY(_start)
SECTIONS
{
  . = 0x01000000;
  _image_start = .;
  .text :
  {
    KEEP(*(.multiboot2))
    *(.text .text.*)
    *(.rodata .rodata.*)
  } =0x9090

  .data :
  {
    *(.data .data.*)
    *(.bss .bss.*)
    *(COMMON)
  }
  _image_end = .;

  /* Value of DEFAULT_RELOC_amd64 */
  .sixtyfour 0x2d0000 :
  {
    . = . + 500000;
  }

  /DISCARD/ : {
    *(.interp)
    *(.comment)
    *(.note)
    *(.eh_frame)
  }
}
