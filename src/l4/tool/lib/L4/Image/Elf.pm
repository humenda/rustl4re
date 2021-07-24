# The modules in the ELF file are expected to occupy a dedicated section.

package L4::Image::Elf;

use warnings;
use strict;
use Exporter;
use L4::Image::Utils qw/error check_sysread check_syswrite checked_sysseek
                        sysreadz filepos_set/;
use File::Temp ();

use vars qw(@ISA @EXPORT);
@ISA    = qw(Exporter);
@EXPORT = qw();

use constant {
  ELFCLASS32 => 1,
  ELFCLASS64 => 2,

  ELFDATA2LSB => 1,
  ELFDATA2MSB => 2,

  SHT_NULL => 0x00,
  SHT_NOBITS => 0x08,
};


# Like CORE::pack but takes care of the ELF file endianess
sub pack
{
  my $self = shift;
  my $pattern = shift;
  my @args = @_;

  if ($self->{'data'} == ELFDATA2LSB) {
    $pattern = "(" . $pattern . ")<";
  } else {
    $pattern = "(" . $pattern . ")>";
  }

  return pack($pattern, @args);
}

# Like CORE::unpack but takes care of the ELF file endianess
sub unpack
{
  my $self = shift;
  my $pattern = shift;
  my $data = shift;

  if ($self->{'data'} == ELFDATA2LSB) {
    $pattern = "(" . $pattern . ")<";
  } else {
    $pattern = "(" . $pattern . ")>";
  }

  return unpack($pattern, $data);
}

# Read PHDR from the file and return reference to it
sub read_phdr
{
  my $self = shift;

  my $phdr;
  my ($p_type, $p_offset, $p_vaddr, $p_paddr, $p_filesz, $p_memsz, $p_flags,
    $p_align);
  check_sysread(sysread($self->{'fd'}, $phdr, $self->{'e_phentsize'}),
                $self->{'e_phentsize'});

  if ($self->{'class'} == ELFCLASS32) {
    ($p_type, $p_offset, $p_vaddr, $p_paddr, $p_filesz, $p_memsz, $p_flags,
      $p_align) = $self->unpack("LLLLLLLL", $phdr);
  } else {
    ($p_type, $p_flags, $p_offset, $p_vaddr, $p_paddr, $p_filesz, $p_memsz,
      $p_align) = $self->unpack("LLQQQQQQ", $phdr);
  }

  return {
    adjust => 1,

    type => $p_type,
    flags => $p_flags,
    offset => $p_offset,
    vaddr => $p_vaddr,
    paddr => $p_paddr,
    filesz => $p_filesz,
    memsz => $p_memsz,
    align => $p_align,
  };
}

# Write PHDR at current file position
sub write_phdr
{
  my $self = shift;
  my $p = shift;

  my $phdr;
  if ($self->{'class'} == ELFCLASS32) {
    $phdr = $self->pack("LLLLLLLL", $p->{'type'}, $p->{'offset'}, $p->{'vaddr'},
                        $p->{'paddr'}, $p->{'filesz'}, $p->{'memsz'},
                        $p->{'flags'}, $p->{'align'});
  } else {
    $phdr = $self->pack("LLQQQQQQ", $p->{'type'}, $p->{'flags'}, $p->{'offset'},
                        $p->{'vaddr'}, $p->{'paddr'}, $p->{'filesz'},
                        $p->{'memsz'}, $p->{'align'});
  }

  check_syswrite(syswrite($self->{'fd'}, $phdr), length($phdr));
}

# Read section header from file and return reference to it
sub read_shdr
{
  my $self = shift;

  my $shdr;
  my ($sh_name, $sh_type, $sh_flags, $sh_addr, $sh_offset, $sh_size);
  check_sysread(sysread($self->{'fd'}, $shdr, $self->{'e_shentsize'}),
                $self->{'e_shentsize'});

  if ($self->{'class'} == ELFCLASS32) {
    ($sh_name, $sh_type, $sh_flags, $sh_addr, $sh_offset, $sh_size) =
      $self->unpack("LLLLLL", $shdr);
  } else {
    ($sh_name, $sh_type, $sh_flags, $sh_addr, $sh_offset, $sh_size) =
      $self->unpack("LLQQQQ", $shdr);
  }

  return {
    adjust => ($sh_type != SHT_NULL and $sh_type != SHT_NOBITS),

    name => $sh_name,
    type => $sh_type,
    flags => $sh_flags,
    vaddr => $sh_addr,
    offset => $sh_offset,
    filesz => $sh_size,
  };
}

