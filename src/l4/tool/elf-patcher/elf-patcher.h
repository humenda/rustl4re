

static int
Elf(patch_phdrs)(void *_elf,
    unsigned long long stack_addr,
    unsigned long long stack_size,
    unsigned long long kip_addr)
{
  Elf(Ehdr) *elf = (Elf(Ehdr)*)_elf;
  Elf(Phdr) *phdr = (Elf(Phdr)*)((char*)elf + elf->e_phoff);
  int phnum  = elf->e_phnum;
  int phsz   = elf->e_phentsize;
  unsigned long long kip_size = 0x1000;
  int be = elf->e_ident[EI_DATA] == ELFDATA2MSB;

  for (;phnum > 0; --phnum, phdr = (Elf(Phdr)*)((char*)phdr + phsz))
    {
      switch (phdr->p_type)
	{
	case PT_LOOS + 0x12:
	  // printf("found stack phdr...\n");
	  // printf("set addr=%llx size=%llx\n", stack_addr, stack_size);
	  host_to(be, stack_addr, phdr->p_vaddr);
	  host_to(be, stack_addr, phdr->p_paddr);
	  host_to(be, stack_size, phdr->p_memsz);
	  break;
	case PT_LOOS + 0x13:
	  // printf("found kip phdr...\n");
	  // printf("set addr=%llx size=%llx\n", kip_addr, kip_size);
	  host_to(be, kip_addr, phdr->p_vaddr);
	  host_to(be, kip_addr, phdr->p_paddr);
	  host_to(be, kip_size, phdr->p_memsz);
	  break;
	default:
	  break;
	}
    }
  return 0;
}

static int
Elf(patch_shdrs)(void *_elf, unsigned long long min_align)
{
  Elf(Ehdr) *elf   = (Elf(Ehdr)*)_elf;
  int be = elf->e_ident[EI_DATA] == ELFDATA2MSB;
  Elf32_Off shoff  = host_to(be, elf->e_shoff, shoff);
  Elf32_Half shnum = host_to(be, elf->e_shnum, shnum);
  Elf32_Half shsz  = host_to(be, elf->e_shentsize, shsz);

  Elf(Shdr) *shdr = (Elf(Shdr)*)((char*)elf + shoff);
  for (;shnum > 0; --shnum, shdr = (Elf(Shdr)*)((char*)shdr + shsz))
    {
      if (shdr->sh_type == SHT_PROGBITS)
	{
          if (shdr->sh_addralign < min_align)
            host_to(be, min_align, shdr->sh_addralign);
	}
    }
  return 0;
}
