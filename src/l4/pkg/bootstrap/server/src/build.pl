#! /usr/bin/perl -W
#
# (c) 2008-2009 Technische Universität Dresden
# This file is part of TUD:OS and distributed under the terms of the
# GNU General Public License 2.
# Please see the COPYING-GPL-2 file for details.
#
# Adam Lackorzynski <adam@os.inf.tu-dresden.de>
#

use strict;

BEGIN { unshift @INC, $ENV{L4DIR}.'/tool/lib'
           if $ENV{L4DIR} && -d $ENV{L4DIR}.'/tool/lib/L4';}

use Digest::MD5;
use L4::ModList;


my $cross_compile_prefix = $ENV{CROSS_COMPILE} || '';
my $arch                 = $ENV{OPT_ARCH}     || "x86";

my $module_path   = $ENV{SEARCHPATH}    || ".";
my $prog_objcopy  = $ENV{OBJCOPY}       || "${cross_compile_prefix}objcopy";
my $prog_cc       = $ENV{CC}            || "${cross_compile_prefix}gcc";
my $prog_ld       = $ENV{LD}            || "${cross_compile_prefix}ld";
my $prog_cp       = $ENV{PROG_CP}       || "cp";
my $prog_gzip     = $ENV{PROG_GZIP}     || "gzip";
my $compress      = $ENV{OPT_COMPRESS}  || 0;
my $strip         = $ENV{OPT_STRIP}     || 1;
my $output_dir    = $ENV{OUTPUT_DIR}    || '.';
my $make_inc_file = $ENV{MAKE_INC_FILE} || "mod.make.inc";
my $flags_cc      = $ENV{FLAGS_CC}      || '';

my $modulesfile      = $ARGV[1];
my $entryname        = $ARGV[2];

sub usage()
{
  print STDERR "$0 modulefile entry\n";
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

# build object files from the modules
sub build_obj
{
  my ($_file, $cmdline, $modname, $no_strip) = @_;

  my $file = L4::ModList::search_file($_file, $module_path)
    || die "Cannot find file $_file! Used search path: $module_path";

  printf STDERR "Merging image %s to %s [%dkB]\n",
                $file, $modname, ((-s $file) + 1023) / 1024;
  # make sure that the file isn't already compressed
  system("$prog_gzip -dc $file > $modname.ugz 2> /dev/null");
  $file = "$modname.ugz" if !$?;
  system("$prog_objcopy -S $file $modname.obj 2> /dev/null")
    if $strip && !$no_strip;
  system("$prog_cp         $file $modname.obj")
    if $? || !$strip || $no_strip;
  my $uncompressed_size = -s "$modname.obj";

  my $c_unc = Digest::MD5->new;
  open(M, "$modname.obj") || die "Failed to open $modname.obj: $!";
  $c_unc->addfile(*M);
  close M;

  system("$prog_gzip -9f $modname.obj && mv $modname.obj.gz $modname.obj")
    if $compress;

  my $c_compr = Digest::MD5->new;
  open(M, "$modname.obj") || die "Failed to open $modname.obj: $!";
  $c_compr->addfile(*M);
  close M;

  my $size = -s "$modname.obj";
  my $md5_compr = $c_compr->hexdigest;
  my $md5_uncompr = $c_unc->hexdigest;

  my $section_attr = ($arch ne 'sparc' && $arch ne 'arm'
       ? #'"a", @progbits' # Not Xen
         '\"awx\", @progbits' # Xen
       : '#alloc' );

  write_to_file("$modname.extra.c",qq|
    #include "mod_info.h"

    extern char const _binary_${modname}_start[];

    struct Mod_info const _binary_${modname}_info
    __attribute__((section(".module_info"), aligned(4))) =
    {
      _binary_${modname}_start, $size, $uncompressed_size,
      "$_file", "$cmdline",
      "$md5_compr", "$md5_uncompr"
    };

    asm (".section .module_data, $section_attr           \\n"
         ".p2align 12                                    \\n"
         ".global _binary_${modname}_start               \\n"
         "_binary_${modname}_start:                      \\n"
         ".incbin \\"$modname.obj\\"                     \\n"
         "_binary_${modname}_end:                        \\n");
  |);

  system("$prog_cc $flags_cc -c -o $modname.bin $modname.extra.c");
  die "Assembling $modname failed" if $?;
  unlink("$modname.extra.c", "$modname.obj", "$modname.ugz");
}

sub build_mbi_modules_obj($@)
{
  my $cmdline = shift;
  my @mods = @_;
  my $asm_string;

  # generate mbi module structures
  write_to_file("mbi_modules.c", qq|char const _mbi_cmdline[] = "$cmdline";|);
  system("$prog_cc $flags_cc -c -o mbi_modules.bin mbi_modules.c");
  die "Compiling mbi_modules.bin failed" if $?;
  unlink("mbi_modules.c");

}

sub build_objects(@)
{
  my %entry = @_;
  my @mods = @{$entry{mods}};
  my $objs = "$output_dir/mbi_modules.bin";
  
  unlink($make_inc_file);

  # generate module-names
  for (my $i = 0; $i < @mods; $i++) {
    $mods[$i]->{modname} = sprintf "mod%02d", $i;
  }

  build_mbi_modules_obj($entry{bootstrap}{cmdline}, @mods);

  for (my $i = 0; $i < @mods; $i++) {
    build_obj($mods[$i]->{command}, $mods[$i]->{cmdline}, $mods[$i]->{modname},
	      $mods[$i]->{type} =~ /.+-nostrip$/);
    $objs .= " $output_dir/$mods[$i]->{modname}.bin";
  }

  my $make_inc_str = "MODULE_OBJECT_FILES += $objs\n";
  $make_inc_str   .= "MOD_ADDR            := $entry{modaddr}"
    if defined $entry{modaddr};

  write_to_file($make_inc_file, $make_inc_str);
}

sub get_files
{
  my %entry = @_;
  map { L4::ModList::search_file_or_die($_->{command}, $module_path) }
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
  L4::ModList::fetch_remote_file($_->{command})
    foreach (@{$entry{mods}});
}

sub dump_entry(@)
{
  my %entry = @_;
  print "modaddr=$entry{modaddr}\n" if defined $entry{modaddr};
  print "$entry{bootstrap}{command}\n";
  print "$entry{bootstrap}{cmdline}\n";
  print join("\n", map { $_->{cmdline} } @{$entry{mods}}), "\n";
  print join(' ', map { $_ } @{$entry{files}}), "\n";
}

# ------------------------------------------------------------------------

if (!$ARGV[2]) {
  print STDERR "Error: Invalid usage!\n";
  usage();
  exit 1;
}

if (defined $output_dir)
  {
    if (not -d $output_dir)
      {
        mkdir $output_dir || die "Cannot create '$output_dir': $!";
      }
    chdir $output_dir || die "Cannot change to directory '$output_dir': $!";
  }

my %entry = L4::ModList::get_module_entry($modulesfile, $entryname);

if ($ARGV[0] eq 'build')
  {
    build_objects(%entry);
  }
elsif ($ARGV[0] eq 'list')
  {
    list_files(%entry);
  }
elsif ($ARGV[0] eq 'list_unique')
  {
    list_files_unique(%entry);
  }
elsif ($ARGV[0] eq 'fetch_files_and_list_unique')
  {
    fetch_files(%entry);
    list_files_unique(%entry);
  }
elsif ($ARGV[0] eq 'dump')
  {
    dump_entry(%entry);
  }
