/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef STDIO_HOSTFILE_HPP_
#define STDIO_HOSTFILE_HPP_

#include "washdc/hostfile.h"

static washdc_hostfile file_stdio_open(char const *path,
				       enum washdc_hostfile_mode mode) {
    char modestr[4] = { 0 };
    int top = 0;
    if (mode & WASHDC_HOSTFILE_WRITE)
        modestr[top++] = 'w';
    else if (mode & WASHDC_HOSTFILE_READ)
        modestr[top++] = 'r';
    else
        return WASHDC_HOSTFILE_INVALID;

    if (mode & WASHDC_HOSTFILE_BINARY)
        modestr[top++] = 'b';
    if (mode & WASHDC_HOSTFILE_DONT_OVERWRITE)
        modestr[top++] = 'x';

    return fopen(path, modestr);
}

static void file_stdio_close(washdc_hostfile file) {
    fclose((FILE*)file);
}

static int file_stdio_seek(washdc_hostfile file, long disp,
			   enum washdc_hostfile_seek_origin origin) {
    int whence;
    switch (origin) {
    case WASHDC_HOSTFILE_SEEK_BEG:
        whence = SEEK_SET;
        break;
    case WASHDC_HOSTFILE_SEEK_CUR:
        whence = SEEK_CUR;
        break;
    case WASHDC_HOSTFILE_SEEK_END:
        whence = SEEK_END;
        break;
    default:
        return -1;
    }
    return fseek((FILE*)file, disp, whence);
}

static long file_stdio_tell(washdc_hostfile file) {
    return ftell((FILE*)file);
}

static size_t file_stdio_read(washdc_hostfile file, void *outp, size_t len) {
    return fread(outp, 1, len, (FILE*)file);
}

static size_t file_stdio_write(washdc_hostfile file, void const *inp, size_t len) {
    return fwrite(inp, 1, len, (FILE*)file);
}

static int file_stdio_flush(washdc_hostfile file) {
    return fflush((FILE*)file);
}

#endif
