/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "log.h"
#include "mem_areas.h"
#include "washdc/error.h"
#include "memory.h"

#include "load_elf.h"

int load_elf(washdc_hostfile file, struct Memory *dc_mem) {
    unsigned char ident[16];
    uint16_t type, machine, phentsize, phnum;
    uint32_t version, entry, phoff;

    washdc_hostfile_read(file, ident, sizeof(ident));
    washdc_hostfile_read(file, &type, sizeof(type));
    washdc_hostfile_read(file, &machine, sizeof(machine));
    washdc_hostfile_read(file, &version, sizeof(version));
    washdc_hostfile_read(file, &entry, sizeof(entry));
    washdc_hostfile_read(file, &phoff, sizeof(phoff));
    washdc_hostfile_seek(file, 10, WASHDC_HOSTFILE_SEEK_CUR);
    washdc_hostfile_read(file, &phentsize, sizeof(phentsize));
    washdc_hostfile_read(file, &phnum, sizeof(phnum));

    if (memcmp(ident, "\177ELF", 4) != 0) {
        LOG_ERROR("NOT A VALID ELF FILE\n");
        goto return_error;
    }

    if (ident[4] != 1) {
        LOG_ERROR("NOT A 32-BIT CPU ARCHITECTURE\n");
        goto return_error;
    }

    if (ident[5] != 1) {
        LOG_ERROR("NOT LITTLE-ENDIAN\n");
        goto return_error;
    }

    if (ident[6] != 1) {
        LOG_ERROR("UNKNOWN VERSION %d\n", (int)ident[6]);
        goto return_error;
    }

    if (type != 2) {
        LOG_ERROR("NOT AN EXECUTABLE ELF FILE\n");
        goto return_error;
    }

    if (machine != 42) {
        LOG_ERROR("NOT A HITACHI/RENESAS EXECUTABLE\n");
        goto return_error;
    }

    if (version != 1) {
        LOG_ERROR("UNKNOWN VERSION %d\n", (int)version);
        goto return_error;
    }

    washdc_hostfile_seek(file, phoff, WASHDC_HOSTFILE_SEEK_BEG);

    if (phentsize < 8) {
        LOG_ERROR("program headers too small\n");
        goto return_error;
    }

    uint32_t *prog_hdr = malloc(phentsize);
    if (!prog_hdr)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    unsigned idx;
    for (idx = 0; idx < phnum; idx++) {
        if (washdc_hostfile_read(file, prog_hdr, phentsize) != phentsize) {
            RAISE_ERROR(ERROR_FILE_IO);
        }

        if (prog_hdr[4] > prog_hdr[5]) {
            LOG_ERROR("CORRUPTED ELF: filesz (%08x) is greater than memsz (%08x)\n",
                      (unsigned)prog_hdr[4], (unsigned)prog_hdr[5]);
            free(prog_hdr);
            goto return_error;
        }

        void *buf = calloc(1, prog_hdr[5]);

        long pos = washdc_hostfile_tell(file);
        errno = 0;
        int res;
        if ((res = washdc_hostfile_seek(file, prog_hdr[1], WASHDC_HOSTFILE_SEEK_BEG)) < 0) {
            RAISE_ERROR(ERROR_FILE_IO);
        }
        if (washdc_hostfile_read(file, buf, prog_hdr[4]) != prog_hdr[4])
            RAISE_ERROR(ERROR_FILE_IO);
        if (washdc_hostfile_seek(file, pos, WASHDC_HOSTFILE_SEEK_BEG) < 0) {
            RAISE_ERROR(ERROR_FILE_IO);
        }

        memory_write(dc_mem, buf, prog_hdr[3] & ADDR_AREA3_MASK, prog_hdr[5]);

        free(buf);
    }
    free(prog_hdr);
    return 0;

 return_error:
    return -1;
}
