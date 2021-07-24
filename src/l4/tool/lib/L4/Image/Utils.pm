package L4::Image::Utils;

use warnings;
use strict;
use Exporter;

use vars qw(@ISA @EXPORT);
@ISA    = qw(Exporter);
@EXPORT = qw(error check_syswrite check_sysread checked_sysseek
             filepos_get filepos_set sysreadz);


sub error
{
  print STDERR "Error: ", shift, "\n";
  print STDERR "Call trace:\n";
  my $i = 0;
  my @d;
  print STDERR $d[1].":".$d[2]." (".$d[3].")\n" while @d = caller($i++);

  exit 1;
}

sub check_syswrite
{
  my $r = shift;
  my $wr_size = shift;
  error("Write error: $!") unless defined $r;
  error("Did not write all data ($r < $wr_size)") if $wr_size != $r;
  $r;
}

sub check_sysread
{
  my $r = shift;
  my $rd_size = shift;
  error("Read error: $!") unless defined $r;
  error("Did not read all data ($r < $rd_size)") if $rd_size != $r;
  $r;
}

sub sysreadz
{
  my $fd = shift;
  my $result = "";
  while (my $err = sysread($fd, my $s, 10) != 0)
    {
      error("Error reading zero terminated string") unless defined $err;
      $result .= [split(/\0/, $s, 2)]->[0];
      last if $s =~ /\0/;
    }
  $result;
}

sub checked_sysseek
{
  my ($fd, $p, $what) = @_;

  my $r = sysseek($fd, $p, $what);
  die "sysseek failed" unless defined $r;

  return $r + 0;
}

sub filepos_get
{
  return checked_sysseek(shift, 0, 1);
}

sub filepos_set
{
  return checked_sysseek(shift, shift, 0);
}

1;
