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

#ifndef ENABLE_SH4_OCACHE
#error this file cannot be built with the sh4 operand cache disabled!
#endif

#include <cstring>

#include "sh4.hpp"
#include "BaseException.hpp"

#include "Ocache.hpp"

Ocache::Ocache(Sh4 *sh4, Memory *mem) {
    this->sh4 = sh4;
    this->mem = mem;

    op_cache = new cache_line[ENTRY_COUNT];
    reset();
}

Ocache::~Ocache() {
    delete[] op_cache;
}

void Ocache::reset(void) {
    memset(op_cache, 0, sizeof(struct cache_line) * ENTRY_COUNT);
}

bool Ocache::cache_check(struct cache_line const *line,
                         addr32_t paddr) {
    addr32_t paddr_tag;

    // upper 19 bits (of the lower 29 bits) of paddr
    paddr_tag = tag_from_paddr(paddr);

    addr32_t line_tag = cache_line_get_tag(line);
    if (line_tag == paddr_tag)
        return line;
    return NULL;
}

addr32_t Ocache::cache_selector(addr32_t paddr, bool index_enable,
                                bool cache_as_ram) const {
    if (cache_as_ram) {
        // the hardware manual is a little vague on how this effects
        // the half of the cache which is not being used as memory.
        throw UnimplementedError("Operand Cache as RAM");
    }

    addr32_t ent_sel = (paddr & 0x1fe0) >> 5;
    if (index_enable)
        ent_sel |= (paddr & (1 << 25)) >> 12;
    else
        ent_sel |= (paddr & (1 << 13)) >> 5;

    return ent_sel;
}

template<>
int Ocache::do_cache_read<1>(basic_val_t *out, addr32_t paddr,
                             bool index_enable, bool cache_as_ram) {
    int err = 0;

    addr32_t line_idx = Ocache::cache_selector(paddr, index_enable,
                                               cache_as_ram);
    struct cache_line *line = line_idx + op_cache;

    if (line->key & KEY_VALID_MASK) {
        if (cache_check(line, paddr)) {
            // cache hit
            addr32_t idx = paddr & 0x1f;

            *out = line->byte[idx];
            return 0;
        } else {
            // tag does not match, V bit is 1
            if (line->key & KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = cache_write_back(line);
                if (err)
                    return err;
                err = cache_load(line, paddr);
            } else {
                //cache miss (no write-back)
                err = cache_load(line, paddr);
            }
        }
    } else {
        // valid bit is 0, tag may or may not match
        // cache miss (no write-back)
        err = cache_load(line, paddr);
    }

    if (!err) {
        addr32_t idx = paddr & 0x1f;

        *out = line->byte[idx];
        return 0;
    }

    return err;
}

