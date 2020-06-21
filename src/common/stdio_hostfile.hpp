/*******************************************************************************
 *
 * Copyright 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
