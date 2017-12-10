/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
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

#ifndef MEM_CODE_H_
#define MEM_CODE_H_

// common error codes returned by the various memory access/mapping functions

// sh4 CPU exception raised during memory access; access aborted
#define MEM_ACCESS_EXC 1

// memory access succeeded
#define MEM_ACCESS_SUCCESS 0

/*
 * access failed due to a bug or unimplemented feature in WashingtonDC.
 *
 * the access was aborted and the appropriate details have been sent to the
 * error reporting-system, but the error was not raised.
 */
#define MEM_ACCESS_FAILURE -1

#endif
