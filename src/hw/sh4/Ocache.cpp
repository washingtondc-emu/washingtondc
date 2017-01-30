/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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

#ifndef ENABLE_SH4_OCACHE
#error this file cannot be built with the sh4 operand cache disabled!
#endif

#include <cassert>
#include <cstring>

#include "sh4.hpp"
#include "BaseException.hpp"

#include "Ocache.hpp"

static const size_t SH4_OCACHE_LONGS_PER_CACHE_LINE = 8;
static const size_t SH4_OCACHE_ENTRY_COUNT = 512;
static const size_t SH4_OCACHE_LINE_SHIFT = 5;
static const size_t SH4_OCACHE_LINE_SIZE = SH4_OCACHE_LONGS_PER_CACHE_LINE * 4;
static const size_t SH4_OCACHE_SIZE = SH4_OCACHE_ENTRY_COUNT *
    SH4_OCACHE_LINE_SIZE;

// The valid flag
static const unsigned SH4_OCACHE_KEY_VALID_SHIFT = 0;
static const unsigned SH4_OCACHE_KEY_VALID_MASK = 1 << SH4_OCACHE_KEY_VALID_SHIFT;

// the dirty flag
static const unsigned SH4_OCACHE_KEY_DIRTY_SHIFT = 1;
static const unsigned SH4_OCACHE_KEY_DIRTY_MASK = 1 << SH4_OCACHE_KEY_DIRTY_SHIFT;

// the tag represents bits 28:10 (inclusive) of a 29-bit address.
static const unsigned SH4_OCACHE_KEY_TAG_SHIFT = 2;
static const unsigned SH4_OCACHE_KEY_TAG_MASK = 0x7ffff << SH4_OCACHE_KEY_TAG_SHIFT;

static bool sh4_ocache_check(Sh4Ocache *ocache, sh4_ocache_line_t line_no,
                             addr32_t paddr);
static sh4_ocache_line_t sh4_ocache_selector(addr32_t paddr, bool index_enable,
                                             bool cache_as_ram);
static int sh4_ocache_load(Sh4Ocache *ocache, MemoryMap *mem,
                           sh4_ocache_line_t line_no, addr32_t paddr);
static int sh4_ocache_write_back(Sh4Ocache *ocache, MemoryMap *mem,
                                 sh4_ocache_line_t line_no);
static addr32_t sh4_ocache_line_get_tag(Sh4Ocache const *state,
                                        sh4_ocache_line_t line);
static addr32_t sh4_ocache_tag_from_paddr(addr32_t paddr);
static void sh4_ocache_line_set_tag(Sh4Ocache *state, sh4_ocache_line_t line,
                                    addr32_t tag);
static void* sh4_ocache_get_ram_addr(Sh4Ocache *ocache, addr32_t paddr,
                                     bool index_enable);

template<typename buf_t>
static int sh4_ocache_do_cache_read(Sh4Ocache *ocache, MemoryMap *mem,
                                    buf_t *out, addr32_t paddr,
                                    bool index_enable, bool cache_as_ram);

template<typename buf_t>
static int sh4_ocache_do_cache_write_cb(Sh4Ocache *ocache, MemoryMap *mem,
                                        buf_t const *data, addr32_t paddr,
                                        bool index_enable, bool cache_as_ram);

template<typename buf_t>
static int sh4_ocache_do_cache_write_wt(Sh4Ocache *ocache, MemoryMap *mem,
                                        buf_t const *data, addr32_t paddr,
                                        bool index_enable, bool cache_as_ram);

void sh4_ocache_init(Sh4Ocache *sh4_ocache) {
    sh4_ocache->op_cache_keys = new sh4_ocache_key_t[SH4_OCACHE_ENTRY_COUNT];
    sh4_ocache->op_cache = new uint8_t[SH4_OCACHE_SIZE];
}

void sh4_ocache_cleanup(Sh4Ocache *sh4_ocache) {
    delete[] sh4_ocache->op_cache;
    delete[] sh4_ocache->op_cache_keys;
}

void sh4_ocache_reset(Sh4Ocache *sh4_ocache) {
    memset(sh4_ocache->op_cache_keys, 0,
           sizeof(sh4_ocache->op_cache_keys[0]) * SH4_OCACHE_ENTRY_COUNT);
    memset(sh4_ocache->op_cache, 0, SH4_OCACHE_SIZE);

}

