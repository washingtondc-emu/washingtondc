/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef CMD_H_
#define CMD_H_

/*
 * This is the component of the cmd system which actually implements a
 * command-line by parsing incoming text and running commands.
 *
 * Everything here runs from the emu thread, even the commands themselves.
 */

/*
 * input a character to the cli.  As soon as a newline comes in, it will
 * execute a command.
 */
void cmd_put_char(char ch);

void cmd_print_prompt(void);

void cmd_print_banner(void);

#endif
