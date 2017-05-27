#!/bin/sh

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
# simple tool for getting IP.BIN out of a gdi file's track03.bin
# I expect this to be of limited usefulness, but I have needed it on a couple
# of occasions
#
################################################################################

in_path=$1
out_path=$2

if test "$#" -ne 2 ; then
    echo "usage: $0 <track03.bin path> <output file path>"
    exit 1
fi

iter=0

touch $out_path

while test $iter -ne 16 ; do

    set -x
    skip_count=$(expr $iter "*" 2352 + 16)

    dd if=$in_path of=$out_path bs=1 count=2048 skip=$skip_count oflag=append conv=notrunc > /dev/null 2>&1
    set +x

    iter=$(expr "$iter" + 1)
done

echo "done"
