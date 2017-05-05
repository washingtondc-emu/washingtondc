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

#include <boost/exception/all.hpp>
#include <boost/cstdint.hpp>

#include "types.h"

typedef boost::error_info<struct tag_feature_name_error_info, std::string>
errinfo_feature;

typedef boost::error_info<struct tag_param_name_error_info, std::string>
errinfo_param_name;

typedef boost::error_info<struct tag_wtf_error_info, std::string> errinfo_wtf;

typedef boost::error_info<struct tag_guest_addr_error_info, addr32_t>
errinfo_guest_addr;

typedef boost::error_info<struct tag_op_type_error_info, std::string>
errinfo_op_type;

/*
 * errinfo_advice - for when the program already
 * knows what you need to do to fix something.
 */
typedef boost::error_info<struct tag_advice_error_info, std::string>
errinfo_advice;

typedef boost::error_info<struct tag_length_error_info, size_t> errinfo_length;
typedef boost::error_info<struct tag_length_error_info, size_t>
errinfo_length_expect;

typedef boost::error_info<struct tag_val32_error_info, uint8_t> errinfo_val8;
typedef boost::error_info<struct tag_val32_error_info, uint16_t> errinfo_val16;
typedef boost::error_info<struct tag_val32_error_info, uint32_t> errinfo_val32;
typedef boost::error_info<struct tag_val32_error_info, uint64_t> errinfo_val64;

typedef boost::error_info<struct tag_path_error_info, std::string> errinfo_path;

class BaseException : public virtual std::exception,
                      public virtual boost::exception {
};

class InitError : public BaseException {
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

class InvalidFileLengthError : public BaseException {
    char const *what() const throw() {
        return "Invalid file length";
    }
};

// IntegrityError - for things that *should* be impossible
class IntegrityError : public BaseException {
public:
    IntegrityError() {
        this->desc = "IntegrityError";
    }

    IntegrityError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

class MemBoundsError : public BaseException {
public:
    MemBoundsError() {
        this->addr = 0xdeadbeef;
    }

    MemBoundsError(unsigned addr) {
        this->addr = addr;
    }

    char const* what() const throw() {
        return "out-of-bounds memory acces";
    }
private:
    unsigned addr;
};

class MemAlignError : public BaseException {
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

class InvalidParamError : public BaseException {
public:
    InvalidParamError() {
        this->desc = "Invalid parameter value";
    }

    InvalidParamError(char const *desc) {
        this->desc = desc;
    }

    char const *what() const throw() {
        return desc;
    }
private:
    char const *desc;
};

class UnimplementedError : public BaseException {
public:
    UnimplementedError() {
        inst_name = "Unable to continue because an unimplemented "
            "feature is required";
    }

    UnimplementedError(char const *inst_name) {
        this->inst_name = inst_name;
    }

    char const *what() const throw() {
        return inst_name;
    }
private:
    char const *inst_name;
};

class UnimplementedInstructionError : public BaseException {
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

class BadOpcodeError : public BaseException {
public:
    char const *what() const throw() {
        return "Bad opcode";
    }
};

class StackUnderflowError : public BaseException {
public:
    char const *what() const throw() {
        return "Stack underflow";
    }
};

class StackOverflowError : public BaseException {
public:
    char const *what() const throw() {
        return "Stack overflow";
    }
};

class InvalidRegisterError : public BaseException {
public:
    char const *what() const throw() {
        return "Invalid register";
    }
};

#endif
