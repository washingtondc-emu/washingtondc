#!/bin/sh

################################################################################
#
#    WashingtonDC Dreamcast Emulator
#    Copyright (C) 2020  snickerbockers
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
################################################################################

echo "\"# automatically generated from https://github.com/gabomdq/SDL_GameControllerDB on $(date)\\\\n\"" > sdl_gamecontrollerdb.h
echo "\"\\\\n\"" >> sdl_gamecontrollerdb.h
curl https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/LICENSE | sed "s/\(.*\)/\"# \1\\\\n\"/" >> sdl_gamecontrollerdb.h
echo "\"\\\\n\"" >> sdl_gamecontrollerdb.h
curl https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt | sed "s/\(.*\)/\"\1\\\\n\"/" >> sdl_gamecontrollerdb.h
