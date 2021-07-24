# The RAW image format assumes that _start is at the start of the image file
# and everything is in a continuous address space from there on. The module
# data is expected to be at the end of the file.

package L4::Image::Raw;

use warnings;
use strict;
use Exporter;
use L4::Image::Utils qw/error check_sysread check_syswrite filepos_set/;

use vars qw(@ISA @EXPORT);
@ISA    = qw(Exporter);
@EXPORT = qw();

sub new
{
  my $class = shift;
  my $fn = shift;
  my $start_of_binary = shift;

  open(my $fd, "<", $fn) || error("Could not open '$fn': $!");
  binmode($fd);

  return bless {
    start_of_binary => $start_of_binary,
    fd => $fd,
  }, $class;
}

sub dispose
{
  my $self = shift;
  close($self->{'fd'});
}

sub vaddr_to_file_offset
{
  my $self = shift;
  my $vaddr = shift;

  return $vaddr - $self->{'start_of_binary'};
}

sub objcpy_start
{
  my $self = shift;
  my $until_vaddr = shift;
  my $ofn = shift;

  my $offset = $self->vaddr_to_file_offset($until_vaddr);
  open(my $ofd, "+>$ofn") || error("Could not open '$ofn': $!");
  binmode $ofd;

  my $ifd = $self->{'fd'};
  $self->{'ofd'} = $ofd;

  # copy initial part
  my $buf;
  filepos_set($ifd, 0);
  check_sysread(sysread($ifd, $buf, $offset), $offset);
  check_syswrite(syswrite($ofd, $buf), length($buf));

  return $ofd;
}

sub objcpy_finalize
{
  my $self = shift;
  close($self->{'ofd'});
}

1;
