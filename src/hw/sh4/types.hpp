/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include <boost/cstdint.hpp>

typedef boost::uint32_t reg32_t;
typedef boost::uint32_t addr32_t;
typedef boost::uint32_t page_no_t;
typedef boost::uint16_t inst_t; // instruction

/*
 * basic_val_t is the type used for all memory accesses.
 * Any other types will be casted to/from this type.
 *
 * The exception is reading through the instruction path, those
 * components all use inst_t because nothing else would make sense
 * given the context.
 */
typedef boost::uint64_t basic_val_t;

class Sh4;
class Icache;
class Ocache;
class Memory;

#endif
