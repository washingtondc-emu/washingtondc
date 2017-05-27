#!/bin/bash

################################################################################
#
#
#   WashingtonDC Dreamcast Emulator
#   Copyright (C) 2017 snickerbockers
#   chimerasaurusrex@gmail.com
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the
#   Free Software Foundation, Inc.,
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
#
################################################################################

################################################################################
#
# This script automatically generates a gdi-format image from a directory of
# files.  It handles all the mechanical work of creating the first two tracks,
# generating the iso9660fs (with correct multisession offsets), padding that
# filesystem out to 2352-byte blocks and writing the .gdi file.
#
# The images this script generates probably won't work on a real dreamcast
# because all the extra data needed to turn the iso9660fs into a disc image
# (the lba meta-data, and the first two tracks) is filled with zero-padding
# instead of real data.  As much as I'd love to make "real" gdi images with
# this script, I don't have a modded dreamcast to run them on so there'd be no
# way to test.  This script will generate all data needed to mount it in
# WasingonDC's virtual GD-ROM drive.
#
# The output generated is always a 3-track disc.
#
################################################################################

# the lengths of the first two tracks are arbitrary, I chose values that seem to
# be common on real GD-ROM discs
DEFAULT_TRACK01_LEN=604
DEFAULT_TRACK02_LEN=526

if command -v mkisofs ; then
   MKISOFS_CMD="mkisofs"
elif command -v genisoimage ; then
    MKISOFS_CMD="genisoimage"
else
    echo "ERROR: you must install either mkisofs or genisoimage"
    exit 1
fi

function print_usage() {
    echo "Usage: $0 -o <out_dir> [options] directory"

    echo ""
    echo "this script will create a gdi image, with the contents of the given"
    echo "directory forming an ISO-9660 filesystem on the thrid track"
    echo ""

    echo "OPTIONS"
    echo "-o <out_dir> directory where the .gdi file and tracks will go"
    echo "-f delete out_dir if it already exists before building the new gdi"
    echo "-h print this message and exit without doing anything"
    echo "-r *DON'T* use rock-ridge (by default this script does use rock-ridge)"

    exit 1
}

args=$(getopt rfho: $*)

echo $args
set -- $args

out_dir=""

force=false

rock_ridge=true

track01_len="$DEFAULT_TRACK01_LEN"
track02_len="$DEFAULT_TRACK02_LEN"

while :; do
    case "$1" in
        -o)
            out_dir=$2
            shift
            shift
            ;;
        -h)
            print_usage
            shift
            ;;
        -f)
            force=true
            shift
            ;;
        -r)
            rock_ridge=false
            shift
            ;;
        --)
            shift
            break
            ;;
    esac
done

if test $# -ne 1 ; then
    print_usage
fi

in_dir="$1"

track01_lba="150"
track02_lba="$(expr $track01_lba + $track01_len)"
track03_lba="$(expr $track02_lba + $track02_len)"

track01_path="$out_dir/track01.bin"
track02_path="$out_dir/track02.raw"
track03_path="$out_dir/track03.bin"

if test "x$out_dir" = "x" ; then
    echo "error - you must supply an output directory (-o argument)"
    print_usage
fi

if test -e "$out_dir" ; then
    echo "$out_dir already exists!"
    if test "$force" = true ; then
        rm -rf $out_dir
        echo "$out_dir removed"
    else
        echo "Unable to proceed (delete $out_dir, use a different directory or \
             re-run with -f)"
        exit 1
    fi
fi

echo "creating $out_dir..."

mkdir -p "$out_dir"

echo "creating track01..."
dd if=/dev/zero of=$track01_path bs=2352 count=$track01_len >/dev/null 2>&1

echo "creating track02..."
dd if=/dev/zero of=$track02_path bs=2352 count=$track02_len >/dev/null 2>&1

echo "generating iso9660fs filesystem..."
msinfo="0,$(expr $track03_lba)"

fs_path="$out_dir/trash.iso"

mkisofs_args=""

if $rock_ridge = true; then
    echo "using rock_ridge"
    echo "$rock_ridge"
    mkisofs_args="$mkisofs_args -R"
fi

$MKISOFS_CMD $mkisofs_args -C$msinfo -o $fs_path $in_dir

fs_len="$(ls -l $fs_path  | awk '{ print $5 }')"

if test $(expr $fs_len % 2048) -ne 0 ; then
    echo "ERROR - filesystem length is not an integral number of blocks"
    exit 1
fi

block_count=$(expr $fs_len / 2048)

echo "filesystem length is $block_count blocks"
echo "padding filesystem with empty meta-data..."

touch $track03_path

block_no=0
while test $block_no -lt $block_count ; do
    dd if=/dev/zero of=$track03_path bs=16 count=1 oflag=append conv=notrunc > /dev/null 2>&1
    dd if=$fs_path of=$track03_path bs=2048 skip=$block_no count=1 oflag=append conv=notrunc > /dev/null 2>&1
    dd if=/dev/zero of=$track03_path bs=288 count=1 oflag=append conv=notrunc > /dev/null 2>&1

    block_no=$(expr $block_no + 1)
done

echo "done padding filesystem"

rm $fs_path

echo "generating disc.gdi..."

gdi_path="$out_dir/disc.gdi"

echo "3" | tee -a $gdi_path
echo "1 0 4 2352 track01.bin 0" | tee -a $gdi_path
echo "2 $track02_lba 0 2352 track02.raw 0" | tee -a $gdi_path
echo "3 $track03_lba 4 2352 track03.bin 0" | tee -a $gdi_path

echo "operation complete"