# Write section header at current file position
sub write_shdr
{
  my $self = shift;
  my $s = shift;

  my $shdr;

  if ($self->{'class'} == ELFCLASS32) {
    $shdr = $self->pack("LLLLLL", $s->{'name'}, $s->{'type'}, $s->{'flags'},
      $s->{'vaddr'}, $s->{'offset'}, $s->{'filesz'});
  } else {
    $shdr = $self->pack("LLQQQQ", $s->{'name'}, $s->{'type'}, $s->{'flags'},
      $s->{'vaddr'}, $s->{'offset'}, $s->{'filesz'});
  }

  check_syswrite(syswrite($self->{'fd'}, $shdr), length($shdr));
}

# Read PHDR table if not done so already.
sub read_phdrs
{
  my $self = shift;
  if (defined $self->{'phdr'}) {
    return;
  }

  my @phdr = ();

  filepos_set($self->{'fd'}, $self->{'e_phoff'});
  for (my $i = 0; $i < $self->{'e_phnum'}; $i++) {
    push @phdr, $self->read_phdr();
  }

  $self->{'phdr'} = \@phdr;
}

# Write PHDR table
sub write_phdrs
{
  my $self = shift;

  my $i = 0;
  foreach (@{$self->{'phdr'}})
    {
      my $off = $self->{'e_phoff'} + $self->{'e_phentsize'} * $i;
      filepos_set($self->{'fd'}, $off);
      $self->write_phdr($_);
      $i++;
    }
}

# Read section headers if not done so already
sub read_shdrs
{
  my $self = shift;
  if (defined $self->{'shdr'}) {
    return;
  }

  my @shdr = ();

  filepos_set($self->{'fd'}, $self->{'e_shoff'});
  for (my $i = 0; $i < $self->{'e_shnum'}; $i++) {
    push @shdr, $self->read_shdr();
  }

  $self->{'shdr'} = \@shdr;
}

# Write section headers table
sub write_shdrs
{
  my $self = shift;

  my $i = 0;
  foreach (@{$self->{'shdr'}})
    {
      my $off = $self->{'e_shoff'} + $self->{'e_shentsize'} * $i;
      filepos_set($self->{'fd'}, $off);
      $self->write_shdr($_);
      $i++;
    }
}

# Patch a PHDR/SHDR entry
#
# Parameters: $entry: Reference to entry that should be patched
#             $offset: The file offset where data is removed/inserted
#             $amount: Number of bytes inserted/removed after $offset
#
# Sections that are in the file before $offset are obviously unaffected. All
# sections that start after $offset are moved by $amount. The sections that
# span $offset are adjusted in size accordingly.
sub patch_entry
{
  my $entry = shift;
  my $offset = shift;
  my $amount = shift;

  # Must not be adjusted. Typically a SHT_NOBITS section.
  return unless $entry->{'adjust'};

  if ($entry->{'offset'} > $offset)
    {
      # Entry is in file after patched location -> adjust location
      $entry->{'offset'} += $amount;
    }
  elsif (($entry->{'offset'} + $entry->{'filesz'}) > $offset)
    {
      # Entry is covering patched location -> adjust size
      $entry->{'filesz'} += $amount;
      $entry->{'memsz'} += $amount if defined $entry->{'memsz'};
    }
}