template <int N_BYTES>
int Ocache::do_cache_read(basic_val_t *out, addr32_t paddr, bool index_enable,
                          bool cache_as_ram) {
    int err = 0;

    if (paddr & (N_BYTES - 1)) {
        /*
         * the lazy implementation: do N_BYTES 1-byte reads.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        basic_val_t out_buf = 0;
        for (int i = 0; i < N_BYTES; i++) {
            basic_val_t tmp;
            int err;

            err = do_cache_read<1>(&tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;

            out_buf |= tmp << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    addr32_t line_idx = Ocache::cache_selector(paddr, index_enable,
                                               cache_as_ram);
    struct cache_line *line = line_idx + op_cache;

    if (line->key & KEY_VALID_MASK) {
        if (cache_check(line, paddr)) {
            // cache hit
            addr32_t byte_offset = paddr & 0x1f;

            *out = 0;
            memcpy(out, line->byte + byte_offset, N_BYTES);
            return 0;
        } else {
            // tag does not match, V bit is 1
            if (line->key & KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = cache_write_back(line);
                if (err)
                    return err;
                err = cache_load(line, paddr);
            } else {
                //cache miss (no write-back)
                err = cache_load(line, paddr);
            }
        }
    } else {
        // valid bit is 0, tag may or may not match
        // cache miss (no write-back)
        err = cache_load(line, paddr);
    }

    if (!err) {
        addr32_t byte_offset = paddr & 0x1f;

        *out = 0;
        memcpy(out, line->byte + byte_offset, N_BYTES);
        return 0;
    }

    return err;
}


template<>
int Ocache::do_cache_write_cb<1>(basic_val_t data, addr32_t paddr,
                                 bool index_enable, bool cache_as_ram) {
    int err = 0;
    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (cache_check(line, paddr)) {
        if (line->key & KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            line->byte[byte_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->byte[byte_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        }
    } else {
        if (line->key & KEY_VALID_MASK) {
            if (line->key & KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = cache_write_back(line);
                if (err)
                    return err;
                err = cache_load(line, paddr);
                line->byte[byte_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                cache_load(line, paddr);
                line->byte[byte_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->byte[byte_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        }
    }

    return 0;
}

template<int N_BYTES>
int Ocache::do_cache_write_cb(basic_val_t data, addr32_t paddr,
                              bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & (N_BYTES - 1)) {
        /*
         * the lazy implementation: do 2 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < N_BYTES; i++) {
            basic_val_t tmp;
            int err;
            basic_val_t mask = basic_val_t(0xff) << (i * 8);

            tmp = (mask & data) >> (i * 8);
            err = do_cache_write_cb<1>(tmp, paddr + i, index_enable,
                                       cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (cache_check(line, paddr)) {
        if (line->key & KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            memcpy(line->byte + byte_idx, &data, N_BYTES);
            line->key |= KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            memcpy(line->byte + byte_idx, &data, N_BYTES);
            line->key |= KEY_DIRTY_MASK;
        }
    } else {
        if (line->key & KEY_VALID_MASK) {
            if (line->key & KEY_DIRTY_MASK) {
                // cache miss (with write-back)
                // The manual says the SH4 should save the cache line to the
                // write-back buffer.  Since memory writes are effectively
                // instant for the emulator and since I *think* the write-back
                // buffer is invisible from the software's perspective, I don't
                // implement that.
                err = cache_write_back(line);
                if (err)
                    return err;
                err = cache_load(line, paddr);
                memcpy(line->byte + byte_idx, &data, N_BYTES);
                line->key |= KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                cache_load(line, paddr);
                memcpy(line->byte + byte_idx, &data, N_BYTES);
                line->key |= KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            memcpy(line->byte + byte_idx, &data, N_BYTES);
            line->key |= KEY_DIRTY_MASK;
        }
    }

    return 0;
}

template <>
int Ocache::do_cache_write_wt<1>(basic_val_t data, addr32_t paddr,
                                 bool index_enable, bool cache_as_ram) {
    int err = 0;

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (cache_check(line, paddr) && (line->key & KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        line->byte[byte_idx] = data;
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    }

    return 0;
}

template<int N_BYTES>
int Ocache::do_cache_write_wt(basic_val_t data, addr32_t paddr,
                              bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & (N_BYTES - 1)) {
        /*
         * the lazy implementation: do N_BYTES 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < N_BYTES; i++) {
            basic_val_t tmp;
            int err;
            basic_val_t mask = basic_val_t(0xff) << (i * 8);

            tmp = (mask & data) >> (i * 8);
            err = do_cache_write_wt<1>(tmp, paddr + i, index_enable,
                                       cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned byte_idx = paddr & 0x1f;

    if (cache_check(line, paddr) && (line->key & KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        memcpy(line->byte + byte_idx, &data, N_BYTES);
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    }

    return 0;
}

int Ocache::cache_read(basic_val_t *out, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return do_cache_read<1>(out, paddr, index_enable,
                                cache_as_ram);
    case 2:
        return do_cache_read<2>(out, paddr, index_enable,
                                cache_as_ram);
    case 4:
        return do_cache_read<4>(out, paddr, index_enable,
                                cache_as_ram);
    case 8:
        return do_cache_read<8>(out, paddr, index_enable,
                                cache_as_ram);
    }

    throw InvalidParamError("Ocache::cache_read: trying to read a length other "
                            "than 1, 2, 4 or 8");
}

int Ocache::cache_write_cb(basic_val_t data, unsigned len, addr32_t paddr,
                           bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return do_cache_write_cb<1>(data, paddr, index_enable, cache_as_ram);
    case 2:
        return do_cache_write_cb<2>(data, paddr, index_enable, cache_as_ram);
    case 4:
        return do_cache_write_cb<4>(data, paddr, index_enable, cache_as_ram);
    case 8:
        return do_cache_write_cb<8>(data, paddr, index_enable, cache_as_ram);
    }

    throw InvalidParamError("Ocache::cache_write_cb: trying to read a length "
                            "other than 1, 2, 4 or 8");
}

int Ocache::cache_write_wt(basic_val_t data, unsigned len, addr32_t paddr,
                           bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return do_cache_write_wt<1>(data, paddr, index_enable, cache_as_ram);
    case 2:
        return do_cache_write_wt<2>(data, paddr, index_enable, cache_as_ram);
    case 4:
        return do_cache_write_wt<4>(data, paddr, index_enable, cache_as_ram);
    case 8:
        return do_cache_write_wt<8>(data, paddr, index_enable, cache_as_ram);
    }

    throw InvalidParamError("Ocache::cache_write_wt: trying to read a length "
                            "other than 1, 2, 4 or 8");
}

int Ocache::cache_load(struct cache_line *line, addr32_t paddr) {
    int err_code;

    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_CACHE_LINE;
    if ((err_code = mem->read(line->lw, paddr & ~31, n_bytes)) != 0)
        return err_code;

    cache_line_set_tag(line, tag_from_paddr(paddr));
    line->key |= KEY_VALID_MASK;
    line->key &= ~KEY_DIRTY_MASK;

    return 0;
}

int Ocache::cache_write_back(struct cache_line *line) {
    int err_code = 0;
    unsigned ent_sel = line - op_cache;
    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_CACHE_LINE;

    addr32_t paddr = ((line->key & KEY_TAG_MASK) >> KEY_TAG_SHIFT) << 10;
    paddr &= 0x7ffff << 10;

    /* bits 12 and 13 are cleared so thar ORA and OIX don't need to be minded.
     * these bits overlap with the tag (bits 28-10), so this should be safe.
     * In the future, a sanity check to make sure these bits match their
     * counterparts in the tag may be warranted.
     */
    paddr |= (ent_sel << 5) & ~0x3000;

    if ((err_code = mem->write(line->lw, paddr & ~31, n_bytes)) != 0)
        return err_code;

    line->key &= ~KEY_DIRTY_MASK;
    return 0;
}
