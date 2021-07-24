package L4::Image;

# More TODOs / Ideas:
# - implement safety/sanity checks
#   - DONE: i.e. that kernel+sigma0+moe are there
#   - no double modules, i.e. the same name multiple times
#   - verify the integrity of an image, e.g. checksums ok
# - list: give out different styles: (if considered useful)
#    - human text
#    - JSON
#    - just numbers, like --num-modules just gives out the number of
#      modules?
# - low-prio: implement patching for EFI files
# - low-prio: provide feature generate grub.cfg/menu.lst to eventually
#             replace genexportpack?
# - implement check that perl and C data structures match
#   - IMPORTANT
#   - first step: wrap pack und unpack in wrapper functions and place
#     them centrally in Image.pm so it's harder to miss a conversion
# - remove %ds{num_ds} in code, it's implicit through the array size
#
# - per-module key:value store:
#    - e.g.: - path to debug-info per module, e.g. a URL
#            - license info?
#    - Add way to specify this info at initial image generation time
#      to avoid repacking the image afterwards.
#      Looks like some perl-plugin could address this.
#      (For debug symbols we will add code anyway.)

use warnings;
use strict;
use Exporter;
use File::Basename;
use File::Temp qw/tempdir/;
use File::Copy qw/copy/;
use Digest::MD5;
use POSIX;
use L4::Image::Utils qw/error check_sysread check_syswrite
                        filepos_get filepos_set/;
use L4::Image::Elf;
use L4::Image::Raw;
use L4::Image::UImage;

BEGIN { unshift @INC, dirname($0).'../lib'; }

use vars qw(@ISA @EXPORT);
@ISA    = qw(Exporter);
@EXPORT = qw();

use constant {
  BOOTSTRAP_IMAGE_INFO_MAGIC => "<< L4Re Bootstrap Image Info >>",
  BOOTSTRAP_IMAGE_INFO_MAGIC_LEN => 32,
  TEMPLATE_IMAGE_INFO => "L<L<Q<Q<Q<Q<Q<Q<Q<", # without the MAGIC
  IMAGE_INFO_SIZE => 64, # without the MAGIC
  IMAGE_INFO_OFFSET_MOD_HEADER => 48,
  IMAGE_INFO_OFFSET_ATTR       => 56,

  TEMPLATE_MOD_HEADER => "A32L<L<q<q<",
  TEMPLATE_MOD_INFO   => "A32L<Q<L<L<q<q<q<q<q<",
  MOD_HEADER_SIZE     => 56,
  MOD_INFO_SIZE       => 92,
  BOOTSTRAP_MOD_INFO_MAGIC_HDR     => "<< L4Re-bootstrap-modinfo-hdr >>",
  BOOTSTRAP_MOD_INFO_MAGIC_HDR_LEN => 32,
  BOOTSTRAP_MOD_INFO_MAGIC_MOD     => "<< L4Re-bootstrap-modinfo-mod >>",
  BOOTSTRAP_MOD_INFO_MAGIC_MOD_LEN => 32,
};

my @arch_l4names = qw/x86 amd64
                      arm arm64
                      mips mips
                      ppc32 powerpc
                      sparc sparc
                      riscv riscv/;

use constant {
  FILE_TYPE_ERROR   => 0,
  FILE_TYPE_UNKNOWN => 1,
  FILE_TYPE_ELF     => 2,
  FILE_TYPE_UIMAGE  => 3,
};

use constant FILE_TYPES => qw(ERROR Unknown/Raw ELF uImage);

sub map_type_name_to_flag
{
  my $t = shift;
  return 0 unless defined $t;
  return 1 if $t eq 'kernel';
  return 2 if $t eq 'sigma0';
  return 3 if $t eq 'roottask';
  return 0;
};

sub page_size
{
  my $arch = shift;
  return 1 << 14 if $arch eq 'mips';
  return 1 << 12;
}

sub round_page
{
  my $ps = page_size($_[1]);
  return ($_[0] + $ps - 1) & ~($ps - 1);
}

