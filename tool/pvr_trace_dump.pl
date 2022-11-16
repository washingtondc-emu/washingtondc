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

our $hdr_tp = -1;
our $vtx_tp = -1;

# returns true if it needs 32 more bytes, else false
sub try_decode_tafifo_packet {
    (my $pkt_ref) = @_;
    my @pkt = @{$pkt_ref};
    my $cmd_tp = ($pkt[0] >> 29) & 7;
    if ($cmd_tp == 0) {
        # end of list
        say "\tPVR2 TA END OF LIST PACKET";
    } elsif ($cmd_tp == 1) {
        # user clip
        say "\tPVR2 TA USER CLIP PACKET";
    } elsif ($cmd_tp == 2) {
        # input list
        say "\tPVR2 TA INPUT LIST PACKET";
    } elsif ($cmd_tp == 4) {
        # polygon header
        say "\tPVR2 TA POLYGON HEADER PACKET";

        my $key = $pkt[0];
        my @poly_types = (
            #    mask       value     header type      vertex type
            [     0x78 ,      0x00 ,           0 ,           0 ],
            [     0x78 ,      0x10 ,           0 ,           1 ],
            [     0x78 ,      0x20 ,           1 ,           2 ],
            [     0x78 ,      0x30 ,           0 ,           2 ],
            [     0x78 ,      0x40 ,           3 ,           9 ],
            [     0x78 ,      0x60 ,           4 ,          10 ],
            [     0x78 ,      0x70 ,           3 ,          10 ],
            [     0x79 ,      0x08 ,           0 ,           3 ],
            [     0x79 ,      0x09 ,           0 ,           4 ],
            [     0x79 ,      0x18 ,           0 ,           5 ],
            [     0x79 ,      0x19 ,           0 ,           6 ],
            [     0x7d ,      0x28 ,           1 ,           7 ],
            [     0x7d ,      0x2c ,           2 ,           7 ],
            [     0x7d ,      0x29 ,           1 ,           8 ],
            [     0x7d ,      0x2d ,           2 ,           8 ],
            [     0x79 ,      0x38 ,           0 ,           7 ],
            [     0x79 ,      0x39 ,           0 ,           8 ],
            [     0x79 ,      0x48 ,           3 ,          11 ],
            [     0x79 ,      0x49 ,           3 ,          12 ],
            [     0x79 ,      0x68 ,           4 ,          13 ],
            [     0x78 ,      0x69 ,           4 ,          14 ],
            [     0x79 ,      0x78 ,           3 ,          13 ],
            [     0x79 ,      0x79 ,           3 ,          14 ]);
        $hdr_tp = -1;
        $vtx_tp = -1;
        for (@poly_types) {
            my @row = @{$_};
            if (($pkt[0] & $row[0]) == $row[1]) {
                $hdr_tp = $row[2];
                $vtx_tp = $row[3];
                last;
            }
        }

        if (($hdr_tp == 2 || $hdr_tp == 4) && ((scalar @pkt) != 16)) {
            return 1;
        }

        if ($hdr_tp == -1) {
            say "\tUNKNOWN HEADER TYPE";
        } else {
            say "HEADER TYPE $hdr_tp";
        }

        if ($vtx_tp == -1) {
            say "\tUNKNOWN VERTEX TYPE";
        } else {
            say "VERTEX TYPE $vtx_tp";
        }

        my $poly_tp = ($pkt[0] >> 24) & 7;
        my @poly_tp_names = ( "OPAQUE", "OPAQUE MODIFIER", "TRANSPARENT",
                              "TRANSPARENT MODIFIER", "PUNCH-THROUGH",
                              "INVALID 5", "INVALID 6", "INVALID 7" );
        say "\t\tpolygon type: $poly_tp_names[$poly_tp]";

        if (($pkt[0] >> 6) & 1) {
            say "\t\ttwo-volumes mode: enabled";
        } else {
            say "\t\ttwo-volumes mode: disabled";
        }

        my $color_type;
        if (($pkt[0] >> 4) & 3 == 0) {
            $color_type = "packed";
        } elsif (($pkt[0] >> 4) & 3 == 1) {
            $color_type = "floating-point";
        } elsif (($pkt[0] >> 4) & 3 == 2) {
            $color_type = "intensity mode 1";
        } else {
            $color_type = "intensity mode 2";
        }
        say "\t\tcolor type: $color_type";

        if (($pkt[0] >> 3) & 1) {
            say "\t\ttextures: enabled";
            my $wshift = 3 + (($pkt[2] >> 3) & 7);
            my $hshift = 3 + ($pkt[2] & 7);
            printf "\t\ttexture resolution: (%u, %u)\n", 1 << $wshift, 1 << $hshift;
            printf "\t\ttexture pointer: %08x\n", ($pkt[3] & 0x1fffff) << 3;
        } else {
            say "\t\ttextures: disabled";
        }
    } elsif ($cmd_tp == 5) {
        # sprite header
        say "\tPVR2 TA SPRITE HEADER PACKET";
        if ($pkt[0] & (1 << 3)) {
            $hdr_tp = 6;
            $vtx_tp = 15;
        } else {
            $hdr_tp = 5;
            $vtx_tp = 16;
        }
    } elsif ($cmd_tp == 6) {
        # IDK
        say "\tPVR2 TA UNKNOWN PACKET TYPE 6";
    } elsif ($cmd_tp == 7) {
        # vertex header
        if (($vtx_tp == 5 || $vtx_tp == 6 || $vtx_tp == 11 ||
             $vtx_tp == 12 || $vtx_tp == 13 || $vtx_tp == 14 ||
             $vtx_tp == 15 || $vtx_tp == 16) &&
            ((scalar @pkt) != 16)) {
            return 1;
        }

        if ($vtx_tp == 15 || $vtx_tp == 16) {
            # sprite

            say "\tPVR2 TA VERTEX PACKET (SPRITE)";

            my $packedverts = pack "LLLLLLLLLLL", @pkt[1..11];
            my @verts = unpack "fffffffffff", $packedverts;
            say "\tpos[0]: ($verts[0], $verts[1], $verts[2])";
            say "\tpos[1]: ($verts[3], $verts[4], $verts[5])";
            say "\tpos[2]: ($verts[6], $verts[7], $verts[8])";
            say "\tpos[3]: ($verts[9], $verts[10])";

            if ($vtx_tp == 15) {
                printf "\ttexture coordinates[0]: %08x\n", $pkt[13];
                printf "\ttexture coordinates[1]: %08x\n", $pkt[14];
                printf "\ttexture coordinates[2]: %08x\n", $pkt[15];
            }
        } else {
            say "\tPVR2 TA VERTEX PACKET";
            my $packedpos = pack "LLL", ($pkt[1], $pkt[2], $pkt[3]);
            my @pos = unpack "fff", $packedpos;
            say "\tpos: ($pos[0], $pos[1], $pos[2])";
            if ($vtx_tp == 3 || $vtx_tp == 4 || $vtx_tp == 7) {
                my $packeduv = pack "LL", @pkt[4..5];
                my @uv = unpack "ff", $packeduv;
                say "\ttexture coordinates: ($uv[0], $uv[1])";
            } elsif ($vtx_tp == 4 || $vtx_tp == 6 || $vtx_tp == 8) {
                printf "\ttexture coordinates: %08x\n", $pkt[4];
            } elsif ($vtx_tp == 11 || $vtx_tp == 13) {
                my $packeduv0 = pack "LL", @pkt[4..5];
                my $packeduv1 = pack "LL", @pkt[8..9];
                my @uv0 = unpack "ff", $packeduv0;
                my @uv1 = unpack "ff", $packeduv1;
                say "\ttexture coordinates (vol 0): ($uv0[0], $uv0[1])";
                say "\ttexture coordinates (vol 1): ($uv1[0], $uv1[1])";
            } elsif ($vtx_tp == 12 || $vtx_tp == 14) {
                printf "\ttexture coordinates (vol 0): %08x\n", $pkt[4];
                printf "\ttexture coordinates (vol 1): %08x\n", $pkt[8];
            }
        }
    } else {
        say "\tUnknown TA FIFO packet type $cmd_tp";
    }

    for (@pkt) {
        printf "\t\t%08x\n", $_;
    }

    return 0;
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

our $frameno = 0;

my $hdr;
while (read $dump, $hdr, 4) {
    my ($pkt_tp_and_unit_sz) = unpack "L", $hdr;
    my $pkt_tp = $pkt_tp_and_unit_sz & 0xffff;

    if ($pkt_tp == 1) {
        read $dump, $hdr, 8 or die;
        my ($addr, $n_units) = unpack "L2", $hdr;
        my $unit_sz = $pkt_tp_and_unit_sz >> 16;
        my $n_bytes = $n_units * $unit_sz;

        my $name = identify_addr($addr);
        if ($name) {
            printf "write %u bytes to %08x <%s> in units of %u\n", $n_bytes, $addr, $name, $unit_sz;
        } else {
            printf "write %u bytes to %08x in units of %u\n", $n_bytes, $addr, $unit_sz;
        }

        if ($name eq "TA FIFO") {
            my @pkt;
            while ($n_bytes != 0) {
                my $bin;
                read $dump, $bin, 4 or die "incomplete packet data";
                my ($val) = unpack "L", $bin;
                push @pkt, $val;
                if ((scalar @pkt) % 8 == 0) {
                    if (!try_decode_tafifo_packet(\@pkt)) {
                        @pkt = ();
                    }
                }
                $n_bytes -= 4;
            }
        } elsif ($n_bytes % 4 == 0) {
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
    } elsif ($pkt_tp == 2) {
        my $irq_tp = $pkt_tp_and_unit_sz >> 16;
        my %irq_names = (
            0 => 'VBLANK IN',
            1 => 'VBLANK OUT',
            2 => 'HBLANK',
            3 => 'TA OPAQUE LIST COMPLETE',
            4 => 'TA OPAQUE MODIFIER LIST COMPLETE',
            5 => 'TA TRANSPARENT LIST COMPLETE',
            6 => 'TA TRANSPARENT MODIFIER LIST COMPLETE',
            7 => 'TA PUNCH-THROUGH LIST COMPLETE',
            8 => 'TA RENDER COMPLETE');
        if (exists($irq_names{$irq_tp})) {
            say "IRQ $irq_names{$irq_tp}";
        } else {
            say "IRQ (unrecognized type $irq_tp)";
        }
        if ($irq_tp == 1) {
            say "\tBEGIN FRAME $frameno";
            $frameno++;
        }
    } else {
        die "unrecognized packet type $pkt_tp";
    }
}