/*
 * Return true if line matches paddr; else return false.
 *
 * This function does not verify that the cache is enabled; nor does it
 * verify that paddr is even in an area which can be cached.  The callee
 * should do that before calling this function.
 *
 * This function does not check the valid bit.
 */
static bool sh4_ocache_check(Sh4Ocache *ocache, sh4_ocache_line_t line_no,
                             addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = sh4_ocache_tag_from_paddr(paddr);

    addr32_t line_tag = sh4_ocache_line_get_tag(ocache, line_no);
    return line_tag == paddr_tag;
}

/*
 * returns the index into the op-cache where paddr
 * would go if it had an entry.
 */
static sh4_ocache_line_t sh4_ocache_selector(addr32_t paddr, bool index_enable,
                                             bool cache_as_ram) {
    sh4_ocache_line_t ent_sel = (paddr & 0x1fe0) >> 5;
    if (index_enable)
        ent_sel |= (paddr & (1 << 25)) >> 12;
    else
        ent_sel |= (paddr & (1 << 13)) >> 5;

    if (cache_as_ram) {
        /*
         * The sh4 hardware manual is a little vague on how this effects
         * the half of the cache which is not being used as memory.
         * As an educated guess, I discard bit 7 (which is always cleared
         * for addresses which would reside in a cache area and set for areas
         * which would reside in a RAM area) and use bit 8 to determine whether
         * the selector should map to the 0:127 range or the 256:383 range.
         *
         * The remaining seven bits of the selector determine which specific
         * cache line within the given range to use.
         */
        ent_sel &= ~(1 << 7);
        assert((ent_sel <= 127) || ((ent_sel >= 256) && (ent_sel <= 383)));
    }

    assert(ent_sel < SH4_OCACHE_ENTRY_COUNT);

    return ent_sel;
}

/*
 * Load the cache-line corresponding to paddr into line.
 * Returns non-zero on failure.
 */
static int sh4_ocache_load(Sh4Ocache *ocache, MemoryMap *mem,
                           sh4_ocache_line_t line_no, addr32_t paddr) {
    size_t n_bytes = sizeof(boost::uint32_t) * SH4_OCACHE_LONGS_PER_CACHE_LINE;
    void *read_dst = ocache->op_cache + line_no * SH4_OCACHE_LINE_SIZE;
    int err_code = mem->read(read_dst, paddr & ~31 & 0x1fffffff, n_bytes);

    if (err_code != 0)
        return err_code;

    sh4_ocache_line_set_tag(ocache, line_no, sh4_ocache_tag_from_paddr(paddr));
    ocache->op_cache_keys[line_no] |= SH4_OCACHE_KEY_VALID_MASK;
    ocache->op_cache_keys[line_no] &= ~SH4_OCACHE_KEY_DIRTY_MASK;

    return 0;
}

/*
 * Write the cache-line into memory and clear its dirty-bit.
 * returns non-zero on failure.
 */
static int sh4_ocache_write_back(Sh4Ocache *ocache, MemoryMap *mem,
                                 sh4_ocache_line_t line_no) {
    size_t n_bytes = sizeof(boost::uint32_t) * SH4_OCACHE_LONGS_PER_CACHE_LINE;

    addr32_t paddr =
        ((ocache->op_cache_keys[line_no] & SH4_OCACHE_KEY_TAG_MASK) >>
         SH4_OCACHE_KEY_TAG_SHIFT) << 10;
    paddr &= 0x7ffff << 10;

    /* bits 12 and 13 are cleared so thar ORA and OIX don't need to be minded.
     * these bits overlap with the tag (bits 28-10), so this should be safe.
     * In the future, a sanity check to make sure these bits match their
     * counterparts in the tag may be warranted.
     */
    paddr |= (line_no << 5) & ~0x3000;

    void *write_dst = ocache->op_cache + line_no * SH4_OCACHE_LINE_SIZE;
    int err_code = mem->write(write_dst, paddr & ~31 & 0x1fffffff, n_bytes);
    if (err_code != 0) {
        return err_code;
    }

    ocache->op_cache_keys[line_no] &= ~SH4_OCACHE_KEY_DIRTY_MASK;
    return 0;
}

static addr32_t sh4_ocache_line_get_tag(Sh4Ocache const *state,
                                        sh4_ocache_line_t line) {
    return (SH4_OCACHE_KEY_TAG_MASK & state->op_cache_keys[line]) >>
        SH4_OCACHE_KEY_TAG_SHIFT;
}