sub get_file_type
{
  my $fn = shift;

  open(my $fd, $fn) || return (FILE_TYPE_ERROR, "Cannot open '$fn': $!");
  binmode $fd;

  my $buf;

  my $r = sysread($fd, $buf, 128);
  return (FILE_TYPE_ERROR, "Cannot read from '$fn'") unless defined $r;

  my @n = unpack("C4", $buf);

  printf "DEBUG: %x %x %x %x\n", $n[0], $n[1], $n[2], $n[3] if 0;

  my $type = FILE_TYPE_UNKNOWN;
  if ($n[0] == 0x7f and $n[1] == 0x45 and $n[2] == 0x4c and $n[3] == 0x46) {
    $type = FILE_TYPE_ELF;
  } elsif ($n[0] == 0x27 and $n[1] == 5 and $n[2] == 0x19 and $n[3] == 0x56) {
    $type = FILE_TYPE_UIMAGE;
  }

  close $fd;

  return $type;
}

# TODO: Add something with compression
sub fill_module
{
  my $m_file = shift;
  my $m_name = shift;
  my $m_type = shift;
  my $m_cmdline = shift;

  return ( error => "File '$m_file' does not exist" ) unless -e $m_file;

  my %d;

  $m_name = basename($m_file) unless defined $m_name;
  $m_type = 0 unless defined $m_type;
  $m_cmdline = $m_name unless defined $m_cmdline;

  $d{name} = $m_name;
  $d{cmdline} = $m_cmdline;
  $d{flags} = $m_type;
  $d{filepath} = $m_file;

  my $md5uncomp = Digest::MD5->new;
  my $ff;
  if (!open($ff, $m_file))
    {
      return ( error => "Failed to open '$m_file': $!" );
    }
  binmode $ff;
  $md5uncomp->addfile($ff);
  close $ff;
  $d{md5sum_compr}   = $md5uncomp->hexdigest;
  $d{md5sum_uncompr} = $md5uncomp->hexdigest;
  $d{size} = -s $m_file;
  $d{size_uncompressed} = -s $m_file;

  return %d;
}

sub update_module
{
  my $d = shift;
  my $m_file = shift;
  my $m_name = shift;
  my $m_type = shift;
  my $m_cmdline = shift;

  return ( error => "File '$m_file' does not exist" ) unless -e $m_file;

  $d->{name}     = $m_name if defined $m_name;
  $d->{cmdline}  = $m_cmdline if defined $m_cmdline;
  $d->{flags}    = map_type_name_to_flag($m_type) if defined $m_type;
  $d->{filepath} = $m_file;

  my $md5uncomp = Digest::MD5->new;
  my $ff;
  if (!open($ff, $m_file))
    {
      return ( error => "Failed to open '$m_file': $!" );
    }
  binmode $ff;
  $md5uncomp->addfile($ff);
  close $ff;
  $d->{md5sum_compr}   = $md5uncomp->hexdigest;
  $d->{md5sum_uncompr} = $md5uncomp->hexdigest;
  $d->{size} = -s $m_file;
  $d->{size_uncompressed} = -s $m_file;
}

sub check_modules
{
  my (%ds) = @_;

  my $num_modules = scalar @{$ds{mods}} || 0;
  my @saw;
  for (my $i = 0; $i < $num_modules; ++$i)
    {
      $saw[$ds{mods}[$i]{flags} & 7]++;
    }

  return "No kernel found" if not defined $saw[1];
  return "No sigma0 found" if not defined $saw[2];
  return "No roottask found" if not defined $saw[3];
  return "Only one kernel allowed" if $saw[1] > 1;
  return "Only one sigma0 allowed" if $saw[2] > 1;
  return "Only one roottask allowed" if $saw[3] > 1;

  return undef;
}


# KVS layout
# - Header: "ATTR"
# - key-value pair
#   - descriptor, 1 byte
#   - len of key, number of bytes according to size in descriptor
#   - len of value number of bytes according to size in descriptor
#   - key data, number of bytes according to "len of key"
#   - value data, number of bytes according to "len of value"
#
# type: 1-byte
#       bit7: 0=done, end of KVS
#             1=entry
#       bit0-1: 2^x encoded len of size field for key (possible 1, 2, 4, 8)
#       bit2-3: 2^x encoded len of size field for value (possible 1, 2, 4, 8)

