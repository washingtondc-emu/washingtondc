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

#ifndef CMD_THREAD_H_
#define CMD_THREAD_H_

void cmd_thread_launch(void);

void cmd_thread_join(void);

void cmd_thread_kick(void);

/*
 * input a character to the cmd system.
 * This is NOT the function you call to print to the console, it's the function
 * you call to input a character as if it was typed by the user.
 *
 * This should only be called from within the cmd_thread
 */
void cmd_thread_put_char(char c);

/*
 * print text to the cmd system.
 *
 * Anything you send to this function will get echoed to the cmd frontend.
 * this function can only be safely called from within the cmd_thread.
 */
void cmd_thread_print(char const *txt);

#endif
