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
use PerlIO::gzip;

my %regmap = (
    # device
    0x005f8000 => 'ID',
    0x005f8004 => 'REVISION',
    0x005f80a0 => 'SDRAM_REFRESH',
    0x005f80a4 => 'SDRAM_ARB_CFG',
    0x005f80a8 => 'SDRAM_CFG',
    0x005f8018 => 'TEST_SELECT',

    # rendering core
    0x005f8008 => 'SOFTRESET',
    0x005f8014 => 'STARTRENDER',
    0x005f8020 => 'PARAM_BASE',
    0x005f802c => 'REGION_BASE',
    0x005f8030 => 'SPAN_SORT_CFG',
    0x005f8074 => 'FPU_SHAD_SCALE',
    0x005f8078 => 'FPU_CULL_VAL',
    0x005f807c => 'FPU_PARAM_CFG',
    0x005f8080 => 'HALF_OFFSET',
    0x005f8084 => 'FPU_PERP_VAL',
    0x005f8088 => 'ISP_BACKGND_D',
    0x005f808c => 'ISP_BACKGND_T',
    0x005f8098 => 'ISP_FEED_CFG',
    0x005f80e4 => 'TEXT_CONTROL',
    0x005f8108 => 'PAL_RAM_CTRL',
    0x005f811c => 'PT_ALPHA_REF',

    # video
    0x005f8044 => 'FB_R_CTRL',
    0x005f8048 => 'FB_W_CTRL',
    0x005f804c => 'FB_W_LINESTRIDE',
    0x005f8050 => 'FB_R_SOF1',
    0x005f8054 => 'FB_R_SOF2',
    0x005f805c => 'FB_R_SIZE',
    0x005f8060 => 'FB_W_SOF1',
    0x005f8064 => 'FB_W_SOF2',
    0x005f8068 => 'FB_X_CLIP',
    0x005f806c => 'FB_Y_CLIP',
    0x005f80c4 => 'SPG_TRIGGER_POS',
    0x005f80c8 => 'SPG_HBLANK_INT',
    0x005f80cc => 'SPG_VBLANK_INT',
    0x005f80d0 => 'SPG_CONTROL',
    0x005f80d4 => 'SPG_HBLANK',
    0x005f80d8 => 'SPG_LOAD',
    0x005f80dc => 'SPG_VBLANK',
    0x005f80e0 => 'SPG_WIDTH',
    0x005f80e8 => 'VO_CONTROL',
    0x005f80ec => 'VO_STARTX',
    0x005f80f0 => 'VO_STARTY',
    0x005f80f4 => 'SCALER_CTL',
    0x005f810c => 'SPG_STATUS',
    0x005f8110 => 'FB_BURSTCTRL',
    0x005f8114 => 'FB_C_SOF',
    0x005f8118 => 'Y_COEFF',
    0x005f8040 => 'VO_BORDER_COL',

    # Tile Accelerator
    0x005f8008 => 'SOFTRESET',
    0x005f8124 => 'TA_OL_BASE',
    0x005f8128 => 'TA_ISP_BASE',
    0x005f812c => 'TA_OL_LIMIT',
    0x005f8130 => 'TA_ISP_LIMIT',
    0x005f8134 => 'TA_NEXT_OPB',
    0x005f8138 => 'TA_ITP_CURRENT',
    0x005f813c => 'TA_GLOB_TILE_CLIP',
    0x005f8140 => 'TA_ALLOC_CTRL',
    0x005f8144 => 'TA_LIST_INIT',
    0x005f8148 => 'TA_YUV_TEX_BASE',
    0x005f814c => 'TA_YUV_TEX_CTRL',
    0x005f8150 => 'YA_YUV_TEX_CNT',
    0x005f8160 => 'TA_LIST_CONT',
    0x005f8164 => 'TA_NEXT_OPB_INIT',

    # fog
    0x005f80b0 => 'FOG_COL_RAM',
    0x005f80b4 => 'FOG_COL_VERT',
    0x005f80b8 => 'FOG_DENSITY',
    0x005f80bc => 'FOG_CLAMP_MAX',
    0x005f80c0 => 'FOG_CLAMP_MIN'
    );
sub identify_addr {
=begin
        accepts as its sole parameter an address
        returns a string identifying what that address represents; the string
        will be empty if the address could not be identified
=cut
    my $addr = $_[0] & 0x1fffffff;
    if (($addr >= 0x005f7c00 && $addr < 0x005fa000) ||
        ($addr >= 0x025f7c00 && $addr < 0x025fa000)) {
        my $index = $addr & 0x01ffffff;

        if ($index >= 0x005f9000 && $index < 0x005fa000) {
            return 'PALETTE_RAM';
        }

        if ($index >= 0x005f8600 && $index < 0x005f8f60) {
            return 'TA_OL_POINTERS';
        }

        if ($index >= 0x005f8200 && $index < 0x005f8400) {
            return 'FOG_TABLE';
        }

        if (exists($regmap{$index})) {
            return $regmap{$index};
        } else {
            return "";
        }
    } elsif (($addr >= 0x04000000 && $addr < 0x04800000) ||
        ($addr >= 0x06000000 && $addr < 0x06800000)) {
        return "texture memory (64-bit path)";
    } elsif (($addr >= 0x05000000 && $addr < 0x05800000) ||
             ($addr >= 0x07000000 && $addr < 0x07800000)) {
        return "texture memory (32-bit path)";
    } elsif (($addr >= 0x10000000 && $addr < 0x10800000) ||
             ($addr >= 0x11000000 && $addr < 0x11800000)) {
        return "TA FIFO";
    } elsif ($addr >= 0x10800000 && $addr < 0x11000000) {
        return "TA YUV conversion FIFO";
    } else {
        return "";
    }
}

if (scalar(@ARGV) != 1) {
    die "usage: $0 <dump_file>";
}

my $dump;
if ($ARGV[0] =~ /.*\.gz$/) {
    open $dump, "<:gzip", $ARGV[0] or die "cannot open $ARGV[0] as gzip file";
} else {
    open $dump, '<', $ARGV[0] or die "cannot open $ARGV[0]";
    binmode $dump;
}


my $hdr;
while (read $dump, $hdr, 12) {
    my ($pkt_tp, $addr, $n_bytes) = unpack "L3", $hdr;
    $pkt_tp == 1 or die "unrecognized packet type $pkt_tp";

    my $name = identify_addr($addr);
    if ($name) {
        printf "write %u bytes to %08x <%s>\n", $n_bytes, $addr, $name;
    } else {
        printf "write %u bytes to %08x\n", $n_bytes, $addr;
    }

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