sub read_attrs
{
  my $fd = shift;
  my $offset = shift;
  my %d;
  my $buf;

  printf "attrs-offset: %x\n", $offset if 0;

  filepos_set($fd, $offset);
  sysread($fd, $buf, 4);
  my $attr = unpack("A4", $buf);
  return %d if $attr ne 'ATTR';

  while (1)
    {
      my $r = sysread($fd, $buf, 1);
      return %d if $r != 1;

      my $type = unpack("C", $buf);

      if (($type & 0x80) == 0)
        {
          print "attrs done\n" if 0;
          return %d;
        }

      my $key_field_exp = (($type >> 0) & 3);
      my $val_field_exp = (($type >> 2) & 3);
      my $key_field_len = 1 << $key_field_exp;
      my $val_field_len = 1 << $val_field_exp;

      print "key_field_len=$key_field_len val_field_len=$val_field_len\n" if 0;

      my @packval = qw/C S< L< Q</;
      my $pack_pattern = $packval[$key_field_exp].$packval[$val_field_exp];

      sysread($fd, $buf, $key_field_len + $val_field_len);

      my ($key_len, $val_len) = unpack($pack_pattern, $buf);

      print "key_len=$key_len val_len=$val_len\n" if 0;

      sysread($fd, my $key, $key_len);
      sysread($fd, my $val, $val_len);

      print "=> $key: $val\n" if 0;
      $d{$key} = $val;
    }
}

sub write_attrs
{
  my $fd = shift;
  my %kvs = @_;

  return 0 if keys %kvs == 0;

  print "Number of attrs to write out: ", int(keys %kvs), "\n" if 0;

  sub field_len
  {
    my $l = shift;
    return 0 if $l < (1 << 8);
    return 1 if $l < (1 << 16);
    return 2 if $l < (1 << 32);
    return 3;
  }

  my $sum = 4;

  syswrite($fd, "ATTR", 4);

  foreach my $key (sort keys %kvs)
    {
      my @packval = qw/C S< L< Q</;
      my $key_len = length($key);
      my $val = $kvs{$key} || '';
      my $val_len = length($val);
      my $key_field_exp = field_len($key_len);
      my $val_field_exp = field_len($val_len);

      #print "Attr-write: $key = $val\n";

      my $type = 0x80 | $key_field_exp | ($val_field_exp << 2);

      my $buf = pack("C".
                     $packval[$key_field_exp].
                     $packval[$val_field_exp].
                     "a$key_len".
                     "a$val_len",
                     $type, $key_len, $val_len, $key, $val);

      syswrite($fd, $buf, length($buf));

      $sum += 1 + (1 << $key_field_exp) + (1 << $val_field_exp) +
              $key_len + $val_len;
    }

  # write end marker
  $sum++;
  syswrite($fd, "\0", 1);

  return $sum;
}

sub find_image_info
{
  my $fd = shift;

  my $buf;
  filepos_set($fd, 0);
  my $r = sysread($fd, $buf, 1 << 20); # would we be interrupted?
  return(0, "Could not read from file: $!") unless $r;

  my $pos = index($buf, BOOTSTRAP_IMAGE_INFO_MAGIC);
  return(0, "Did not find image info") if $pos == -1;

  #printf "Found image info at 0x%x\n", $pos;

  return $pos;
}