# Patch an ELF file to accommodate for some inserted/removed bytes in a
# section.
#
# Parameters: $offset: The file offset where data is removed/inserted
#             $amount: Number of bytes inserted/removed after $offset
#
# It is assumed that the data was already inserted/removed but the ELF data
# structures are not yet adapted. This method will fix the ELF header and
# section tables to fit again the inserted/removed data.
sub patch
{
  my $self = shift;
  my $offset = shift;
  my $amount = shift;

  # Fixup offsets of PHDRs and SHDRs
  if ($self->{'e_phoff'} >= $offset) {
    $self->{'e_phoff'} += $amount;
  }
  if ($self->{'e_shoff'} >= $offset) {
    $self->{'e_shoff'} += $amount;
  }

  # Write back ELF header.
  if ($self->{'class'} == ELFCLASS32)
    {
      filepos_set($self->{'fd'}, 0x1C);
      check_syswrite(syswrite($self->{'fd'},
                              $self->pack("LL",
                                          $self->{'e_phoff'},
                                          $self->{'e_shoff'})),
                     8);
    }
  else
    {
      filepos_set($self->{'fd'}, 0x20);
      check_syswrite(syswrite($self->{'fd'},
                              $self->pack("QQ",
                                          $self->{'e_phoff'},
                                          $self->{'e_shoff'})),
                     16);
    }

  # Read *HDRS from their new location
  $self->read_phdrs();
  $self->read_shdrs();

  # Fixup entries to match the new file layout
  foreach (@{$self->{'phdr'}}) {
    patch_entry($_, $offset, $amount);
  }
  foreach (@{$self->{'shdr'}}) {
    patch_entry($_, $offset, $amount);
  }

  # Write'em back
  $self->write_phdrs();
  $self->write_shdrs();
}

#------------------------------------------------------------------------------

# Create Elf object from open file descriptor.
#
# Parameter: $fd: file opened for reading and optionally writing
#
# The object will take ownership of the file descriptor. It will close it when
# dispose() is invoked.
sub new_fd
{
  my $class = shift;
  my $fd = shift;

  my $elf_hdr;
  filepos_set($fd, 0);
  check_sysread(sysread($fd, $elf_hdr, 0x40), 0x40);
  my ($elf_magic, $elf_class, $elf_data, $elf_version) =
    CORE::unpack("a4CCC", $elf_hdr);

  die("Not an ELF file") unless $elf_magic eq "\x7fELF";
  die("Unknonw ELF version") unless $elf_version == 1;

  my $self = bless {
    fd => $fd,
    class => $elf_class,
    data => $elf_data,
  }, $class;

  my ($e_phoff, $e_shoff, $e_phentsize, $e_phnum, $e_shentsize, $e_shnum, $e_shstrndx);
  if ($elf_class == ELFCLASS32)
    {
      # 32-bit variant
      ($e_phoff, $e_shoff, $e_phentsize, $e_phnum, $e_shentsize, $e_shnum, $e_shstrndx) =
        $self->unpack("LLx6SSSSS", substr($elf_hdr, 0x1C));
    }
  elsif ($elf_class == ELFCLASS64)
    {
      # 64-bit variant
      ($e_phoff, $e_shoff, $e_phentsize, $e_phnum, $e_shentsize, $e_shnum, $e_shstrndx) =
        $self->unpack("QQx6SSSSS", substr($elf_hdr, 0x20));
    }
  else
    { die("Unknown ELF class"); }

  $self->{'e_phoff'}     = $e_phoff;
  $self->{'e_shoff'}     = $e_shoff;
  $self->{'e_phentsize'} = $e_phentsize;
  $self->{'e_phnum'}     = $e_phnum;
  $self->{'e_shentsize'} = $e_shentsize;
  $self->{'e_shnum'}     = $e_shnum;
  $self->{'e_shstrndx'}  = $e_shstrndx;

  return $self;
}

# Create Elf object from file name.
#
# Parameters: $fn: Name of file
#
# Will open the file read-only to query the ELF file.
sub new
{
  my $class = shift;
  my $fn = shift;

  open(my $fd, "<", $fn) || error("Could not open '$fn': $!");
  binmode($fd);

  return $class->new_fd($fd);
}

