/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
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

#include "trace_proxy.h"

static float trace_proxy_readfloat(uint32_t addr, void *ctxt);
static double trace_proxy_readdouble(uint32_t addr, void *ctxt);
static uint32_t trace_proxy_read32(uint32_t addr, void *ctxt);
static uint16_t trace_proxy_read16(uint32_t addr, void *ctxt);
static uint8_t trace_proxy_read8(uint32_t addr, void *ctxt);

static void trace_proxy_writefloat(uint32_t addr, float val, void *ctxt);
static void trace_proxy_writedouble(uint32_t addr, double val, void *ctxt);
static void trace_proxy_write32(uint32_t addr, uint32_t val, void *ctxt);
static void trace_proxy_write16(uint32_t addr, uint16_t val, void *ctxt);
static void trace_proxy_write8(uint32_t addr, uint8_t val, void *ctxt);

static int
trace_proxy_try_readfloat(uint32_t addr, float *val, void *ctxt);
static int
trace_proxy_try_readdouble(uint32_t addr, double *val, void *ctxt);
static int
trace_proxy_try_read32(uint32_t addr, uint32_t *val, void *ctxt);
static int
trace_proxy_try_read16(uint32_t addr, uint16_t *val, void *ctxt);
static int
trace_proxy_try_read8(uint32_t addr, uint8_t *val, void *ctxt);

static int
trace_proxy_try_writefloat(uint32_t addr, float val, void *ctxt);
static int
trace_proxy_try_writedouble(uint32_t addr, double val, void *ctxt);
static int
trace_proxy_try_write32(uint32_t addr, uint32_t val, void *ctxt);
static int
trace_proxy_try_write16(uint32_t addr, uint16_t val, void *ctxt);
static int
trace_proxy_try_write8(uint32_t addr, uint8_t val, void *ctxt);

struct memory_interface trace_proxy_memory_interface = {
    .readfloat = trace_proxy_readfloat,
    .readdouble = trace_proxy_readdouble,
    .read32 = trace_proxy_read32,
    .read16 = trace_proxy_read16,
    .read8 = trace_proxy_read8,

    .writefloat = trace_proxy_writefloat,
    .writedouble = trace_proxy_writedouble,
    .write32 = trace_proxy_write32,
    .write16 = trace_proxy_write16,
    .write8 = trace_proxy_write8,

    .try_readfloat = trace_proxy_try_readfloat,
    .try_readdouble = trace_proxy_try_readdouble,
    .try_read32 = trace_proxy_try_read32,
    .try_read16 = trace_proxy_try_read16,
    .try_read8 = trace_proxy_try_read8,

    .try_writefloat = trace_proxy_try_writefloat,
    .try_writedouble = trace_proxy_try_writedouble,
    .try_write32 = trace_proxy_try_write32,
    .try_write16 = trace_proxy_try_write16,
    .try_write8 = trace_proxy_try_write8
};

void
trace_memory_write(washdc_hostfile outfile, uint32_t addr,
                   unsigned n_bytes, void const *data) {
    uint32_t pkt_tp = 1;
    uint32_t len = n_bytes;
    washdc_hostfile_write(outfile, &pkt_tp, sizeof(pkt_tp));
    washdc_hostfile_write(outfile, &addr, sizeof(addr));
    washdc_hostfile_write(outfile, &len, sizeof(len));
    washdc_hostfile_write(outfile, data, n_bytes);

    char padding = 0;
    while (n_bytes % 4) {
        washdc_hostfile_write(outfile, &padding, sizeof(padding));
        n_bytes++;
    }
}

void
trace_proxy_create(struct trace_proxy *ctxt, washdc_hostfile outfile,
                   uint32_t mask, struct memory_interface const *intf,
                   void *proxied_ctxt) {
    ctxt->mask = mask;
    ctxt->proxied_intf = intf;
    ctxt->proxied_ctxt = proxied_ctxt;
    ctxt->outfile = outfile;
}

static float trace_proxy_readfloat(uint32_t addr, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    return proxy->proxied_intf->readfloat(addr & proxy->mask,
                                          proxy->proxied_ctxt);
}

static double trace_proxy_readdouble(uint32_t addr, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    return proxy->proxied_intf->readdouble(addr & proxy->mask,
                                           proxy->proxied_ctxt);
}

static uint32_t trace_proxy_read32(uint32_t addr, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    return proxy->proxied_intf->read32(addr & proxy->mask,
                                       proxy->proxied_ctxt);
}

static uint16_t trace_proxy_read16(uint32_t addr, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    return proxy->proxied_intf->read16(addr & proxy->mask,
                                       proxy->proxied_ctxt);
}