sub process_image
{
  my $fn = shift;
  my $opts = shift;
  my $cb = shift;

  my ($file_type, $err) = get_file_type($fn);
  return $err if $err;
  my $img;
  if ($file_type == FILE_TYPE_ELF)
    {
      # We parse ELF images here already since the actual file is changed
      # if an ELF64 bootstrap needs to be unwrapped from an ELF binary as is the
      # case for amd64.
      $img = L4::Image::Elf->new($fn);
      my $inner_fh = $img->inner_elf();
      if (defined $inner_fh)
        {
          my $inner_img = L4::Image::Elf->new($inner_fh->filename);
          $inner_img->{'wrapper-fn'} = $fn;
          $inner_img->{'wrapper-img'} = $img;
          $img = $inner_img;
          $fn = $inner_fh->filename;
        }
    }

  my $workdir;

  if ($opts->{mods_to_disk})
    {
      $workdir = $opts->{workdir};
      if (not defined $workdir)
        {
          $workdir = tempdir('XXXXXXXX', CLEANUP => 1, TMPDIR => 1)
        }
      elsif (not -d $workdir)
        {
          system("mkdir -p \"$workdir\"");
          die "Cannot create '$workdir'" if $?;
        }
    }

  open(my $fd, "<$fn") || return("Could not open '$fn': $!");
  binmode $fd;

  my ($image_info_file_pos, $error_text) = find_image_info($fd);
  error($error_text) if defined $error_text;

  filepos_set($fd, $image_info_file_pos + BOOTSTRAP_IMAGE_INFO_MAGIC_LEN);

  my $buf;
  my $r = sysread($fd, $buf, IMAGE_INFO_SIZE);
  return("Could not read from binary ($r)")
    if not defined $r or $r != IMAGE_INFO_SIZE;
  my ($crc32, $structure_version, $_flags, $_start, $_end,
      $_module_data_start, $bin_addr_end_bin, $mod_header, $attrs_offset)
    = unpack(TEMPLATE_IMAGE_INFO, $buf);

  printf "crc32=%x structure_version=%x _flags=%x _start=%x _end=%x ".
         "_module_data_start=%x bin_addr_end_bin=%x mod_header=%x attrs=%x\n",
         $crc32, $structure_version, $_flags, $_start, $_end,
         $_module_data_start, $bin_addr_end_bin, $mod_header, $attrs_offset
    if 0;

  # See L4::Image::Elf module for a description of the interface.
  if ($file_type == FILE_TYPE_UIMAGE) {
    $img = L4::Image::UImage->new($fn, $_start);
  } elsif ($file_type == FILE_TYPE_ELF) {
    # Already parsed above due to unwrapping ...
  } else {
    $img = L4::Image::Raw->new($fn, $_start);
  }

  my $file_attrshdr_start = $img->vaddr_to_file_offset($_start + $attrs_offset)
    if $attrs_offset;

  my %d;
  if ($mod_header == 0)
    {
      print "Found empty image\n";
      $d{num_mods}    = 0;
      $d{mbi_cmdline} = "";
      $d{flags}       = 0;
    }
  else
    {
      my $file_modhdr_start = $img->vaddr_to_file_offset($_start + $mod_header);
      error("Virtual offset " . ($_start + $mod_header) . " not found in binary")
        unless defined $file_modhdr_start;
      %d = import_modules($fd, $file_modhdr_start, $workdir);
      return($d{error}) if defined $d{error};
    }

  $d{image_info_file_pos} = $image_info_file_pos;
  $d{structure_version} = $structure_version;
  $d{crc32}             = $crc32;
  $d{image_flags}       = $_flags;
  $d{attrs}             = { read_attrs($fd, $file_attrshdr_start) }
    if defined $file_attrshdr_start;
  $d{file_type}         = $file_type;

  my $archval_with_width = $_flags & 0x1f;
  $d{arch} = $arch_l4names[$archval_with_width];
  $d{arch} = "Unknown" unless defined $d{arch};

  close($fd);

  # Apply operation
  $cb->(\%d, @_);

  if ($opts->{gen_new_image})
    {
      my $checkresult = check_modules(%d);
      return $checkresult if $checkresult;

      my $ofn = $fn;
      $ofn = $img->{'wrapper-fn'} if exists $img->{'wrapper-fn'};
      $ofn = $opts->{outimagefile} if defined $opts->{outimagefile};
      my $tmp_ofn = $ofn . ".tmp";

      my $ofd = $img->objcpy_start($_module_data_start, $tmp_ofn);
      my $module_start_pos = filepos_get($ofd);
      my %offsets = export_modules($ofd, %d);
      my $module_end_pos = filepos_get($ofd);

      write_image_info($ofd, $image_info_file_pos,
                       $_module_data_start - $_start,  %offsets);

      # Update optional pointer to _module_data_end
      if (($d{arch} eq 'arm' or $d{arch} eq 'arm64')
          and $bin_addr_end_bin)
        {
          my $_module_data_end = $_module_data_start + $module_end_pos -
                                 $module_start_pos;
          $r = filepos_set($ofd, $img->vaddr_to_file_offset($bin_addr_end_bin));
          check_syswrite(syswrite($ofd, pack("Q<", $_module_data_end)), 8);
        }

      $img->objcpy_finalize();
      $img->dispose();
      if (exists $img->{'wrapper-img'})
        {
          # If we unwrapped the binary, then rewrap it here
          my $tmp_ofn_outer = $tmp_ofn . ".outer";
          my $outer = $img->{'wrapper-img'};
          printf "Patching from outer vaddr: 0x%x\n", $outer->{'inner-vaddr'} if 0;
          my $outer_fd = $outer->objcpy_start($outer->{'inner-vaddr'}, $tmp_ofn_outer);

          open(my $inner_fd, $tmp_ofn) || die "Cannot open $tmp_ofn for reading: $!";
          binmode $inner_fd;
          my $len;
          while ($len = sysread($inner_fd, my $buf, 4096))
            {
              syswrite($outer_fd, $buf, $len);
            }
          unlink $inner_fd;

          $outer->objcpy_finalize();
          $outer->dispose();
          rename($tmp_ofn_outer, $ofn) || error("Could not rename output file!");
        }
      else
        {
          rename($tmp_ofn, $ofn) || error("Could not rename output file!");
        }
    }
  else
    {
      $img->dispose();
    }

  return undef;
}

