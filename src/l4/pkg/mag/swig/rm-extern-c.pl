#! /usr/bin/perl -W

use strict;

my $if_lvl = 0;
my $cpp = 0;
my $ext_c = 0;

while (<>)
{
  chomp;

  if (/^#ifdef\s+__cplusplus\s*$/)
    {
      $if_lvl++;
      $cpp = $if_lvl;
    }
  elsif (/^#if.*$/)
    {
      $if_lvl++;
    }
  elsif (/^#endif\s*$/)
    {
      $cpp = 0 if $cpp == $if_lvl;
      $if_lvl--;
    }

  if ($ext_c && $cpp && $cpp == $if_lvl && /^}/)
    {
      $ext_c = 0;
      print ("// removed: }\n");
    }
  elsif ($cpp && $cpp == $if_lvl && /^\s*extern\s+"C"\s+{/)
    {
      print ("// removed: extern \"C\" {\n");
      $ext_c = 1;
    }
  else
    {
      print("$_\n");
    }
}
