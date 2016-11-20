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
#include <sstream>
#include <cstring>
#include <list>

#include "BaseException.hpp"
#include "hw/sh4/Memory.hpp"
#include "hw/sh4/sh4.hpp"
#include "RandGenerator.hpp"

// Generator that returns the address
template<typename T>
class AddrGenerator {
public:
    T pick_val(addr32_t addr) {
        return (T)addr;
    }

    /*
     * needed for compatibility, this does nothing
     * because this generator keeps no state.
     */
    void reset() {
    }

    std::string name() const {
        std::stringstream ss;
        ss << "AddrGenerator<" << (sizeof(T) * 8) << " bits>";
        return ss.str();
    }
};

typedef AddrGenerator<boost::uint8_t> AddrGen8;
typedef RandGenerator<boost::uint8_t> RandGen8;
typedef AddrGenerator<boost::uint16_t> AddrGen16;
typedef RandGenerator<boost::uint16_t> RandGen16;
typedef AddrGenerator<boost::uint32_t> AddrGen32;
typedef RandGenerator<boost::uint32_t> RandGen32;
typedef AddrGenerator<boost::uint64_t> AddrGen64;
typedef RandGenerator<boost::uint64_t> RandGen64;


class Test {
public:
    Test(Sh4 *cpu, Memory *ram) {
        this->cpu = cpu;
        this->ram = ram;
    }
    virtual ~Test() {
    }
    virtual int run() = 0;
    virtual std::string name() = 0;
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

    virtual std::string name() {
        return std::string("NullTest");
    }
};

/*
 * really simple test here: fill a large region of memory with 4-byte values
 * which correspond to the addresses where those values are being written, then
 * read them all back to confirm they are what we expected.  This goes off of
 * the CPU's default state, which should be no MMU, and priveleged mode.
 */
template<typename ValType, class Generator>
class BasicMemTest : public Test {
private:
    int offset;
    Generator gen;
public:
    BasicMemTest(Generator gen, Sh4 *cpu, Memory *ram, int offset = 0) :
        Test(cpu, ram) {
        this->offset = offset;
        this->gen = gen;
    }

    int run() {
        int err = 0;

        setup();

        gen.reset();
        addr32_t start = offset;
        addr32_t end = std::min(ram->get_size(), (size_t)0x1fffffff);
        static const addr32_t CACHELINE_MASK = ~0x1f;
        for (addr32_t addr = start;
             ((addr + sizeof(ValType)) & CACHELINE_MASK) + 32 < end;
             addr += sizeof(ValType)) {
            ValType val = gen.pick_val(addr);

            if ((err = cpu->write_mem(val, addr, sizeof(ValType))) != 0) {
                std::cout << "Error while writing 0x" << std::hex << addr <<
                    " to 0x" << std::hex << addr << std::endl;
                return err;
            }
        }

        std::cout << "Now verifying that values written are correct..." <<
            std::endl;

        gen.reset();

        // read all the values and check that they match expectations
        for (addr32_t addr = start;
             ((addr + sizeof(ValType)) & CACHELINE_MASK) + 32 < end;
             addr += sizeof(ValType)) {
            basic_val_t val = 0;
            if ((err = cpu->read_mem(&val, addr, sizeof(ValType))) != 0) {
                std::cout << "Error while reading four bytes from 0x" <<
                    addr << std::endl;
                return err;
            }

            ValType expected_val = gen.pick_val(addr);
            // should be a nop since both are uint32_t
            if (val != expected_val) {
                std::cout << "Mismatch at address 0x" << std::hex << addr <<
                    ": got 0x" << std::hex << val << ", expected 0x" <<
                    std::hex << expected_val << std::endl;
                return 1;
            }
        }

        std::cout << "Now verifying that values read through the instruction "
            "read path are correct..." << std::endl;

        gen.reset();

        // now read all the values through the instruction path
        for (addr32_t addr = start;
             ((addr + sizeof(ValType)) & CACHELINE_MASK) + 32 < end;
             addr += sizeof(ValType)) {
            inst_t inst;

            if ((err = cpu->read_inst(&inst, addr)) != 0) {
                std::cout << "Error while reading instruction from 0x" <<
                    addr << std::endl;
                return err;
            }

            /*
             * in case ValType is narrower than inst_t (ie uint8_t), clear any
             * bits which may be set in inst_t that aren't set in ValType
             */
            inst &= ValType(-1);

            inst_t expected_val = gen.pick_val(addr);
            if (inst != expected_val) {
                std::cout << "Mismatch at address 0x" << std::hex << addr <<
                    ": got 0x" << std::hex << inst << ", expected 0x" <<
                    std::hex << expected_val << std::endl;
                return 1;
            }
        }

        return 0;
    }

