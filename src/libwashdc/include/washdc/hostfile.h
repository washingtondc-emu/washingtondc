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