static addr32_t sh4_ocache_tag_from_paddr(addr32_t paddr) {
    return (paddr & 0x1ffffc00) >> 10;
}

static void sh4_ocache_line_set_tag(Sh4Ocache *state, sh4_ocache_line_t line,
                                    addr32_t tag) {
    sh4_ocache_key_t key = state->op_cache_keys[line];
    state->op_cache_keys[line] = (key & ~SH4_OCACHE_KEY_TAG_MASK) |
        (tag << SH4_OCACHE_KEY_TAG_SHIFT);
}

/*
 * returns a pointer to where paddr ought to go if ORA ram is enabled.
 *
 * This function does not verify that paddr is actually a valid address,
 * nor does it handle the possibility that paddr might straddle the border
 * between RAM area 1 and RAM area 2 (which are not adjacent in the
 * operand cache).  The caller must make sure that paddr is actually a RAM
 * address and that the entire length of the read/write operation can be
 * safely executed.
 */
static void* sh4_ocache_get_ram_addr(Sh4Ocache *ocache, addr32_t paddr,
                                     bool index_enable) {
    addr32_t area_offset = paddr & 0xfff;
    addr32_t area_start;
    addr32_t mask;
    if (index_enable)
        mask = 1 << 25;
    else
        mask = 1 << 13;
    if (paddr & mask)
        area_start = SH4_OCACHE_LINE_SIZE * 0x180;
    else
        area_start = SH4_OCACHE_LINE_SIZE * 0x80;
    return ocache->op_cache + area_start + area_offset;
}

int sh4_ocache_alloc(Sh4Ocache *ocache, MemoryMap *mem,
                     addr32_t paddr, bool index_enable,
                     bool cache_as_ram) {
    int err = 0;

    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        // no need to allocate it, it's always going to be part of the cache
        return 0;
    }

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    sh4_ocache_key_t *keyp = ocache->op_cache_keys + line_idx;

    if (*keyp & SH4_OCACHE_KEY_VALID_MASK) {
        if (sh4_ocache_check(ocache, line_idx, paddr))
            return 0; // cache hit, nothing to see here

        if (*keyp & SH4_OCACHE_KEY_DIRTY_MASK) {
            if ((err = sh4_ocache_write_back(ocache, mem, line_idx)))
                return err;
        }

        sh4_ocache_line_set_tag(ocache, line_idx,
                                sh4_ocache_tag_from_paddr(paddr));
        (*keyp) |= SH4_OCACHE_KEY_VALID_MASK;
        (*keyp) &= ~SH4_OCACHE_KEY_DIRTY_MASK;
    } else {
        // cache holds no valid data
        sh4_ocache_line_set_tag(ocache, line_idx,
                                sh4_ocache_tag_from_paddr(paddr));
        (*keyp) |= SH4_OCACHE_KEY_VALID_MASK;
        (*keyp) &= ~SH4_OCACHE_KEY_DIRTY_MASK;
    }

    return err;
}

void sh4_ocache_invalidate(Sh4Ocache *ocache, addr32_t paddr,
                           bool index_enable, bool cache_as_ram) {
    if (cache_as_ram && Sh4::in_oc_ram_area(paddr))
        return;

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);

    if (sh4_ocache_check(ocache, line_idx, paddr))
        ocache->op_cache_keys[line_idx] &= ~SH4_OCACHE_KEY_VALID_MASK;
}

int sh4_ocache_purge(Sh4Ocache *ocache, MemoryMap *mem,
                     addr32_t paddr, bool index_enable,
                     bool cache_as_ram) {
    int err;

    if (cache_as_ram && Sh4::in_oc_ram_area(paddr))
        return 0;

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);

    if (sh4_ocache_check(ocache, line_idx, paddr) &&
        (ocache->op_cache_keys[line_idx] & SH4_OCACHE_KEY_VALID_MASK)) {
        if (ocache->op_cache_keys[line_idx] & SH4_OCACHE_KEY_DIRTY_MASK)
            if ((err = sh4_ocache_write_back(ocache, mem, line_idx)))
                return err;
        ocache->op_cache_keys[line_idx] &= ~SH4_OCACHE_KEY_VALID_MASK;
    }

    return 0;
}

