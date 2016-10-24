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

#include <algorithm>
#include <iostream>
#include <list>

#include "hw/sh4/Memory.hpp"
#include "hw/sh4/sh4.hpp"

class Test {
public:
    Test(Sh4 *cpu, Memory *ram) {
        this->cpu = cpu;
        this->ram = ram;
    }
    virtual ~Test() {
    }
    virtual int run() = 0;
    virtual char const *name() = 0;
protected:
    Sh4 *cpu;
    Memory *ram;
};

// the NullTest - does nothing, always passes
class NullTest : public Test {
public:
    NullTest(Sh4 *cpu, Memory *ram) : Test(cpu, ram) {
    }

    int run() {
        return 0;
    }

    virtual char const *name() {
        return "NullTest";
    }
};

/*
 * really simple test here: fill a large region of memory with 4-byte values
 * which correspond to the addresses where those values are being written, then
 * read them all back to confirm they are what we expected.  This goes off of
 * the CPU's default state, which should be no MMU, and priveleged mode.
 */
class BasicMemTest : public Test {
public:
    BasicMemTest(Sh4 *cpu, Memory *ram) : Test(cpu, ram) {
    }

    int run() {
        int err = 0;

        addr32_t start = 0;
        addr32_t end = std::min(ram->get_size(), (size_t)0x1fffffff);
        for (addr32_t addr = start; addr + 32 < end; addr += 4) {
            if ((err = cpu->write_mem(addr, addr, 4)) != 0) {
                std::cout << "Error while writing 0x" << std::hex << addr <<
                    " to 0x" << std::hex << addr << std::endl;
                return err;
            }
        }

        std::cout << "Now verifying that values written are correct..." <<
            std::endl;

        // read all the values and check that they match expectations
        for (addr32_t addr = start; addr + 32 < end; addr += 4) {
            boost::uint32_t val;
            if ((err = cpu->read_mem(&val, addr, 4)) != 0) {
                std::cout << "Error while reading four bytes from 0x" <<
                    addr << std::endl;
                return err;
            }

            // should be a nop since both are uint32_t
            addr32_t val_as_addr = val;

            if (val_as_addr != addr) {
                std::cout << "Mismatch at address 0x" << std::hex << addr <<
                    ": got 0x" << std::hex << val_as_addr << ", expected 0x" <<
                    std::hex << addr << std::endl;
                return 1;
            }
        }

        return 0;
    }

    virtual char const *name() {
        return "BasicMemTest";
    }
};

/*
 * really simple test here: fill a large region of memory with 4-byte values
 * which correspond to the addresses where those values are being written, then
 * read them all back to confirm they are what we expected.  This goes off of
 * the CPU's default state, which should be no MMU, and priveleged mode, BUT we
 * also set the OIX bit which screws around with the cache line entry selector
 * a bit.
 */
class BasicMemTestWithOix : public Test {
public:
    BasicMemTestWithOix(Sh4 *cpu, Memory *ram) : Test(cpu, ram) {
    }

    int run() {
        int err = 0;

        // turn on oix
        cpu->cache_reg.ccr |= Sh4::CCR_OIX_MASK;

        addr32_t start = 0;
        addr32_t end = std::min(ram->get_size(), (size_t)0x1fffffff);
        for (addr32_t addr = start; addr + 32 < end; addr += 4) {
            if ((err = cpu->write_mem(addr, addr, 4)) != 0) {
                std::cout << "Error while writing 0x" << std::hex << addr <<
                    " to 0x" << std::hex << addr << std::endl;
                return err;
            }
        }

        std::cout << "Now verifying that values written are correct..." <<
            std::endl;

        // read all the values and check that they match expectations
        for (addr32_t addr = start; addr + 32 < end; addr += 4) {
            boost::uint32_t val;
            if ((err = cpu->read_mem(&val, addr, 4)) != 0) {
                std::cout << "Error while reading four bytes from 0x" <<
                    addr << std::endl;
                return err;
            }

            // should be a nop since both are uint32_t
            addr32_t val_as_addr = val;

            if (val_as_addr != addr) {
                std::cout << "Mismatch at address 0x" << std::hex << addr <<
                    ": got 0x" << std::hex << val_as_addr << ", expected 0x" <<
                    std::hex << addr << std::endl;
                return 1;
            }
        }

        return 0;
    }

    virtual char const *name() {
        return "BasicMemTestWithOix";
    }
};

typedef std::list<Test*> TestList;

static TestList tests;

void instantiate_tests(Sh4 *cpu, Memory *ram) {
    tests.push_back(new NullTest(cpu, ram));
    tests.push_back(new BasicMemTest(cpu, ram));
    tests.push_back(new BasicMemTestWithOix(cpu, ram));
}

void cleanup_tests() {
    for (TestList::iterator it = tests.begin(); it != tests.end(); it++) {
        delete *it;
    }
}

int run_tests() {
    unsigned n_success = 0;
    unsigned n_tests = tests.size();

    for (TestList::iterator it = tests.begin(); it != tests.end(); it++) {
        char const *test_name = (*it)->name();
        std::cout << "Running " << test_name << "..." << std::endl;
        if ((*it)->run() == 0) {
            n_success++;
            std::cout << test_name << " completed successfully" << std::endl;
        } else {
            std::cout << test_name << " failed" << std::endl;
        }
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << tests.size() << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;

    if (n_success == n_tests)
        return 0;
    return 1;
}

int main(int argc, char **argv) {
    Memory mem(16 * 1024 * 1024);
    Sh4 cpu(&mem);

    instantiate_tests(&cpu, &mem);

    int ret_val = run_tests();

    cleanup_tests();

    return ret_val;
}
