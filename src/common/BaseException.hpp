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

#ifndef BASEEXCEPTION_HPP_
#define BASEEXCEPTION_HPP_

#include <string>
#include <exception>

class InitError : std::exception {
public:
    InitError(char const *desc) {
        this->desc = desc;
    }

    char const *what() {
        return desc;
    }
private:
    char const *desc;
};

// IntegrityError - for things that *should* be impossible
class IntegrityError : std::exception {
public:
    IntegrityError(char const *desc) {
        this->desc = desc;
    }

    char const *what() {
        return desc;
    }
private:
    char const *desc;
};

class MemBoundsError : std::exception {
public:
    MemBoundsError(unsigned addr) {
        this->addr = addr;
    }

    char const* what() {
        // TODO: IDK how to put the addr in the what() output without
        //       making an allocation that may throw another exception
        return "Memory access error (bad address)";
    }
private:
    unsigned addr;
};

class MemAlignError : std::exception {
public:
    MemAlignError(unsigned addr) {
        this->addr = addr;
    }
    char const* what() {
        // TODO: IDK how to put the addr in the what() output without
        //       making an allocation that may throw another exception
        return "Memory access error (unaligned 16-bit read or write)";
    }
private:
    unsigned addr;
};

class InvalidParamError : std::exception {
public:
    InvalidParamError(char const *desc) {
        this->desc = desc;
    }

    char const *what() {
        return desc;
    }
private:
    char const *desc;
};

class UnimplementedError : std::exception {
public:
    UnimplementedError(char const *inst_name) {
        this->inst_name = inst_name;
    }

    char const *what() {
        return inst_name;
    }
private:
    char const *inst_name;
};

class UnimplementedInstructionError : std::exception {
public:
    UnimplementedInstructionError(char const *inst_name) {
        this->inst_name = inst_name;
    }

    char const *what() {
        return inst_name;
    }
private:
    char const *inst_name;
};

class BadOpcodeError : std::exception {
public:
    char const *what() {
        return "Bad opcode";
    }
};

class StackUnderflowError : std::exception {
public:
    char const *what() {
        return "Stack underflow";
    }
};

class StackOverflowError : std::exception {
public:
    char const *what() {
        return "Stack overflow";
    }
};

class InvalidRegisterError : std::exception {
public:
    char const *what() {
        return "Invalid register";
    }
};

#endif