template<typename buf_t>
int sh4_ocache_do_cache_read(Sh4Ocache *ocache, MemoryMap *mem,
                             buf_t *out, addr32_t paddr, bool index_enable,
                             bool cache_as_ram) {
    int err = 0;

    if (paddr & (sizeof(buf_t) - 1)) {
        /*
         * the lazy implementation: do sizeof(buf_t) 1-byte reads.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        buf_t out_buf = 0;
        for (unsigned i = 0; i < sizeof(buf_t); i++) {
            uint8_t tmp;
            int err;

            err = sh4_ocache_do_cache_read<uint8_t>(ocache, mem,
                                                    &tmp, paddr + i,
                                                    index_enable,
                                                    cache_as_ram);
            if (err)
                return err;

            out_buf |= buf_t(tmp) << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        *out = *(buf_t*)sh4_ocache_get_ram_addr(ocache, paddr, index_enable);
        return 0;
    }

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;

    if (ocache->op_cache_keys[line_idx] & SH4_OCACHE_KEY_VALID_MASK) {
        if (sh4_ocache_check(ocache, line_idx, paddr)) {
            // cache hit
            addr32_t byte_offset = paddr & 0x1f;

            *out = 0;
            memcpy(out, line + byte_offset, sizeof(buf_t));
            return 0;
        } else {
            // tag does not match, V bit is 1
            if (ocache->op_cache_keys[line_idx] & SH4_OCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = sh4_ocache_write_back(ocache, mem, line_idx);
                if (err)
                    return err;
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
            } else {
                //cache miss (no write-back)
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
            }
        }
    } else {
        // valid bit is 0, tag may or may not match
        // cache miss (no write-back)
        err = sh4_ocache_load(ocache, mem, line_idx, paddr);
    }

    if (!err) {
        addr32_t byte_offset = paddr & 0x1f;

        *out = 0;
        memcpy(out, line + byte_offset, sizeof(buf_t));
        return 0;
    }

    return err;
}

template<>
int sh4_ocache_do_cache_read<uint8_t>(Sh4Ocache *ocache, MemoryMap *mem,
                                      uint8_t *out, addr32_t paddr,
                                      bool index_enable,
                                      bool cache_as_ram) {
    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        *out = *(uint8_t*)sh4_ocache_get_ram_addr(ocache, paddr,
                                                  index_enable);
        return 0;
    }

    int err = 0;
    uint32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;
    sh4_ocache_key_t *key = ocache->op_cache_keys + line_idx;

    if (*key & SH4_OCACHE_KEY_VALID_MASK) {
        if (sh4_ocache_check(ocache, line_idx, paddr)) {
            // cache hit
            addr32_t idx = paddr & 0x1f;

            *out = line[idx];
            return 0;
        } else {
            // tag does not match, V bit is 1
            if (*key & SH4_OCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = sh4_ocache_write_back(ocache, mem, line_idx);
                if (err)
                    return err;
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
            } else {
                //cache miss (no write-back)
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
            }
        }
    } else {
        // valid bit is 0, tag may or may not match
        // cache miss (no write-back)
        err = sh4_ocache_load(ocache, mem, line_idx, paddr);
    }

    if (!err) {
        addr32_t idx = paddr & 0x1f;

        *out = line[idx];
        return 0;
    }

    return err;
}

template<>
int sh4_ocache_do_cache_write_cb<uint8_t>(Sh4Ocache *ocache, MemoryMap *mem,
                                          uint8_t const *data, addr32_t paddr,
                                          bool index_enable,
                                          bool cache_as_ram) {
    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        uint8_t *ptr = (uint8_t*)sh4_ocache_get_ram_addr(ocache, paddr,
                                                         index_enable);
        *ptr = *data;
        return 0;
    }

    int err = 0;
    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;
    unsigned byte_idx = paddr & 0x1f;
    sh4_ocache_key_t *keyp = ocache->op_cache_keys + line_idx;

    if (sh4_ocache_check(ocache, line_idx, paddr)) {
        if (*keyp & SH4_OCACHE_KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            line[byte_idx] = *data;
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            sh4_ocache_load(ocache, mem, line_idx, paddr);
            line[byte_idx] = *data;
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        }
    } else {
        if (*keyp & SH4_OCACHE_KEY_VALID_MASK) {
            if (*keyp & SH4_OCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = sh4_ocache_write_back(ocache, mem, line_idx);
                if (err)
                    return err;
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
                line[byte_idx] = *data;
                *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                sh4_ocache_load(ocache, mem, line_idx, paddr);
                line[byte_idx] = *data;
                *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            sh4_ocache_load(ocache, mem, line_idx, paddr);
            line[byte_idx] = *data;
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        }
    }

    return 0;
}

template<typename buf_t>
int sh4_ocache_do_cache_write_cb(Sh4Ocache *ocache, MemoryMap *mem,
                                 buf_t const *data, addr32_t paddr,
                                 bool index_enable, bool cache_as_ram) {
    int err = 0;
    buf_t data_val_nbit = *(buf_t*)data;

    if (paddr & (sizeof(buf_t) - 1)) {
        /*
         * the lazy implementation: do 2 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (unsigned i = 0; i < sizeof(buf_t); i++) {
            int err;
            buf_t mask = buf_t(0xff) << (i * 8);

            uint8_t tmp = (mask & data_val_nbit) >> (i * 8);
            err = sh4_ocache_do_cache_write_cb<uint8_t>(ocache, mem, &tmp,
                                                        paddr + i, index_enable,
                                                        cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        buf_t *ptr = (buf_t*)sh4_ocache_get_ram_addr(ocache, paddr,
                                                     index_enable);
        *ptr = *data;
        return 0;
    }

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;
    unsigned byte_idx = paddr & 0x1f;
    sh4_ocache_key_t *keyp = ocache->op_cache_keys + line_idx;

    if (sh4_ocache_check(ocache, line_idx, paddr)) {
        if (*keyp & SH4_OCACHE_KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            memcpy(line + byte_idx, data, sizeof(buf_t));
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            sh4_ocache_load(ocache, mem, line_idx, paddr);
            memcpy(line + byte_idx, data, sizeof(buf_t));
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        }
    } else {
        if (*keyp & SH4_OCACHE_KEY_VALID_MASK) {
            if (*keyp & SH4_OCACHE_KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = sh4_ocache_write_back(ocache, mem, line_idx);
                if (err)
                    return err;
                err = sh4_ocache_load(ocache, mem, line_idx, paddr);
                memcpy(line + byte_idx, data, sizeof(buf_t));
                *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                sh4_ocache_load(ocache, mem, line_idx, paddr);
                memcpy(line + byte_idx, data, sizeof(buf_t));
                *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            sh4_ocache_load(ocache, mem, line_idx, paddr);
            memcpy(line + byte_idx, data, sizeof(buf_t));
            *keyp |= SH4_OCACHE_KEY_DIRTY_MASK;
        }
    }

    return 0;
}

template <>
int sh4_ocache_do_cache_write_wt<uint8_t>(Sh4Ocache *ocache, MemoryMap *mem,
                                          uint8_t const *data, addr32_t paddr,
                                          bool index_enable,
                                          bool cache_as_ram) {
    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        uint8_t *ptr  = (uint8_t*)sh4_ocache_get_ram_addr(ocache, paddr,
                                                          index_enable);
        *ptr = *data;
        return 0;
    }

    int err = 0;

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;
    unsigned byte_idx = paddr & 0x1f;
    sh4_ocache_key_t *keyp = line_idx + ocache->op_cache_keys;

    if (sh4_ocache_check(ocache, line_idx, paddr) &&
        (*keyp & SH4_OCACHE_KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        line[byte_idx] = *data;
        if ((err = mem->write(data, paddr & 0x1fffffff, sizeof(*data))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(data, paddr & 0x1fffffff, sizeof(*data))) != 0)
            return err;
    }

    return 0;
}

template<typename buf_t>
static int sh4_ocache_do_cache_write_wt(Sh4Ocache *ocache, MemoryMap *mem,
                                        buf_t const *data, addr32_t paddr,
                                        bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & (sizeof(buf_t) - 1)) {
        /*
         * the lazy implementation: do sizeof(buf_t) 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (unsigned i = 0; i < sizeof(buf_t); i++) {
            buf_t *data_nbit = (buf_t*)data;
            int err;
            buf_t mask = buf_t(0xff) << (i * 8);

            uint8_t tmp = uint8_t((mask & *data_nbit) >> (i * 8));
            err = sh4_ocache_do_cache_write_wt<uint8_t>(ocache, mem, &tmp,
                                                        paddr + i, index_enable,
                                                        cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    if (cache_as_ram && Sh4::in_oc_ram_area(paddr)) {
        buf_t *ptr = (buf_t*)sh4_ocache_get_ram_addr(ocache, paddr,
                                                     index_enable);
        *ptr = *data;
        return 0;
    }

    addr32_t line_idx = sh4_ocache_selector(paddr, index_enable, cache_as_ram);
    uint8_t *line = line_idx * SH4_OCACHE_LINE_SIZE + ocache->op_cache;
    unsigned byte_idx = paddr & 0x1f;
    sh4_ocache_key_t *keyp = line_idx + ocache->op_cache_keys;

    if (sh4_ocache_check(ocache, line_idx, paddr) &&
        (*keyp & SH4_OCACHE_KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        memcpy(line + byte_idx, data, sizeof(buf_t));
        if ((err = mem->write(data, paddr & 0x1fffffff, sizeof(buf_t))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(data, paddr & 0x1fffffff, sizeof(buf_t))) != 0)
            return err;
    }

    return 0;
}

int sh4_ocache_read(Sh4Ocache *state, MemoryMap *mem, void *out,
                    unsigned len, addr32_t paddr, bool index_enable,
                    bool cache_as_ram) {
    switch (len) {
    case 1:
        return sh4_ocache_do_cache_read<uint8_t>(state, mem, (uint8_t*)out,
                                                 paddr, index_enable,
                                                 cache_as_ram);
    case 2:
        return sh4_ocache_do_cache_read<uint16_t>(state, mem, (uint16_t*)out,
                                                  paddr, index_enable,
                                                  cache_as_ram);
    case 4:
        return sh4_ocache_do_cache_read<uint32_t>(state, mem, (uint32_t*)out,
                                                  paddr, index_enable,
                                                  cache_as_ram);
    case 8:
        return sh4_ocache_do_cache_read<uint64_t>(state, mem, (uint64_t*)out,
                                                  paddr, index_enable,
                                                  cache_as_ram);
    }

    BOOST_THROW_EXCEPTION(InvalidParamError() << errinfo_param_name("len"));
}

int sh4_ocache_write_cb(Sh4Ocache *state, MemoryMap *mem, void const *data,
                        unsigned len, addr32_t paddr, bool index_enable,
                        bool cache_as_ram) {
    switch (len) {
    case 1:
        return sh4_ocache_do_cache_write_cb<uint8_t>(state, mem, (uint8_t*)data,
                                                     paddr, index_enable,
                                                     cache_as_ram);
    case 2:
        return sh4_ocache_do_cache_write_cb<uint16_t>(state, mem,
                                                      (uint16_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    case 4:
        return sh4_ocache_do_cache_write_cb<uint32_t>(state, mem,
                                                      (uint32_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    case 8:
        return sh4_ocache_do_cache_write_cb<uint64_t>(state, mem,
                                                      (uint64_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    }

    BOOST_THROW_EXCEPTION(InvalidParamError() << errinfo_param_name("len"));
}

int sh4_ocache_write_wt(Sh4Ocache *state, MemoryMap *mem, void const *data,
                        unsigned len, addr32_t paddr, bool index_enable,
                        bool cache_as_ram) {
    switch (len) {
    case 1:
        return sh4_ocache_do_cache_write_wt<uint8_t>(state, mem,
                                                     (uint8_t*)data, paddr,
                                                     index_enable,
                                                     cache_as_ram);
    case 2:
        return sh4_ocache_do_cache_write_wt<uint16_t>(state, mem,
                                                      (uint16_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    case 4:
        return sh4_ocache_do_cache_write_wt<uint32_t>(state, mem,
                                                      (uint32_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    case 8:
        return sh4_ocache_do_cache_write_wt<uint64_t>(state, mem,
                                                      (uint64_t*)data, paddr,
                                                      index_enable,
                                                      cache_as_ram);
    }

    BOOST_THROW_EXCEPTION(InvalidParamError() << errinfo_param_name("len"));
}

void sh4_ocache_pref(Sh4Ocache *ocache, MemoryMap *mem, addr32_t paddr,
                     bool index_enable, bool cache_as_ram) {
    if (!(cache_as_ram && Sh4::in_oc_ram_area(paddr))) {
        addr32_t line_idx = sh4_ocache_selector(paddr, index_enable,
                                                cache_as_ram);

        sh4_ocache_load(ocache, mem, line_idx, paddr);
    }
}
