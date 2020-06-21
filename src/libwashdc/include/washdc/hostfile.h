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

#ifndef WASHDC_HOSTFILE_H_
#define WASHDC_HOSTFILE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* washdc_hostfile;
#define WASHDC_HOSTFILE_INVALID NULL

enum washdc_hostfile_mode {
    WASHDC_HOSTFILE_READ = 1,
    WASHDC_HOSTFILE_WRITE = 2,
    WASHDC_HOSTFILE_BINARY = 4,
    WASHDC_HOSTFILE_DONT_OVERWRITE = 8,
    WASHDC_HOSTFILE_TEXT = 0
};

enum washdc_hostfile_seek_origin {
    WASHDC_HOSTFILE_SEEK_BEG,
    WASHDC_HOSTFILE_SEEK_CUR,
    WASHDC_HOSTFILE_SEEK_END
};

#define WASHDC_HOSTFILE_EOF 0xfeedface

struct washdc_hostfile_api {
    washdc_hostfile(*open)(char const *path, enum washdc_hostfile_mode mode);
    void(*close)(washdc_hostfile file);
    int(*seek)(washdc_hostfile file, long disp,
               enum washdc_hostfile_seek_origin origin);
    long(*tell)(washdc_hostfile file);
    size_t(*read)(washdc_hostfile file, void *outp, size_t len);
    size_t(*write)(washdc_hostfile file, void const *inp, size_t len);
    int(*flush)(washdc_hostfile file);

    washdc_hostfile(*open_cfg_file)(enum washdc_hostfile_mode mode);
    washdc_hostfile(*open_screenshot)(char const *name, enum washdc_hostfile_mode mode);

    char pathsep;
};

washdc_hostfile washdc_hostfile_open_cfg_file(enum washdc_hostfile_mode mode);
washdc_hostfile washdc_hostfile_open_screenshot(char const *name,
                                                enum washdc_hostfile_mode mode);

washdc_hostfile washdc_hostfile_open(char const *path,
                                     enum washdc_hostfile_mode mode);
void washdc_hostfile_close(washdc_hostfile file);
int washdc_hostfile_seek(washdc_hostfile file,
                         long disp,
                         enum washdc_hostfile_seek_origin origin);
long washdc_hostfile_tell(washdc_hostfile file);
size_t washdc_hostfile_read(washdc_hostfile file, void *outp, size_t len);
size_t washdc_hostfile_write(washdc_hostfile file, void const *inp, size_t len);
int washdc_hostfile_flush(washdc_hostfile file);

int washdc_hostfile_putc(washdc_hostfile file, char ch);
int washdc_hostfile_puts(washdc_hostfile file, char const *str);
int washdc_hostfile_getc(washdc_hostfile file);
void washdc_hostfile_printf(washdc_hostfile file, char const *fmt, ...);
char washdc_hostfile_pathsep(void);

#ifdef __cplusplus
}
#endif

#endif
