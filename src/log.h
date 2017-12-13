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

#ifndef LOG_H_
#define LOG_H_

#define ENABLE_LOG_ERROR
#define ENABLE_LOG_WARN
#define ENABLE_LOG_INFO

// imminent problem that will impact WashingtonDC's operation
#ifdef ENABLE_LOG_ERROR
#define LOG_ERROR(msg, ...) printf(msg, ##__VA_ARGS__)
#else
#define LOG_ERROR(msg, ...)
#endif

// something that should be noted but probably isn't too important
#ifdef ENABLE_LOG_WARN
#define LOG_WARN(msg, ...) printf(msg, ##__VA_ARGS__)
#else
#define LOG_WARN(msg, ...)
#endif

// used to communicate information to the user
#ifdef ENABLE_LOG_INFO
#define LOG_INFO(msg, ...) printf(msg, ##__VA_ARGS__)
#else
#define LOG_INFO(msg, ...)
#endif

// catch-all for most of the useless crap WashingtonDC dumps to stdout
#ifdef ENABLE_LOG_DEBUG
#define LOG_DBG(msg, ...) printf(msg, ##__VA_ARGS__)
#else
#define LOG_DBG(msg, ...)
#endif

#endif
