#! /usr/bin/env perl
#
# (c) 2008-2009 Technische Universität Dresden
# This file is part of TUD:OS and distributed under the terms of the
# GNU General Public License 2.
# Please see the COPYING-GPL-2 file for details.
#
# Adam Lackorzynski <adam@os.inf.tu-dresden.de>
#

use strict;
use warnings;

BEGIN { unshift @INC, $ENV{L4DIR}.'/tool/lib'
           if $ENV{L4DIR} && -d $ENV{L4DIR}.'/tool/lib/L4';}

use Digest::MD5;
use File::Basename;
use File::Path qw(make_path);
use POSIX;
use Math::BigInt;
use L4::ModList;
use L4::Image;


my $cross_compile_prefix = $ENV{CROSS_COMPILE} || '';
my $arch                 = $ENV{OPT_ARCH}     || "x86";
my $platform_type        = $ENV{OPT_PLATFORM_TYPE};

my $module_path   = $ENV{SEARCHPATH}    || ".";
my $prog_objcopy  = $ENV{OBJCOPY}       || "${cross_compile_prefix}objcopy";
my $prog_cc       = $ENV{CC}            || "${cross_compile_prefix}gcc";
my $prog_nm       = $ENV{NM}            || "${cross_compile_prefix}nm";
my $prog_cp       = $ENV{PROG_CP}       || "cp";
my $prog_gzip     = $ENV{PROG_GZIP}     || "gzip";
my $compress      = $ENV{OPT_COMPRESS}  || 0;
my $strip         = $ENV{OPT_STRIP}     || 1;
my $output_dir    = $ENV{OUTPUT_DIR}    || '.';
my $make_inc_file = $ENV{MAKE_INC_FILE} || "mod.make.inc";
my $flags_cc      = $ENV{FLAGS_CC}      || '';

sub usage()
{
  print STDERR "$0 modulefile entry\n";
}

sub error
{
  L4::Image::error(@_);
}

# Write a string to a file, overwriting it.
# 1:    filename
# 2..n: Strings to write to the file
sub write_to_file
{
  my $file = shift;

  open(A, ">$file") || die "Cannot open $file!";
  while ($_ = shift) {
    print A;
  }
  close A;
}

sub round_kb($)
{
  return ($_[0] + 1023) / 1024;
}

sub default_mod_merge_text(%)
{
  my %d = @_;
  my $size_str = '';

  $size_str .= sprintf " =s> %dkB", round_kb($d{size_stripped})
    if $d{size_stripped};
  $size_str .= sprintf " =c> %dkB", round_kb($d{size_compressed})
    if $d{size_compressed};

  print "$d{modname}: $d{path} ",
        "[".int(round_kb($d{size_orig}))."kB$size_str]\n";
}

sub default_output_begin
{
  my %entry = @_;
  print "Merging images:\n" if exists $entry{mods};
}

sub default_output_end
{}

my %output_formatter = (
  begin  => \&default_output_begin,
  module => \&default_mod_merge_text,
  end    => \&default_output_end,
);

# build object files from the modules
sub build_obj
{
  my ($file, $cmdline, $modname, $flags, $opts) = @_;
  my %d;
  my %imgmod;

  $d{path} = L4::ModList::search_file($file, $module_path)
    || die "Cannot find file $file! Used search path: $module_path";

  if (exists $opts->{uncompress})
    {
      system("$prog_gzip -dc $d{path} > $modname.ugz 2> /dev/null");
      if ($?)
        {
          unlink("$modname.ugz");
        }
      else
        {
          $d{path} = "$modname.ugz";
        }
    }
  $d{size_orig} = -s $d{path};

  my $take_orig = 1;
  if ($strip and not exists $opts->{nostrip})
    {
      system("$prog_objcopy -S $d{path} $modname.obj 2> /dev/null");
      $take_orig = $?;
      if ($take_orig == 0)
        {
          $d{size_stripped} = -s "$modname.obj";
          undef $d{size_stripped} if $d{size_orig} == $d{size_stripped};
        }
    }

  system("$prog_cp $d{path} $modname.obj") if $take_orig;

  my $uncompressed_size = -s "$modname.obj";

  my $c_unc = Digest::MD5->new;
  open(M, "$modname.obj") || die "Failed to open $modname.obj: $!";
  binmode M;
  $c_unc->addfile(*M);
  close M;

  if ($compress and
      not L4::ModList::is_gzipped_file("$modname.obj"))
    {
       system("$prog_gzip -9f $modname.obj && mv $modname.obj.gz $modname.obj");
       $d{size_compressed} = -s "$modname.obj";
    }

  $d{modname} = $modname;

  &{$output_formatter{module}}(%d);

  my $c_compr = Digest::MD5->new;
  open(M, "$modname.obj") || die "Failed to open $modname.obj: $!";
  $c_compr->addfile(*M);
  close M;

  my $size = -s "$modname.obj";
  my $md5_compr = $c_compr->hexdigest;
  my $md5_uncompr = $c_unc->hexdigest;

  $imgmod{filepath}          = "$modname.obj";
  $imgmod{flags}             = $flags;
  $imgmod{size}              = $size;
  $imgmod{size_uncompressed} = $uncompressed_size;
  $imgmod{name}              = basename((split(/\s+/, $cmdline))[0]);
  ($imgmod{cmdline}          = $cmdline) =~ s,^\S+\/,,;
  $imgmod{md5sum_compr}      = $md5_compr;
  $imgmod{md5sum_uncompr}    = $md5_uncompr;

  return %imgmod;
}

