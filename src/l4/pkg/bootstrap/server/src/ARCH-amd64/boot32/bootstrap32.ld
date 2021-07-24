ENTRY(_start)

PHDRS
{
  sixtyfour PT_LOAD FLAGS(6);
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
  rodata PT_LOAD FLAGS(4);
}

SECTIONS
{
  . = 0x01000000;
  _image_start = .;
  .text :
  {
    KEEP(*(.multiboot2))
    *(.text .text.*)
    *(.rodata .rodata.*)
  } :text =0x9090

  .note.gnu.build-id : { *(.note.gnu.build-id) }

  .data :
  {
    *.o32 (.idt .idt.*)
    *.a (.idt .idt.*)
    *.o32 (.data .data.*)
    *.a (.data .data.*)
    *.o32 (.bss .bss.*)
    *.a (.bss .bss.*)
    *.o32 (COMMON)
    *.a (COMMON)
  } :data

  .data.elf64 :
  {
    bootstrap32.bin (.data)
  } :rodata

  _image_end = .;


  /* Value of DEFAULT_RELOC_amd64 */
  .sixtyfour 0x2d0000 :
  {
    . = . + 500000;
  } :sixtyfour

  /DISCARD/ : {
    *(.interp)
    *(.comment)
    *(.note)
    *(.eh_frame)
  }
}
