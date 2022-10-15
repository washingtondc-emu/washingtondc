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

our $outfile;

sub save_pkt {
    (my %pkt) = @_;

    if (($pkt{'tp'} & 0xffff) == 1) {
        # address-write packet
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
    } elsif (($pkt{'tp'} & 0xffff) == 2) {
        my $bin = pack "L", $pkt{'tp'};
        print $outfile $bin;
    } else {
        my $tp = $pkt{'tp'} & 0xffff;
        die "unrecognized packet type $tp";
    }
}

if (scalar(@ARGV) != 2) {
    die "usage: $0 <input> <output>";
}

say "reading from $ARGV[0] and writing optimized packet stream to $ARGV[1]";
open my $infile, '<', $ARGV[0] or die;
open $outfile, '>', $ARGV[1] or die;
binmode $infile;
binmode $outfile;

my $tp_bin;
my %pkt;
while (read $infile, $tp_bin, 4) {
    (my $tp) = unpack "L", $tp_bin;
    if (($tp & 0xffff) == 1) {
        my $hdr;
        read $infile, $hdr, 8 or die;
        (my $addr, my $len) = unpack "L2", $hdr;
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
                save_pkt(%pkt);
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
    } elsif (($tp & 0xffff) == 2) {
        # irq packet
        if (keys %pkt) {
            # we don't combine irq packets because that wouldn't make sense
            save_pkt(%pkt);
            %pkt = ( );
        }
        $pkt{'tp'} = $tp;
    } else {
        $tp &= 0xffff;
        die "unrecognized packet type $tp";
    }
}

close $outfile;
close $infile;
