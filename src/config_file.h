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

#ifndef CONFIG_FILE_H_
#define CONFIG_FILE_H_

/*
 * text file containing configuration settings.
 *
 * This is completely unrelated to the bullshit in config.h/config.c; that only
 * pertains to runtime settings and not everything in there even maps to the
 * config file.
 */

void cfg_init(void);
void cfg_cleanup(void);

void cfg_put_char(char ch);

char const *cfg_get_node(char const *key);

int cfg_get_bool(char const *key, bool *outp);

int cfg_get_rgb(char const *key, int *red, int *green, int *blue);

char const *cfg_get_default_dir(void);

char const *cfg_get_default_file(void);

#endif
