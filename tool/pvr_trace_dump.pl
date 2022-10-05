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

if (scalar(@ARGV) != 1) {
    die "usage: $0 <dump_file>";
}

open my $dump, '<', $ARGV[0] or die;
binmode $dump;

my $hdr;
while (read $dump, $hdr, 12) {
    my ($pkt_tp, $addr, $n_bytes) = unpack "L3", $hdr;
    $pkt_tp == 1 or die "unrecognized packet type $pkt_tp";

    printf "write %u bytes to %08x\n", $n_bytes, $addr;

    if ($n_bytes % 4 == 0) {
        # do four bytes at a time
        while ($n_bytes != 0) {
            my $bin;
            read $dump, $bin, 4 or die "incomplete packet data";
            my ($val) = unpack "L", $bin;
            printf "\t%08x\n", $val;
            $n_bytes -= 4;
        }
    } else {
        my $extra_bytes = 4 - ($n_bytes % 4); # padding length
        # do one byte at a time
        while ($n_bytes != 0) {
            my $bin;
            read $dump, $bin, 1 or die "incomplete packet data";
            my ($val) = unpack "L", $bin;
            printf "\t%02x\n", $val;
            $n_bytes--;
        }
        # read padding
        read $dump, $_, $extra_bytes or die "missing padding";
    }
}
