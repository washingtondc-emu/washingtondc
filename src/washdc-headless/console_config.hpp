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

#ifndef CONSOLE_CONFIG_HPP_
#define CONSOLE_CONFIG_HPP_

/*
 * a 'console' in this context is a collection of per-console configs.
 * this includes RTC value, firmware image and flash image.
 * In the future it will also include which VMUs and controllers are connected.
 *
 * Effectively, it represents a virtual dreamcast console, and you can have
 * several different consoles with different settings just like in real life.
 */

char const *console_get_dir(char const *console_name);
void create_console_dir(char const *console_name);

char const *console_get_rtc_path(char const *console_name);
char const *console_get_firmware_path(char const *console_name);
char const *console_get_flashrom_path(char const *console_name);

#endif