sub creation_info
{
  my $tag = shift;

  my %a;
  $a{"l4i:$tag-date"} = strftime("%Y-%m-%d %T %Z%z", localtime());
  my $hostname = `hostname`;
  chomp $hostname;
  $a{"l4i:$tag-host"} = $hostname;
  if ($^O eq 'MSWin32')
    {
      $a{"l4i:$tag-user"} = $ENV{USERNAME};
      $a{"l4i:$tag-name"} = '';
    }
  else
    {
      my @u = getpwuid($<);
      $a{"l4i:$tag-user"} = $u[0];
      $a{"l4i:$tag-name"} = (split /,/, $u[6])[0];
    }
  return %a;
}

sub import_modules
{
  my $fd = shift;
  my $offset_mod_header = shift;
  my $file_store_path = shift;

  printf "mod-offset: %x\n", $offset_mod_header if 0;

  filepos_set($fd, $offset_mod_header);

  my %ds;

  my $hdr;
  check_sysread(sysread($fd, $hdr, MOD_HEADER_SIZE), MOD_HEADER_SIZE);

  ($ds{magic}, $ds{num_mods}, $ds{flags}, $ds{mbi_cmdline}, $ds{mods_offset})
    = unpack(TEMPLATE_MOD_HEADER, $hdr);

  return ('error' => "Invalid module info file header at file offset $offset_mod_header")
    if $ds{magic} ne BOOTSTRAP_MOD_INFO_MAGIC_HDR;

  my @mods;
  for (my $i = 0; $i < $ds{num_mods}; ++$i)
    {
      $mods[$i]{image_filepos} = MOD_HEADER_SIZE + $i * MOD_INFO_SIZE;
      my $buf;
      check_sysread(sysread($fd, $buf, MOD_INFO_SIZE), MOD_INFO_SIZE);

      my $magic;
      ($magic, $mods[$i]{flags}, $mods[$i]{start},
       $mods[$i]{size}, $mods[$i]{size_uncompressed},
       $mods[$i]{name}, $mods[$i]{cmdline},
       $mods[$i]{md5sum_compr}, $mods[$i]{md5sum_uncompr},
       $mods[$i]{filepos_attr})
        = unpack(TEMPLATE_MOD_INFO, $buf);

      return ('error' => 'Invalid module header')
        if $magic ne BOOTSTRAP_MOD_INFO_MAGIC_MOD;
    }

  # TODO should we check for addressing mode here? or at least enforce that
  # we only have relative addressing popping up here

  my $hdrlen = MOD_HEADER_SIZE + $ds{num_mods} * MOD_INFO_SIZE;

  my $data;
  # read up to 100kb mbi-info, eof might be earlier for empty images
  sysread($fd, $data, 100000);

  $ds{mbi_cmdline} = unpack('Z*', substr($data, $ds{mbi_cmdline} - $hdrlen));

  my $opos = filepos_get($fd);
  for (my $i = 0; $i < $ds{num_mods}; ++$i)
    {
      if ($mods[$i]{filepos_attr})
        {
          my $o = $offset_mod_header + MOD_HEADER_SIZE + $i * MOD_INFO_SIZE;
          $mods[$i]{attrs} = { read_attrs($fd, $o + $mods[$i]{filepos_attr}) };
        }
    }
  filepos_set($fd, $opos);

  my $modules_list = "# vim:set ft=l4mods:\n";
  $modules_list .= "\n";
  $modules_list .= "entry image\n";
  $modules_list .= "bootstrap $ds{mbi_cmdline}\n";

  if (defined $file_store_path)
    {
      die "Directory '$file_store_path' does not exist"
        unless -d $file_store_path;
    }

  for (my $i = 0; $i < $ds{num_mods}; ++$i)
    {
      my $offs = MOD_HEADER_SIZE + $i * MOD_INFO_SIZE - $hdrlen;

      $mods[$i]{name}           = unpack('Z*', substr($data, $mods[$i]{name} + $offs));
      $mods[$i]{cmdline}        = unpack('Z*', substr($data, $mods[$i]{cmdline} + $offs));
      $mods[$i]{md5sum_compr}   = unpack('Z*', substr($data, $mods[$i]{md5sum_compr} + $offs));
      $mods[$i]{md5sum_uncompr} = unpack('Z*', substr($data, $mods[$i]{md5sum_uncompr} + $offs));

      $mods[$i]{fileoffset} = $offset_mod_header + $mods[$i]{start} + MOD_HEADER_SIZE + $i * MOD_INFO_SIZE;

      if (defined $file_store_path)
        {
          die "'name' in mod$i contains path elements" if $mods[$i]{name} =~ m,/,;
          my $modfn = "$file_store_path/$mods[$i]{name}";
          open(my $filefd, ">$modfn") || die "Cannot open $modfn for writing: $!";
          binmode $filefd;

          my $buf;
          my $r;
          my $sz = $mods[$i]{size};
          filepos_set($fd, $mods[$i]{fileoffset});
          do
            {
              my $l = $sz;
              my $chunk_size = 1 << 20;
              $l = $chunk_size if $l > $chunk_size;

              $r = sysread($fd, $buf, $l);
              die "Could not read" unless defined $r;

              check_syswrite(syswrite($filefd, $buf, $r), $r) if $r;
              $sz -= $r;
            }
          while ($r);

          close $filefd;

          $mods[$i]{filepath} = $modfn;

          my $type = $mods[$i]{flags} & 7;

          my $s = $mods[$i]{cmdline};
          $s =~ s/^\S+//;
          $s = $mods[$i]{name}.$s;
          if ($type == 1) {
            $modules_list .= "kernel $s\n";
          } elsif ($type == 2) {
            $modules_list .= "sigma0 $s\n";
          } elsif ($type == 3) {
            $modules_list .= "roottask $s\n";
          } else {
            $modules_list .= "module $s\n";
          }
        }
    }

  $ds{mods} = [ @mods ];

  if (defined $file_store_path and -d $file_store_path)
    {
      open(my $ml, ">$file_store_path/modules.list") || die "Cannot open modules.list for writing: $!";
      print $ml $modules_list;
      close $ml;
    }


  return %ds;
}

