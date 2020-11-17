/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifndef WASHDC_I_HATE_WINDOWS_H_
#define WASHDC_I_HATE_WINDOWS_H_

/*
 * Windows has a strange set of arbitrary rules regarding its windows.h header.
 * For one thing, specific preprocessor macros must be defined to specific
 * values in order to access certain functions.  For another thing, winsock2.h
 * must also be included before windows.h.  Since several headers implicitly
 * include windows.h (including C library headers), this must always be the
 * first file included.
 */

#ifdef _WIN32

#ifndef WINVER
#define WINVER 0x0600
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <windows.h>
#include <KnownFolders.h>
#include <Shlobj.h>

#endif

#endif