# This detects if bootstrap is wrapping another bootstrap image in a .data.elf64
# section. This is the case on amd64 where a 32-bit bootstrap is later loading a
# 64-bit bootstrap, but the 64-bit version is the one actually containing the
# modules of interest.
#
# Returns a tempfile object for the wrapped file which is deleted when going out
# of scope or 0 if this is not a wrapping bootstrap image.
sub inner_elf
{
  my $self = shift;
  return undef unless $self->{'class'} == ELFCLASS32;

  $self->read_shdrs();

  my $shstrn_off = $self->{'shdr'}[$self->{'e_shstrndx'}]->{'offset'};
  my $s = "";
  foreach my $hdr (@{$self->{'shdr'}})
    {
      my $section_name_offset = $shstrn_off + $hdr->{'name'};
      filepos_set($self->{'fd'}, $section_name_offset);

      $s = sysreadz($self->{'fd'});

      # The .data.elf64 section name denotes an inner bootstrap elf image
      if ($s eq '.data.elf64')
        {
          $self->{'inner-vaddr'} = $hdr->{'vaddr'};
          my $fh = File::Temp->new();
          open my $ofd, '>', $fh->filename or die "$fh->filename: $!";
          filepos_set($self->{'fd'}, $hdr->{'offset'});

          # Unpack nested file to tmpfile ...
          my $buf;
          check_sysread(sysread($self->{'fd'}, $buf, $hdr->{'filesz'}),
                        $hdr->{'filesz'});
          check_syswrite(syswrite($ofd, $buf, $hdr->{'filesz'}),
                         $hdr->{'filesz'});
          return $fh;
        }
    }

  return undef;
}

sub dispose
{
  my $self = shift;
  close($self->{'fd'});
}

# Convert virtual address to file offset.
#
# Will return undef if no section matched the virtual address.
sub vaddr_to_file_offset
{
  my $self = shift;
  my $vaddr = shift;

  $self->read_phdrs();

  foreach (@{$self->{'phdr'}})
    {
      if ($vaddr >= $_->{'vaddr'} and
          $vaddr < ($_->{'vaddr'} + $_->{'filesz'})) {
        return $vaddr - $_->{'vaddr'} + $_->{'offset'};
      }
    }

  return undef;
}

# Start to copy this ELF file to a new one, giving the user the possibility to
# replace the content of a section.
#
# Parameters: $until_vaddr: Virtual address of section to replace
#             $ofn: Name of output file
# Returns: file handle of output file
#
# The $ofn file is opened for writing and reading. All sections before the
# given $until_vaddr virtual address are copied already. The $until_vaddr
# address must match the beginning of a section exactly. The returned handle
# can be used to write the new content of that section and then call
# objcpy_finalize() to copy the remaining sections.
sub objcpy_start
{
  my $self = shift;
  my $until_vaddr = shift;
  my $ofn = shift;

  # Search for section matching the given virtual address. It is expected that
  # a whole section is replaced!
  $self->read_shdrs();
  my $offset;
  my $size;
  foreach (@{$self->{'shdr'}})
    {
      if ($_->{'vaddr'} == $until_vaddr) {
        $offset = $_->{'offset'};
        $size = $_->{'filesz'};
      }
    }
  error("Cannot find exact module section") unless $offset;

  open(my $ofd, "+>$ofn") || error("Could not open '$ofn': $!");
  binmode $ofd;

  my $ifd = $self->{'fd'};
  $self->{'ofd'} = $ofd;
  $self->{'upd_vaddr'} = $until_vaddr;
  $self->{'upd_file_start'} = $offset;
  $self->{'upd_file_end'} = $offset + $size;

  # copy initial part
  my $buf;
  filepos_set($ifd, 0);
  check_sysread(sysread($ifd, $buf, $offset), $offset);
  check_syswrite(syswrite($ofd, $buf), length($buf));

  return $ofd;
}

# Finish copying of ELF file.
#
# The user is expected to have written the new data of the section he wanted to
# replace. This method will copy the remaining sections and fix the ELF headers
# to match the new layout.
sub objcpy_finalize
{
  my $self = shift;
  my $ifd = $self->{'fd'};
  my $ofd = $self->{'ofd'};

  # Determine new size of patched section
  my $offset = checked_sysseek($ofd, 0, 2) - $self->{'upd_file_end'};

  # Copy remaining sections and headers
  my $buf;
  filepos_set($ifd, $self->{'upd_file_end'});
  sysread($ifd, $buf, 0xffffffff);
  check_syswrite(syswrite($ofd, $buf), length($buf));

  # Fix headers. The Elf object will take ownership of the file handle.
  my $out_elf = L4::Image::Elf->new_fd($ofd);
  $out_elf->patch($self->{'upd_file_start'}, $offset);
  $out_elf->dispose();
}

1;
