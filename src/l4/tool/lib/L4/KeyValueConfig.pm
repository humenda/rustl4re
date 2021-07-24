package L4::KeyValueConfig;

use strict;
use warnings;
use Scalar::Util "looks_like_number";

sub parse {
  my $with_quotes = shift;
  # key/value syntax
  my $kv_regex = qr/^\s*(\w+)\s*=\s*(.*?)(\s*)$/;

  map { my @ret;
        $_ =~ $kv_regex;
        my ($key, $val) = ($1, $2);
        # keep existing outer quotes
        if ($val =~ /^'([^']*)'$/ || $val =~ /^"([^"]*)"$/) {
          @ret = ($key => $with_quotes ? $val : $1);
        }
        # numbers
        elsif (looks_like_number($val)) {
          @ret = ($key => $val);
        }
        # quote unquoted non-number strings;
        # ignore strange inner quoted strings
        elsif ($val !~ /['"]/) {
          @ret = ($key => $with_quotes ? "'$val'" : "$val");
        }
        # ignore everything else
        else {
          @ret = ();
        }
        @ret;
      }
    grep { /^\s*\w+\s*=/ }      # looks like key=value at all
    grep { $_ !~ /^\s*#/ }      # ignore comment lines
    @_;
}

1;
