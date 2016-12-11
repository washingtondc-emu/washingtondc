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

class InitError : public std::exception {
public:
    InitError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

// IntegrityError - for things that *should* be impossible
class IntegrityError : public std::exception {
public:
    IntegrityError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

class MemBoundsError : public std::exception {
public:
    MemBoundsError(unsigned addr) {
        this->addr = addr;
    }

    char const* what() const throw() {
        // TODO: IDK how to put the addr in the what() output without
        //       making an allocation that may throw another exception
        return "Memory access error (bad address)";
    }
private:
    unsigned addr;
};

class MemAlignError : public std::exception {
public:
    MemAlignError(unsigned addr) {
        this->addr = addr;
    }
    char const* what() const throw() {
        // TODO: IDK how to put the addr in the what() output without
        //       making an allocation that may throw another exception
        return "Memory access error (unaligned 16-bit read or write)";
    }
private:
    unsigned addr;
};

class InvalidParamError : public std::exception {
public:
    InvalidParamError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

class UnimplementedError : public std::exception {
public:
    UnimplementedError(char const *inst_name) {
        this->inst_name = inst_name;
    }

    char const *what() const throw() {
        return inst_name;
    }
private:
    char const *inst_name;
};

class UnimplementedInstructionError : public std::exception {
public:
    UnimplementedInstructionError(char const *inst_name) {
        this->inst_name = inst_name;
    }

    char const *what() const throw() {
        return inst_name;
    }
private:
    char const *inst_name;
};

class BadOpcodeError : public std::exception {
public:
    char const *what() const throw() {
        return "Bad opcode";
    }
};

class StackUnderflowError : public std::exception {
public:
    char const *what() const throw() {
        return "Stack underflow";
    }
};

class StackOverflowError : public std::exception {
public:
    char const *what() const throw() {
        return "Stack overflow";
    }
};

class InvalidRegisterError : public std::exception {
public:
    char const *what() const throw() {
        return "Invalid register";
    }
};

#endif