    // called at the beginning of run to set up the CPU's state.
    virtual void setup() {
    }

    virtual std::string name() {
        std::stringstream ss;
        ss << "BasicMemTest <offset=" << get_offset() << ", size=" <<
            (sizeof(ValType) * 8) << " bits, generator=" << gen.name() << ">";
        return ss.str();
    }

    int get_offset() const {
        return offset;
    }

    void set_oix(bool enable) {
        if (enable)
            this->cpu->cache_reg.ccr |= Sh4::CCR_OIX_MASK;
        else
            this->cpu->cache_reg.ccr &= ~Sh4::CCR_OIX_MASK;
    }

    void set_iix(bool enable) {
        if (enable)
            this->cpu->cache_reg.ccr |= Sh4::CCR_IIX_MASK;
        else
            this->cpu->cache_reg.ccr &= ~Sh4::CCR_IIX_MASK;
    }

    /*
     * simoltaneously enables/disables the writethrough and callback flags
     * so that either writethrough is enabled and callback is disabled
     * (enable=true) or writethrough is disabled and callback is enabled
     * (enable = false).
     */
    void set_wt(bool enable) {
        if (enable) {
            this->cpu->cache_reg.ccr |= Sh4::CCR_WT_MASK;
            this->cpu->cache_reg.ccr &= ~Sh4::CCR_CB_MASK;
        } else {
            this->cpu->cache_reg.ccr &= ~Sh4::CCR_WT_MASK;
            this->cpu->cache_reg.ccr |= Sh4::CCR_CB_MASK;
        }
    }
};

/*
 * really simple test here: fill a large region of memory with 4-byte values
 * which correspond to the addresses where those values are being written, then
 * read them all back to confirm they are what we expected.  This goes off of
 * the CPU's default state, which should be no MMU, and priveleged mode, BUT we
 * also optionally set the OIX, IIX, WT, and CB flags in the cache-control
 * register.
 */
template<typename ValType, class Generator>
class BasicMemTestWithFlags : public BasicMemTest<ValType, Generator> {
public:
    BasicMemTestWithFlags(Generator gen, Sh4 *cpu, Memory *ram,
                          int offset = 0, bool oix = false, bool iix = false,
                          bool wt = false) :
        BasicMemTest<ValType, Generator>(gen, cpu, ram, offset) {
        this->oix = oix;
        this->iix = iix;
        this->wt = wt;
    }

    virtual void setup() {
        // turn on oix and iix
        this->set_oix(oix);
        this->set_iix(iix);
        this->set_wt(wt);
    }

    virtual std::string name() {
        std::stringstream ss;
        ss << "BasicMemTestWithFlags (offset=" << this->get_offset() <<
            ", oix=" << this->oix << ", iix=" << this->iix << ", wt=" <<
            this->wt << ", cb=" << !this->wt << ")";
        return ss.str();
    }

private:
    bool oix, iix, wt;
};

#ifdef ENABLE_SH4_MMU
/*
 * Set up an mmu mapping, then run through every possible address (in P1 area)
 * and verify that either there was a Data TLB miss exception or the read/write
 * went through as expected.
 */
template <typename ValType, class Generator>
class MmuUtlbMissTest : public Test {
public:
    static const addr32_t CACHELINE_MASK = ~0x1f;

    MmuUtlbMissTest(Generator gen, int offset, int page_sz, Sh4 *cpu,
                    Memory *ram) :
        Test(cpu, ram) {
        this->offset = offset;
        this->gen = gen;
        this->page_sz = page_sz;
    }

