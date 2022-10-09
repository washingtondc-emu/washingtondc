#!/usr/bin/env perl

################################################################################
#
#    Copyright (C) 2022 snickerbockers
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

use v5.20;

if (scalar(@ARGV) != 2) {
    die "usage: $0 <input> <output>";
}

say "reading from $ARGV[0] and writing optimized packet stream to $ARGV[1]";
open my $infile, '<', $ARGV[0] or die;
open my $outfile, '>', $ARGV[1] or die;
binmode $infile;
binmode $outfile;

my $hdr;
my %pkt;
while (read $infile, $hdr, 12) {
    (my $tp, my $addr, my $len) = unpack "L3", $hdr;
    ($tp & 0xffff) == 1 or die 'unrecognized packet type';
    my $unit_sz = $tp >> 16;
    $len *= $unit_sz;

    my $extra_bytes;
    if ($len % 4) {
        $extra_bytes = 4 - ($len % 4); # padding length
    } else {
        $extra_bytes = 0;
    }

    my @data;
    my $n_bytes = $len;
    # do one byte at a time
    while ($n_bytes != 0) {
        my $bin;
        read $infile, $bin, 1 or die "incomplete packet data";
        my ($val) = unpack "C", $bin;
        push @data, $val;
        $n_bytes--;
    }

    if (keys %pkt && $addr == $pkt{'len'} + $pkt{'addr'} && $tp == $pkt{'tp'}) {
        $pkt{'len'} += $len;
        push @{$pkt{'data'}}, @data;
    } else {
        # save previous packet
        if (keys %pkt) {
            my $hdr = pack "L3", ($pkt{'tp'}, $pkt{'addr'}, $pkt{'len'} / ($pkt{'tp'} >> 16));
            print $outfile $hdr;
            for (@{$pkt{'data'}}) {
                my $dat = pack "C", $_;
                print $outfile $dat;
            }
            # padding
            if (scalar @{$pkt{'data'}} % 4) {
                my $extra_bytes = 4 - (scalar @{$pkt{'data'}} % 4);
                while ($extra_bytes) {
                    my $dat = pack "C", 0;
                    print $outfile $dat;
                }
            }
        }

        # save current packet
        $pkt{'data'} = \@data;
        $pkt{'tp'} = $tp;
        $pkt{'addr'} = $addr;
        $pkt{'len'} = $len;
    }
    # read padding
    if ($extra_bytes) {
        read $infile, $_, $extra_bytes or die "missing $extra_bytes padding";
    }
}

close $outfile;
close $infile;