sub build_objects(@)
{
  my %entry = @_;
  my @mods;
  @mods = @{$entry{mods}} if exists $entry{mods};
  my $obj_fn = "$output_dir/mbi_modules.o";
  
  unlink($make_inc_file);

  # generate module-names
  for (my $i = 0; $i < @mods; $i++) {
    $mods[$i]->{modname} = sprintf "mod%02d", $i;
  }

  my %img;

  if ($ENV{BOOTSTRAP_BUILD_OUTPUT_FORMATTER})
    {
      my $f = $ENV{BOOTSTRAP_BUILD_OUTPUT_FORMATTER};
      die "No such file '$f'" unless -e $f;
      my $n = do $f;
      die "Error: $@" if $@;
      die "Could not do file '$f': $!" unless defined $n;
      my %n = %$n;
      $output_formatter{$_} = $n{$_} foreach (keys %n);
    }

  &{$output_formatter{begin}}(%entry);

  for (my $i = 0; $i < @mods; $i++) {
    my $flags = 0;
    $flags = 1 if $i == 0; # kernel
    $flags = 2 if $i == 1; # sigma0
    $flags = 3 if $i == 2; # roottask
    $flags |= 1 << 4;
    $img{mods}[$i] =
      { build_obj($mods[$i]->{file}, $mods[$i]->{cmdline},
                  $mods[$i]->{modname}, $flags,
                  $mods[$i]->{opts}) };
  }

  &{$output_formatter{end}}();

  my $make_inc_str = "MODULE_OBJECT_FILES += $obj_fn\n".
                     "LDFLAGS             += -u _binary_modules_start\n";
  $make_inc_str   .= "MOD_ADDR            := $entry{modaddr}"
    if defined $entry{modaddr};

  write_to_file($make_inc_file, $make_inc_str);

  $img{num_mods}    = scalar @mods;
  $img{mbi_cmdline} = $entry{bootstrap}{cmdline} if @mods;
  $img{arch}        = $arch;

  $img{attrs}{"l4i:PT"} = $platform_type if defined $platform_type;
  $img{attrs}{"l4i:QEMUcmd"} = "$ENV{QEMU_BINARY_NAME} -kernel \$L4IMAGE_FILE $ENV{QEMU_OPTIONS}"
    if $ENV{QEMU_BINARY_NAME};

  $img{attrs}{"l4i:info-url"} = 'https://l4re.org';
  $img{attrs}{"l4i:info-url"} = $ENV{L4IMAGE_INFO_URL}
    if $ENV{L4IMAGE_INFO_URL};
  $img{attrs}{"l4i:license"} = $ENV{L4IMAGE_LICENSE}
    if $ENV{L4IMAGE_LICENSE};

  $img{attrs}{"l4i:loadaddr"} = $ENV{BOOTSTRAP_LINKADDR};

  my $volatile_data = 1;

  if ($volatile_data)
    {
      my %a = L4::Image::creation_info("creation");
      $img{attrs}{$_} = $a{$_} foreach keys %a;
    }

  open(my $fd, ">mods.bin") || die "Cannot open 'mods.bin': $!";
  binmode $fd;
  my %offsets = L4::Image::export_modules($fd, %img);
  close $fd;

  open($fd, ">mods.offsets") || die "Cannot open 'mods.offsets': $!";
  print $fd "{\n";
  print $fd "  attrs => $offsets{attrs},\n" if defined $offsets{attrs};
  print $fd "  mod_header => $offsets{mod_header},\n" if defined $offsets{mod_header};
  print $fd "};\n";
  close $fd;

  my $section_attr = ($arch ne 'sparc' && $arch ne 'arm'
       ? #'"a", @progbits' # Not Xen
         '\"awx\", @progbits' # Xen
       : '#alloc' );

  my $alignment = 12;
  $alignment = 16 if $arch eq 'mips';

  write_to_file("mods.c",qq|
    asm (".section .module_data, $section_attr           \\n"
         ".p2align $alignment                            \\n"
         ".global _binary_modules_start                  \\n"
         "_binary_modules_start:                         \\n"
         ".incbin \\"mods.bin\\"                         \\n"
         "_binary_modules_end:                           \\n");
  |);

  system("$prog_cc $flags_cc -Wall -W -c -o $obj_fn mods.c");
  die "Compiling mods.obj failed" if $?;
  unlink("mods.c");
  unlink("mods.bin");
  unlink($_->{modname}.".obj") foreach @mods;
  unlink($_->{modname}.".ugz") foreach @mods;
}