static uint8_t trace_proxy_read8(uint32_t addr, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    return proxy->proxied_intf->read8(addr & proxy->mask,
                                      proxy->proxied_ctxt);
}

static void trace_proxy_writefloat(uint32_t addr, float val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    return proxy->proxied_intf->writefloat(addr & proxy->mask, val,
                                           proxy->proxied_ctxt);
}

static void trace_proxy_writedouble(uint32_t addr, double val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    return proxy->proxied_intf->writedouble(addr & proxy->mask, val,
                                            proxy->proxied_ctxt);
}

static void trace_proxy_write32(uint32_t addr, uint32_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    return proxy->proxied_intf->write32(addr & proxy->mask, val,
                                        proxy->proxied_ctxt);
}

static void trace_proxy_write16(uint32_t addr, uint16_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    return proxy->proxied_intf->write16(addr & proxy->mask, val,
                                        proxy->proxied_ctxt);
}

static void trace_proxy_write8(uint32_t addr, uint8_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    return proxy->proxied_intf->write8(addr & proxy->mask, val,
                                       proxy->proxied_ctxt);
}

static int
trace_proxy_try_readfloat(uint32_t addr, float *val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    if (proxy->proxied_intf->try_readfloat) {
        return proxy->proxied_intf->try_readfloat(addr & proxy->mask,
                                                  val, proxy->proxied_ctxt);
    } else {
        *val = trace_proxy_readfloat(addr, proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_readdouble(uint32_t addr, double *val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    if (proxy->proxied_intf->try_readdouble) {
        return proxy->proxied_intf->try_readdouble(addr & proxy->mask,
                                                   val, proxy->proxied_ctxt);
    } else {
        *val = trace_proxy_readdouble(addr & proxy->mask,
                                      proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_read32(uint32_t addr, uint32_t *val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    if (proxy->proxied_intf->try_read32) {
        return proxy->proxied_intf->try_read32(addr & proxy->mask, val,
                                               proxy->proxied_ctxt);
    } else {
        *val = trace_proxy_read32(addr & proxy->mask,
                                  proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_read16(uint32_t addr, uint16_t *val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    if (proxy->proxied_intf->try_read16) {
        return proxy->proxied_intf->try_read16(addr & proxy->mask, val,
                                               proxy->proxied_ctxt);
    } else {
        *val = trace_proxy_read16(addr & proxy->mask,
                                  proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_read8(uint32_t addr, uint8_t *val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    if (proxy->proxied_intf->try_read8) {
        return proxy->proxied_intf->try_read8(addr & proxy->mask, val,
                                              proxy->proxied_ctxt);
    } else {
        *val = trace_proxy_read8(addr & proxy->mask,
                                 proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_writefloat(uint32_t addr, float val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    if (proxy->proxied_intf->try_writefloat) {
        return proxy->proxied_intf->try_writefloat(addr & proxy->mask,
                                                   val, proxy->proxied_ctxt);
    } else {
        proxy->proxied_intf->writefloat(addr & proxy->mask, val,
                                        proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_writedouble(uint32_t addr, double val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    if (proxy->proxied_intf->try_writedouble) {
        return proxy->proxied_intf->try_writedouble(addr & proxy->mask,
                                                    val, proxy->proxied_ctxt);
    } else {
        proxy->proxied_intf->writedouble(addr & proxy->mask, val,
                                         proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_write32(uint32_t addr, uint32_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    if (proxy->proxied_intf->try_write32) {
        return proxy->proxied_intf->try_write32(addr, val, proxy->proxied_ctxt);
    } else {
        proxy->proxied_intf->write32(addr, val, proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_write16(uint32_t addr, uint16_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    if (proxy->proxied_intf->try_write16) {
        return proxy->proxied_intf->try_write16(addr & proxy->mask,
                                                val, proxy->proxied_ctxt);
    } else {
        proxy->proxied_intf->write16(addr & proxy->mask, val,
                                     proxy->proxied_ctxt);
        return 0;
    }
}

static int
trace_proxy_try_write8(uint32_t addr, uint8_t val, void *ctxt) {
    struct trace_proxy *proxy = ctxt;
    trace_memory_write(proxy->outfile, addr, sizeof(val), &val);
    if (proxy->proxied_intf->try_write8) {
        return proxy->proxied_intf->try_write8(addr & proxy->mask, val,
                                               proxy->proxied_ctxt);
    } else {
        proxy->proxied_intf->write8(addr & proxy->mask, val,
                                    proxy->proxied_ctxt);
        return 0;
    }
}
