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

#include <cstring>

#include "sh4.hpp"
#include "BaseException.hpp"

#include "Ocache.hpp"

Ocache::Ocache(Sh4 *sh4, Memory *mem) {
    this->sh4 = sh4;
    this->mem = mem;

    op_cache = new cache_line[ENTRY_COUNT];
    memset(op_cache, 0, sizeof(struct cache_line) * ENTRY_COUNT);
}

Ocache::~Ocache() {
    delete[] op_cache;
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

    addr32_t ent_sel = paddr & 0xff0;
    if (index_enable)
        ent_sel |= (paddr & (1 << 25)) >> 12;
    else
        ent_sel |= paddr & (1 << 13);
    ent_sel >>= 4;

    return ent_sel;
}

int Ocache::cache_read(boost::uint32_t *out, unsigned len, addr32_t paddr,
                       bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return cache_read1(out, paddr, index_enable,
                           cache_as_ram);
    case 2:
        return cache_read2(out, paddr, index_enable,
                           cache_as_ram);
    case 4:
        return cache_read4(out, paddr, index_enable,
                           cache_as_ram);
    }

    throw UnimplementedError("8-byte data reads");
}

int Ocache::cache_read1(boost::uint32_t *out, addr32_t paddr, bool index_enable,
                        bool cache_as_ram) {
    int err = 0;

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
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
                err = cache_write_back(line, paddr);
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

int Ocache::cache_read2(boost::uint32_t *out, addr32_t paddr,
                        bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x1) {
        /*
         * the lazy implementation: do 2 1-byte reads.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        uint32_t out_buf = 0;
        for (int i = 0; i < 2; i++) {
            uint32_t tmp;
            int err;

            err = cache_read1(&tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;

            out_buf |= tmp << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;

    if (line->key & KEY_VALID_MASK) {
        if (cache_check(line, paddr)) {
            // cache hit
            addr32_t idx = (paddr & 0x1f) >> 1;

            *out = line->sw[idx];
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
                err = cache_write_back(line, paddr);
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
        addr32_t idx = (paddr & 0x1f) >> 1;

        *out = line->sw[idx];
        return 0;
    }

    return err;
}

int Ocache::cache_read4(boost::uint32_t *out, addr32_t paddr,
                        bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x3) {
        /*
         * the lazy implementation: do 4 1-byte reads.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        uint32_t out_buf = 0;
        for (int i = 0; i < 4; i++) {
            uint32_t tmp;
            int err;

            err = cache_read1(&tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;

            out_buf |= tmp << (8 * i);
        }

        *out = out_buf;
        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;

    if (line->key & KEY_VALID_MASK) {
        if (cache_check(line, paddr)) {
            // cache hit
            addr32_t idx = (paddr & 0x1f) >> 2;

            *out = line->lw[idx];
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
                err = cache_write_back(line, paddr);
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
        addr32_t idx = (paddr & 0x1f) >> 2;

        *out = line->lw[idx];
        return 0;
    }

    return err;
}

int Ocache::cache_write_cb(boost::uint32_t data, unsigned len, addr32_t paddr,
                           bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return cache_write1_cb(data, paddr, index_enable, cache_as_ram);
    case 2:
        return cache_write2_cb(data, paddr, index_enable, cache_as_ram);
    case 4:
        return cache_write4_cb(data, paddr, index_enable, cache_as_ram);
    }

    throw UnimplementedError("8-byte data reads");
}

int Ocache::cache_write1_cb(boost::uint32_t data, addr32_t paddr,
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
                err = cache_write_back(line, paddr);
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

int Ocache::cache_write2_cb(boost::uint32_t data, addr32_t paddr,
                            bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x1) {
        /*
         * the lazy implementation: do 2 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < 2; i++) {
            uint32_t tmp;
            int err;

            tmp = ((0xff << i) & data) >> i;
            err = cache_write1_cb(tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned sw_idx = (paddr & 0x1f) >> 1;

    if (cache_check(line, paddr)) {
        if (line->key & KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            line->sw[sw_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->sw[sw_idx] = data;
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
                err = cache_write_back(line, paddr);
                if (err)
                    return err;
                err = cache_load(line, paddr);
                line->sw[sw_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                cache_load(line, paddr);
                line->sw[sw_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->sw[sw_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        }
    }

    return 0;
}

int Ocache::cache_write4_cb(boost::uint32_t data, addr32_t paddr,
                            bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x3) {
        /*
         * the lazy implementation: do 4 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < 4; i++) {
            uint32_t tmp;
            int err;

            tmp = ((0xff << i) & data) >> i;
            err = cache_write1_cb(tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned lw_idx = (paddr & 0x1f) >> 2;

    if (cache_check(line, paddr)) {
        if (line->key & KEY_VALID_MASK) {
            // cache hit, valid bit is 1
            line->lw[lw_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->lw[lw_idx] = data;
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
                err = cache_write_back(line, paddr);
                if (err)
                    return err;
                err = cache_load(line, paddr);
                line->lw[lw_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            } else {
                // clean data in cache can be safely overwritten.
                cache_load(line, paddr);
                line->lw[lw_idx] = data;
                line->key |= KEY_DIRTY_MASK;
            }
        } else {
            // overwrite invalid data in cache.
            cache_load(line, paddr);
            line->lw[lw_idx] = data;
            line->key |= KEY_DIRTY_MASK;
        }
    }

    return 0;
}

int Ocache::cache_write_wt(boost::uint32_t data, unsigned len, addr32_t paddr,
                           bool index_enable, bool cache_as_ram) {
    switch (len) {
    case 1:
        return cache_write1_wt(data, paddr, index_enable, cache_as_ram);
    case 2:
        return cache_write1_wt(data, paddr, index_enable, cache_as_ram);
    case 4:
        return cache_write4_wt(data, paddr, index_enable, cache_as_ram);
    }

    throw UnimplementedError("8-byte data reads");
}



int Ocache::cache_write1_wt(boost::uint8_t data, addr32_t paddr,
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

int Ocache::cache_write2_wt(boost::uint32_t data, addr32_t paddr,
                            bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x1) {
        /*
         * the lazy implementation: do 2 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < 2; i++) {
            uint32_t tmp;
            int err;

            tmp = ((0xff << i) & data) >> i;
            err = cache_write1_wt(tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned lw_idx = (paddr & 0x1f) >> 2;

    if (cache_check(line, paddr) && (line->key & KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        line->lw[lw_idx] = data;
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    }

    return 0;
}

int Ocache::cache_write4_wt(boost::uint32_t data, addr32_t paddr,
                            bool index_enable, bool cache_as_ram) {
    int err = 0;

    if (paddr & 0x3) {
        /*
         * the lazy implementation: do 4 1-byte writes.
         * Obviously this is suboptibmal, but for now I'm more concerned with
         * getting things to work than I am with getting things to work well.
         * Also all this caching code will probably go the way of the dinosaurs
         * later when I inevitably decide I don't need to emulate this aspect
         * of the SH4, so it's no big deal if it's slow.
         */
        for (int i = 0; i < 4; i++) {
            uint32_t tmp;
            int err;

            tmp = ((0xff << i) & data) >> i;
            err = cache_write1_wt(tmp, paddr + i, index_enable, cache_as_ram);
            if (err)
                return err;
        }

        return 0;
    }

    addr32_t line_idx = cache_selector(paddr, index_enable, cache_as_ram);
    struct cache_line *line = line_idx + op_cache;
    unsigned lw_idx = (paddr & 0x1f) >> 2;

    if (cache_check(line, paddr) && (line->key & KEY_VALID_MASK)) {
        // write to cache and write-through to main memory
        line->lw[lw_idx] = data;
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    } else {
        // write through to main memory ignoring the cache
        if ((err = mem->write(&data, paddr, sizeof(data))) != 0)
            return err;
    }

    return 0;
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

int Ocache::cache_write_back(struct cache_line *line, addr32_t paddr) {
    int err_code = 0;

    size_t n_bytes = sizeof(boost::uint32_t) * LONGS_PER_CACHE_LINE;
    if ((err_code = mem->write(line->lw, paddr & ~31, n_bytes)) != 0)
        return err_code;

    line->key &= ~KEY_DIRTY_MASK;
    return 0;
}