sub get_files
{
  my %entry = @_;
  map { L4::ModList::search_file_or_die($_->{file}, $module_path) }
      @{$entry{mods}};
}

sub list_files
{
  print join(' ', get_files(@_)), "\n";
}

sub list_files_unique
{
  my %d;
  $d{$_} = 1 foreach (get_files(@_));
  print join(' ', keys %d), "\n";
}

sub fetch_files
{
  my %entry = @_;
  L4::ModList::fetch_remote_file($_->{file})
    foreach (@{$entry{mods}});
}

sub dump_entry(@)
{
  my %entry = @_;
  print "modaddr=$entry{modaddr}\n" if defined $entry{modaddr};
  print "$entry{bootstrap}{file}\n";
  print "$entry{bootstrap}{cmdline}\n";
  print join("\n", map { $_->{cmdline} } @{$entry{mods}}), "\n";
  print join(' ', map { $_->{file} } @{$entry{mods}}), "\n";
}

sub postprocess
{
  my $fn = shift;
  my $arch = shift;
  error("Invalid '$0 postprocess' use, no file given") unless defined $fn;
  error("Invalid '$0 postprocess' use, no arch given") unless defined $arch;

  my $v = 0;

  my ($type, $err) = L4::Image::get_file_type($fn);
  error("Error detecting file: $err") if $err;

  print "Type: ", (L4::Image::FILE_TYPES)[$type], "\n" if $v;
  print "Arch: $arch\n" if $v;

  error("Need a bootstrap ELF file") unless $type == L4::Image::FILE_TYPE_ELF;

  my $magic = L4::Image::BOOTSTRAP_IMAGE_INFO_MAGIC;
  my $count = `grep -c "$magic" $fn`;
  chomp $count;
  error("Multiple or no image info headers found -- must not be") if $count != 1;

  my $fn_nm = $fn;
  my ($_start, $_end, $_module_data_start, $bin_addr_end_bin);
  my $restart_nm;
  do
    {
      $restart_nm = 0;
      open(my $nm, "LC_ALL=C $prog_nm $fn_nm |") || error("Cannot get symbols from '$fn_nm'");
      while (<$nm>)
        {
          chomp;
          if (/: no symbols/ && $fn eq $fn_nm)
            {
              $fn_nm = dirname($fn).'/.debug/'.basename($fn).'.debug';
              error("No file with symbols found for '$fn'") unless -e $fn_nm;
              $restart_nm = 1;
              last;
            }
          $_start             = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+T\s+_start$/i;
          $_end               = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+[BD]\s+_end$/i;
          $_module_data_start = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+[BDTNR]\s+_module_data_start$/i;
          $bin_addr_end_bin   = Math::BigInt->from_hex($1) if /^([0-9a-f]+)\s+t\s+crt_end_bin$/i;
        }
      close $nm;
    }
  while ($restart_nm);


  $bin_addr_end_bin = Math::BigInt->new() unless defined $bin_addr_end_bin;

  error("Did not find _end symbol in binary") unless defined $_end;
  error("Did not find _start symbol in binary") unless defined $_start;
  error("Did not find _module_data_start symbol in binary")
    unless defined $_module_data_start;

  open(my $fd, "+<$fn") || error("Could not open '$fn': $!");
  binmode $fd;

  my $buf;
  my $r = sysread($fd, $buf, 1 << 20); # would we be interrupted?
  error("Could not read from file") unless $r;

  my $pos = index($buf, L4::Image::BOOTSTRAP_IMAGE_INFO_MAGIC);
  error("Did not find image info") if $pos == -1;

  if ($v)
    {
      printf "ELF: Filling image_info data at ELF-file pos 0x%x\n", $pos;
      print "ELF: _start=", $_start->as_hex(), "\n";
      print "ELF: _end=", $_end->as_hex(), "\n";
      print "ELF: _module_data_start=", $_module_data_start->as_hex(), "\n";
      print "ELF: bin_addr_end_bin=", $bin_addr_end_bin->as_hex(), "\n";
    }

  $pos += L4::Image::BOOTSTRAP_IMAGE_INFO_MAGIC_LEN; # jump over magic

  my ($_crc32, $_version, $_flags, $start_in_image, $end_in_image,
      undef, undef, $mod_header_in_image, $attrs_in_header)
    = unpack(L4::Image::TEMPLATE_IMAGE_INFO,
             substr($buf, $pos, L4::Image::IMAGE_INFO_SIZE));

  if ($v)
    {
      print  "Original values in image:\n";
      print  " crc32=$_crc32\n";
      print  " version=$_version\n";
      print  " flags=$_flags\n";
      printf " start_in_image=%x\n", $start_in_image;
      printf " end_in_image=%x\n", $end_in_image;
      printf " mod_header_in_image=%x\n", $mod_header_in_image;
      printf " attrs_in_header=%x\n", $attrs_in_header;
    }

  my $fn_offs = dirname($fn)."/mods.offsets";
  my $offsets = do $fn_offs;
  unless ($offsets)
    {
      error "Coulnd't parse $fn_offs: $@" if $@;
      error "Coulnd't do $fn_offs: $!" unless defined $offsets;
      error "Coulnd't run $fn_offs: $!" unless $offsets;
    }

  my $_mod_header = 0;
  if (defined $offsets->{mod_header})
    {
      $_mod_header = $offsets->{mod_header} + $_module_data_start - $_start;
      printf "_mod_header=%x\n", $_mod_header if $v;
    }

  my $_attrs = 0;
  if (defined $offsets->{attrs})
    {
      $_attrs = $offsets->{attrs} + $_module_data_start - $_start;
      printf "_attrs=%x\n", $_attrs if $v;
    }

  sysseek($fd, $pos, 0);
  $r = syswrite($fd, pack(L4::Image::TEMPLATE_IMAGE_INFO,
                          0, # crc32
                          $_version,
                          $_flags,
                          $_start, $_end, $_module_data_start,
                          $bin_addr_end_bin,
                          $_mod_header, $_attrs),
                L4::Image::IMAGE_INFO_SIZE);
  error("Could not patch binary")
    if not defined $r or $r != L4::Image::IMAGE_INFO_SIZE;

  # TODO: update crc32

  close $fd;
}



# ------------------------------------------------------------------------

my $cmd = $ARGV[0];

if ($cmd eq 'postprocess')
  {
    postprocess($ARGV[1], $ARGV[2]);
    exit(0);
  }

my $modulesfile   = $ARGV[1];
my $entryname     = $ARGV[2];

if ($cmd ne 'build' and !$entryname) {
  print STDERR "Error: Invalid usage!\n";
  usage();
  exit 1;
}

if (not -d $output_dir)
  {
    make_path $output_dir || die "Cannot create '$output_dir': $!";
  }
chdir $output_dir || die "Cannot change to directory '$output_dir': $!";

my %entry = ( mbi_cmdline => '' );

%entry = L4::ModList::get_module_entry($modulesfile, $entryname)
  if $modulesfile and $entryname;

if ($cmd eq 'build')
  {
    build_objects(%entry);
  }
elsif ($cmd eq 'list')
  {
    list_files(%entry);
  }
elsif ($cmd eq 'list_unique')
  {
    list_files_unique(%entry);
  }
elsif ($cmd eq 'fetch_files_and_list_unique')
  {
    fetch_files(%entry);
    list_files_unique(%entry);
  }
elsif ($cmd eq 'dump')
  {
    dump_entry(%entry);
  }