sub write_image_info
{
  my $fd = shift;
  my $image_info_file_pos = shift;
  my $data_area_start_offset = shift;
  my %offsets = @_;

  printf "image_info_file_pos = %x\n", $image_info_file_pos if 0;

  if (defined $offsets{attrs})
    {
      filepos_set($fd, $image_info_file_pos
                       + BOOTSTRAP_IMAGE_INFO_MAGIC_LEN
                       + IMAGE_INFO_OFFSET_ATTR);
      syswrite($fd, pack("q<", $data_area_start_offset + $offsets{attrs}), 8);

      printf "data_area_start_offset=%x\n", $data_area_start_offset if 0;
      printf "offsets.attrs=%x\n", $offsets{attrs} if 0;
    }

  if (defined $offsets{mod_header})
    {
      filepos_set($fd, $image_info_file_pos
                       + BOOTSTRAP_IMAGE_INFO_MAGIC_LEN
                       + IMAGE_INFO_OFFSET_MOD_HEADER);
      syswrite($fd, pack("q<", $data_area_start_offset + $offsets{mod_header}), 8);
      printf "data_area_start_offset=%x\n", $data_area_start_offset if 0;
      printf "offsets.mod_header=%x\n", $offsets{mod_header} if 0;
    }
}

sub store_add_info
{
  my $stor = shift;
  my $infodata = shift;
  $$stor{mod_info_payload} .= $infodata;
  $$stor{mod_info_payload_offset} += length($infodata);
}