    int run() {
        int err;
        unsigned sz_tbl[] = { 1024, 4 * 1024, 64 * 1024, 1024 * 1024 };

        this->gen.reset();
        memset(this->cpu->utlb, 0, sizeof(this->cpu->utlb));
        this->cpu->mmu.mmucr |= Sh4::MMUCR_AT_MASK;

        // map (0xf000 + page_sz) into the first page_sz bytes of virtual memory
        addr32_t phys_addr = 0x0000ffff; // TODO: this ought to be randomized
        unsigned sz = page_sz;
        addr32_t ppn = phys_addr & ~(sz_tbl[page_sz] - 1) & 0x1fffffff;
        bool shared = false;
        bool cacheable = false;
        unsigned priv = 3;
        bool dirty = true;
        bool write_through = false;
        boost::uint32_t utlb_ent = gen_utlb_ent(ppn, sz, shared,
                                                cacheable, priv, dirty,
                                                write_through);
        boost::uint32_t utlb_key = gen_utlb_key(0, 0);
        set_utlb(0, utlb_key, utlb_ent);

        addr32_t start = this->offset;
        addr32_t end = std::min(this->ram->get_size(), (size_t)0xffffffff);

        for (addr32_t addr = start; addr < end; addr += sizeof(ValType)) {
            ValType val = this->gen.pick_val(addr);
            err = this->cpu->write_mem(val, addr, sizeof(ValType));
            if (err == 0) {
                if (addr >= sz_tbl[page_sz]) {
                    std::cout << "Error while writing 0x" << std::hex << addr <<
                        " to 0x" << std::hex << addr <<
                        ": There should have been an error!" << std::endl;
                    return 1;
                }
            } else {
                if (addr < sz_tbl[page_sz]) {
                    std::cout << "Error while writing 0x" << std::hex << addr <<
                        " to 0x" << std::hex << addr <<
                        ": There should not have been an error!" << std::endl;
                    return 1;
                } else {
                    // make sure it's the right kind of error
                    reg32_t excp =
                        (this->cpu->excp_reg.expevt & Sh4::EXPEVT_CODE_MASK) >>
                        Sh4::EXPEVT_CODE_SHIFT;
                    if (excp != Sh4::EXCP_DATA_TLB_WRITE_MISS) {
                        std::cout << "Error: The wrong kind of error!" << std::endl;
                        std::cout << "Was expecting 0x" << std::hex <<
                            Sh4::EXCP_DATA_TLB_WRITE_MISS << " but got 0x" <<
                            std::hex << excp << std::endl;
                        return 1;
                    }
                }
            }
        }

        return 0;
    }

    void set_utlb(unsigned utlb_idx, boost::uint32_t utlb_key,
                  boost::uint32_t utlb_ent) {
        if (utlb_idx >= Sh4::UTLB_SIZE)
            throw InvalidParamError("Bad utlb index!");

        this->cpu->utlb[utlb_idx].key = utlb_key;
        this->cpu->utlb[utlb_idx].ent = utlb_ent;
    }

    boost::uint32_t gen_utlb_key(unsigned asid, unsigned vpn, bool valid=true) {
        return ((asid << Sh4::UTLB_KEY_ASID_SHIFT) & Sh4::UTLB_KEY_ASID_MASK) |
            ((vpn << Sh4::UTLB_KEY_VPN_SHIFT) & Sh4::UTLB_KEY_VPN_MASK) |
            (((valid ? 1 : 0) << Sh4::UTLB_KEY_VALID_SHIFT) &
             Sh4::UTLB_KEY_VALID_MASK);
    }

    boost::uint32_t gen_utlb_ent(unsigned ppn, unsigned sz, bool shared,
                                 bool cacheable, unsigned priv, bool dirty,
                                 bool write_through) {
        int sh = shared ? 1 : 0;
        int c = cacheable ? 1 : 0;
        int d = dirty ? 1 : 0;
        int wt = write_through ? 1 : 0;

        boost::uint32_t ret = (ppn << Sh4::UTLB_ENT_PPN_SHIFT) &
            Sh4::UTLB_ENT_PPN_MASK;
        ret |= (sz << Sh4::UTLB_ENT_SZ_SHIFT) & Sh4::UTLB_ENT_SZ_MASK;
        ret |= (sh << Sh4::UTLB_ENT_SH_SHIFT) & Sh4::UTLB_ENT_SH_MASK;
        ret |= (c << Sh4::UTLB_ENT_C_SHIFT) & Sh4::UTLB_ENT_C_MASK;
        ret |= (priv << Sh4::UTLB_ENT_PR_SHIFT) & Sh4::UTLB_ENT_PR_MASK;
        ret |= (d << Sh4::UTLB_ENT_D_SHIFT) & Sh4::UTLB_ENT_D_MASK;
        ret |= (wt << Sh4::UTLB_ENT_WT_SHIFT) & Sh4::UTLB_ENT_WT_MASK;

        return ret;
    }

    virtual std::string name() {
        std::stringstream ss;
        ss << "MmuTlbBasicMissTest<offset=" << this->offset << ", page_sz=" <<
            page_sz << ">";
        return ss.str();
    }

private:
    Generator gen;
    int offset;
    int page_sz;
};
#endif

typedef std::list<Test*> TestList;

static TestList tests;

