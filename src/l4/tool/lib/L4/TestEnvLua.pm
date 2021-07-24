package L4::TestEnvLua;

use 5.008;
use strict;
use warnings;
use Scalar::Util "looks_like_number";
use L4::KeyValueConfig;

# converters turning hwconfig and fiascoconfig into the FULLCONFIG table
my %converters;
%converters = (
  NUM_CPUS => sub {
    my ($hwconfig, $fiascoconfig) = @_;
    my $val_hw = $hwconfig ? 0+($hwconfig->{NUM_CPUS} || "1") : undef;
    my $is_mp = $fiascoconfig ? ($fiascoconfig->{CONFIG_MP} || "n") : undef;

    return $val_hw unless defined $is_mp;
    my $val_fiasco = ($is_mp eq "y") ? 0+($fiascoconfig->{CONFIG_MP_MAX_CPUS} || 4) : 1;

    return $val_fiasco unless defined $val_hw;

    return ($val_fiasco > $val_hw) ? $val_hw : $val_fiasco;
  },

  MP => sub {
    my $val = $converters{NUM_CPUS}->( @_ );
    return undef unless defined $val;
    return ($val > 1) ? "y" : "n";
  },

  VIRTUALIZATION => sub {
    my ($hwconfig, $fiascoconfig) = @_;
    my $val_hw = $hwconfig ? ($hwconfig->{VIRTUALIZATION} || "n") : undef;
    my $val_fiasco = $fiascoconfig ? ($fiascoconfig->{CONFIG_CPU_VIRT} || "n") : undef;

    return $val_hw unless defined $val_fiasco;
    return $val_fiasco unless defined $val_hw;

    return ($val_hw eq "y" && $val_fiasco eq "y") ? "y" : "n";
  },

  NX => sub {
    my ($hwconfig, $fiascoconfig) = @_;
    my $val_hw = $hwconfig ? ($hwconfig->{NX} || "n") : undef;
    my $val_fiasco = $fiascoconfig ? ($fiascoconfig->{CONFIG_KERNEL_NX} || "n") : undef;

    return $val_hw unless defined $val_fiasco;
    return $val_fiasco unless defined $val_hw;

    return ($val_hw eq "y" && $val_fiasco eq "y") ? "y" : "n";
  }
);

sub readconfig {
  my $file = shift;
  my $description = shift;

  die "$description '$file' is not a readable file."
    unless -f $file && -r $file;

  open(my $fh, '<', $file)
    or die "Could not open file $file.";

  my @lines = <$fh>;

  close($fh);

  return { L4::KeyValueConfig::parse(0, @lines) };
}

sub generate_test_env {
  my $hwconfig_file     = shift;
  my $fiascoconfig_file = shift;

  my $hwconfig;
  my $fiascoconfig;

  my $testenv = {};

  # Add hwconfig if present
  $hwconfig = $testenv->{HWCONFIG} =
    readconfig($hwconfig_file, "Hwconfig") if $hwconfig_file;

  # Add fiasco config if present
  $fiascoconfig = $testenv->{FIASCOCONFIG} =
    readconfig($fiascoconfig_file, "Fiasco config") if $fiascoconfig_file;

  my $fullconfig = $testenv->{FULLCONFIG} = {};

  for my $key (sort keys %converters)
    {
      $fullconfig->{$key} = $converters{$key}->($hwconfig, $fiascoconfig);
    }

  # Carry over any additional information from hwconfig that we did not handle
  # here, so that we don't have to alter this for every hwconfig key we add.
  if ($hwconfig)
    {
      for my $key (keys %$hwconfig)
        {
          $fullconfig->{$key} = $hwconfig->{$key}
            unless exists $converters{$key};
        }
    }

  return $testenv;
}

# Turn hashes with string and numbers into lua code
sub to_lua {
  my $element = shift;
  my $indent = shift || 0;
  my $indentstr = "  " x $indent;

  my $luacode = "";

  if (ref($element) eq "HASH")
    {
      $luacode .= "{\n"; # no indent, presumably prefixed by a var =
      for my $key (sort keys %$element)
        {
          my $value = $element->{$key};
          $luacode .= $indentstr . "  [\"${key}\"] = "
            . to_lua($value, $indent + 1)
            . ",\n";
        }
      $luacode .= $indentstr . "}";
    }
  elsif (ref($element) eq "ARRAY")
    {
      $luacode .= "{\n"; # no indent, presumably prefixed by a var =
      for my $value (@$element)
        {
          $luacode .= $indentstr . "  " . to_lua($value) . ",\n";
        }
      $luacode .= $indentstr . "}";
    }
  elsif (ref($element) eq "") # not a ref, but a scalar
    {
      if (!defined($element))
        {
          $luacode .= "nil";
        }
      elsif (looks_like_number($element))
        {
          $luacode .= "$element";
        }
      else
        {
          $element =~ s/'/\'/g;
          $luacode .= "'$element'";
        }
    }
  else
    {
      die "Unable to handle this: $element";
    }
  return $luacode;
}

1;