sub store_add_string
{
  my $stor = shift;
  my $tmp = pack('Z*', shift);
  my $o = length($$stor{data_payload});
  $$stor{data_payload} .= $tmp;
  return $o;
}

sub store_mod_off
{
  my $stor = shift;
  my $o = shift;
  return $o - $$stor{mod_info_payload_offset};
}

sub store_file_add
{
  my $stor = shift;
  my $file = shift;
  my $arch = shift;
  my $o = $$stor{files_offset};
  push @{$$stor{files}}, $file;
  push @{$$stor{files_offsets}}, $o;
  $$stor{files_offset} = round_page($$stor{files_offset} + -s $file, $arch);
  return $o;
}

sub export_modules
{
  my ($fd, %ds) = @_;

  print "Writing out image\n" if 0;

  my $initial_file_pos = filepos_get($fd);

  my %offsets;

  my %a = L4::Image::creation_info("modification");
  $ds{attrs}{$_} = $a{$_} foreach keys %a;

  # write out attrs
  my $r = write_attrs($fd, %{$ds{attrs}});
  $offsets{attrs} = 0 if $r;

  for (my $i = 0; $i < $ds{num_mods}; ++$i)
    {
      my $filepos_attr = filepos_get($fd);
      $r = write_attrs($fd, %{$ds{mods}[$i]{attrs}});
      delete $ds{mods}[$i]{filepos_attr};
      $ds{mods}[$i]{filepos_attr} = $filepos_attr - $initial_file_pos if $r;
      printf "mod$i: attrs at 0x%x\n", $ds{mods}[$i]{filepos_attr}
        if 0 and defined $ds{mods}[$i]{filepos_attr};
    }

  # ---------------------------------------------------

  my $filestartpos = filepos_get($fd);
  die "Cannot get file position" unless defined $filestartpos;
  printf "filestartpos=%x\n", $filestartpos if 0;

  my %stor;

  $ds{flags} = 0 unless defined $ds{flags};

  $ds{flags} &= ~(1 << 4); # we use relative addressing

  print "flags: $ds{flags}\n" if 0;
  print "num_mods: $ds{num_mods}\n" if 0;

  my $size_attrs = 0;

  # round=0: gather how much payload data is there
  # round=1: use fileoffset with proper value according to known payload size
  my $mods_start = $filestartpos - $initial_file_pos;
  $stor{files_offset} = $mods_start;
  printf "files_offset: %x\n", $stor{files_offset} if 0;
  for (my $round = 0; $round < 2; ++$round)
    {
      my $hdrlen = MOD_HEADER_SIZE + $ds{num_mods} * MOD_INFO_SIZE;
      $stor{mod_info_payload_offset} = 0;

      $stor{data_payload} = '';
      $stor{mod_info_payload} = '';
      my $hdrdata = pack(TEMPLATE_MOD_HEADER,
                         BOOTSTRAP_MOD_INFO_MAGIC_HDR,
                         $ds{num_mods},
                         $ds{flags},
                         $hdrlen + store_add_string(\%stor,
                                                    $ds{mbi_cmdline} || ''),
                         MOD_HEADER_SIZE); # offset to mods
      store_add_info(\%stor, $hdrdata);

      for (my $i = 0; $i < $ds{num_mods}; ++$i)
        {
          die "No on-disk file for mod$i" unless defined $ds{mods}[$i]{filepath};
          die "No such file '$ds{mods}[$i]{filepath}'"
            unless -e $ds{mods}[$i]{filepath};

          my $fileoffset = store_mod_off(\%stor,
                                         store_file_add(\%stor,
                                                        $ds{mods}[$i]{filepath},
                                                        $ds{arch}))
                           - $mods_start;

          $ds{mods}[$i]{flags} &= ~(1 << 4); # we use relative addressing

          printf "mod%d: fileoffset=%x %x/%d\n",
                 $i, $fileoffset,
                 $stor{mod_info_payload_offset},
                 $stor{mod_info_payload_offset} if 0;

          my $attr_val = 0;
          $attr_val = -(($filestartpos - $initial_file_pos)
                        - $ds{mods}[$i]{filepos_attr}
                        + $stor{mod_info_payload_offset})
            if defined $ds{mods}[$i]{filepos_attr};

          my $modinfodata = pack(TEMPLATE_MOD_INFO,
                                 BOOTSTRAP_MOD_INFO_MAGIC_MOD,
                                 $ds{mods}[$i]{flags},
                                 $fileoffset, # start
                                 $ds{mods}[$i]{size},
                                 $ds{mods}[$i]{size_uncompressed},
                                 store_mod_off(\%stor, $hdrlen + store_add_string(\%stor, $ds{mods}[$i]{name})),
                                 store_mod_off(\%stor, $hdrlen + store_add_string(\%stor, $ds{mods}[$i]{cmdline})),
                                 store_mod_off(\%stor, $hdrlen + store_add_string(\%stor, $ds{mods}[$i]{md5sum_compr})),
                                 store_mod_off(\%stor, $hdrlen + store_add_string(\%stor, $ds{mods}[$i]{md5sum_uncompr})),
                                 $attr_val
                               );
          store_add_info(\%stor, $modinfodata);
        }

      if ($round == 0)
        {
          my $o = round_page(length($stor{data_payload})
                             + length($stor{mod_info_payload})
                             + $mods_start,
                             $ds{arch});
          undef %stor;
          printf "Files offset: %x\n", $o if 0;
          $stor{files_offset} = $o;
        }
    }

  print STDERR "Size of info_payload: ", length($stor{mod_info_payload}), "\n" if 0;
  print STDERR "Size of      payload: ", length($stor{data_payload}), "\n" if 0;
  print STDERR "End:                  ", $stor{files_offset}, "\n" if 0;

  $offsets{mod_header} = filepos_get($fd) - $initial_file_pos;

  check_syswrite(syswrite($fd, $stor{mod_info_payload}), length($stor{mod_info_payload}));
  check_syswrite(syswrite($fd, $stor{data_payload}), length($stor{data_payload}));

  for (my $i = 0; $i < $ds{num_mods}; ++$i)
    {
      print STDERR "$i: writeout: $stor{files}[$i] size=", -s $stor{files}[$i], "\n" if 0;

      my $fn = $stor{files}[$i];
      open(my $o, $fn) || die "Cannot open $fn: $!";
      binmode $o;

      my $sz = -s $fn;
      my $buf;

      my $clearto = $initial_file_pos + $stor{files_offsets}[$i];
      my $pos = filepos_get($fd);
      error("Internal error on file positions ($clearto, $pos)") if $clearto < $pos;
      if ($clearto > $pos)
        {
          printf("clearing to %x\n", $clearto) if 0;
          syswrite($fd, pack("C", 0) x ($clearto - $pos), $clearto - $pos);
        }

      do
        {
          my $l = $sz;
          my $chunk_size = 1 << 20;
          $l = $chunk_size if $l > $chunk_size;

          $r = sysread($o, $buf, $l);
          error("Could not read from $fn: $!") unless defined $r;

          check_syswrite(syswrite($fd, $buf, $r), $r) if $r;
          $sz -= $r;
        }
      while ($r);

      close $o;
    }

  return %offsets;
}

return 1;