void instantiate_tests(Sh4 *cpu, Memory *ram) {
    /*
     * The 32-bit memory tests all use AddrGen because there is a 1:1 mapping
     * between 32-bit address and 32-bit data.  With AddrGen, it is easy to tell
     * where a bad write came from because it is recorded in the (incorrect)
     * data that was read back.
     *
     * The other tests all use RandGen because AddrGen would get truncated, so
     * there would be a higher chance for false-negatives (since two separate
     * cache-lines could easily have the same data when that data is AddrGen
     * casted to uint8_t) and also it would not be easy to tell where the
     * garbage data is coming from like it is with 32-bit.
     */

    // I almost want to give up on the arbitrary 80-column limit now
    tests.push_back(new NullTest(cpu, ram));
    tests.push_back(new BasicMemTest<boost::uint32_t, AddrGen32>(AddrGen32(),
                                                                 cpu, ram, 0));
    tests.push_back(new BasicMemTest<boost::uint32_t, AddrGen32>(AddrGen32(),
                                                                 cpu, ram, 1));
    tests.push_back(new BasicMemTest<boost::uint32_t, AddrGen32>(AddrGen32(),
                                                                 cpu, ram, 2));
    tests.push_back(new BasicMemTest<boost::uint32_t, AddrGen32>(AddrGen32(),
                                                                 cpu, ram, 3));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 0, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 1, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 2, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 3, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 0, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 1, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 2, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint32_t, AddrGen32>(
                        AddrGen32(), cpu, ram, 3, true, true, true));

    tests.push_back(new BasicMemTest<boost::uint64_t, RandGen64>(RandGen64(),
                                                                 cpu, ram, 0));
    tests.push_back(new BasicMemTest<boost::uint64_t, RandGen64>(RandGen64(),
                                                                 cpu, ram, 1));
    tests.push_back(new BasicMemTest<boost::uint64_t, RandGen64>(RandGen64(),
                                                                 cpu, ram, 2));
    tests.push_back(new BasicMemTest<boost::uint64_t, RandGen64>(RandGen64(),
                                                                 cpu, ram, 3));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 0, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 1, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 2, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 3, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 0, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 1, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 2, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint64_t, RandGen64>(
                        RandGen64(), cpu, ram, 3, true, true, true));

    tests.push_back(new BasicMemTest<boost::uint16_t, RandGen16>(RandGen16(),
                                                                 cpu, ram, 0));
    tests.push_back(new BasicMemTest<boost::uint16_t, RandGen16>(RandGen16(),
                                                                 cpu, ram, 1));
    tests.push_back(new BasicMemTest<boost::uint16_t, RandGen16>(RandGen16(),
                                                                 cpu, ram, 2));
    tests.push_back(new BasicMemTest<boost::uint16_t, RandGen16>(RandGen16(),
                                                                 cpu, ram, 3));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 0, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 1, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 2, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 3, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 0, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 1, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 2, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint16_t, RandGen16>(
                        RandGen16(), cpu, ram, 3, true, true, true));

    tests.push_back(new BasicMemTest<boost::uint8_t, RandGen8>(RandGen8(),
                                                               cpu, ram, 0));
    tests.push_back(new BasicMemTest<boost::uint8_t, RandGen8>(RandGen8(),
                                                               cpu, ram, 1));
    tests.push_back(new BasicMemTest<boost::uint8_t, RandGen8>(RandGen8(),
                                                               cpu, ram, 2));
    tests.push_back(new BasicMemTest<boost::uint8_t, RandGen8>(RandGen8(),
                                                               cpu, ram, 3));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 0, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 1, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 2, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 3, true, true, false));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 0, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 1, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 2, true, true, true));
    tests.push_back(new BasicMemTestWithFlags<boost::uint8_t, RandGen8>(
                        RandGen8(), cpu, ram, 3, true, true, true));

#ifdef ENABLE_SH4_MMU
    for (int page_sz = 0; page_sz < 4; page_sz++) {
        tests.push_back(new MmuUtlbMissTest<boost::uint8_t, RandGen8>(
                            RandGen8(), 0, page_sz, cpu, ram));
        tests.push_back(new MmuUtlbMissTest<boost::uint16_t, RandGen16>(
                            RandGen16(), 0, page_sz, cpu, ram));
        tests.push_back(new MmuUtlbMissTest<boost::uint32_t, RandGen32>(
                            RandGen32(), 0, page_sz, cpu, ram));
        tests.push_back(new MmuUtlbMissTest<boost::uint64_t, RandGen64>(
                            RandGen64(), 0, page_sz, cpu, ram));
    }
#endif
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
        std::string test_name = (*it)->name();
        std::cout << "Running " << test_name << "..." << std::endl;
        if ((*it)->run() == 0) {
            n_success++;
            std::cout << test_name << " completed successfully" << std::endl;
        } else {
            std::cout << test_name << " failed" << std::endl;
        }
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << std::dec << tests.size() << " tests run - " << n_success <<
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
