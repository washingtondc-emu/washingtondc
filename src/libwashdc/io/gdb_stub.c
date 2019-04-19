/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "washdc/cpu.h"
#include "washdc/hw/sh4/sh4_reg_idx.h"
#include "washdc/washdc.h"
#include "washdc/fifo.h"
#include "washdc/log.h"
#include "washdc/error.h"
#include "washdc/debugger.h"
#include "io/io_thread.h"

#include "gdb_stub.h"

// uncomment this to log all traffic in/out of the debugger to stdout
// #define GDBSTUB_VERBOSE

#ifdef INVARIANTS
#define GDB_ASSERT(x)                           \
    do {                                        \
        if (!(x))                               \
            RAISE_ERROR(ERROR_INTEGRITY);       \
    } while (0)
#else
#define GDB_ASSERT(x)
#endif

enum gdb_state {
    GDB_STATE_DISABLED,

    GDB_STATE_LISTENING,

    GDB_STATE_NORM
};

struct gdb_stub {
    struct evconnlistener *listener;
    struct bufferevent *bev;

    struct evbuffer *output_buffer;

    // the last unsuccessfully acknowledged packet, or empty if there is none
    struct string unack_packet;

    struct string input_packet;

    bool frontend_supports_swbreak;

    /*
     * the reason we need a lock here even though we use libevent with pthread
     * support is that transmit_pkt first writes to output_buffer and then
     * dumps that output_buffer into the bufferevent.  Since these two
     * operations are not a single atomic unit, there needs to be a lock held
     * during the interim.
     */
    pthread_mutex_t lock;

    pthread_cond_t cond;

    enum gdb_state state;

    /*
     * variable used to transport breakpoint/watchpoint addresses into the
     * io_thread.  This is only used when one of those events is activated.
     */
    addr32_t break_addr;

    // variable used to transport current context into the io_thread.
    enum dbg_context_id dbg_ctx;
};

static struct gdb_stub stub = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .state = GDB_STATE_DISABLED
};

static struct event *gdb_request_listen_event,
    *gdb_inform_break_event, *gdb_inform_softbreak_event,
    *gdb_inform_read_watchpoint_event, *gdb_inform_write_watchpoint_event;

static void
gdb_callback_attach(void *argptr);
static void
gdb_callback_break(enum dbg_context_id ctx, void *arg);
static void
gdb_callback_read_watchpoint(enum dbg_context_id ctx, addr32_t addr, void *arg);
static void
gdb_callback_write_watchpoint(enum dbg_context_id ctx,
                              addr32_t addr, void *arg);
static void
gdb_callback_softbreak(enum dbg_context_id id, cpu_inst_param inst,
                       addr32_t addr, void *arg);
static void gdb_callback_run_once(void *argptr);
static int decode_hex(char ch);

static void craft_packet(struct string *out, struct string const *in);
static void gdb_serialize_regs(struct string *out);
static void deserialize_regs(struct string const *input_str,
                             reg32_t regs[N_REGS]);
static void serialize_data(struct string *out, void const *buf,
                           unsigned buf_len);
static size_t deserialize_data(struct string const *input_str,
                               void *out, size_t max_sz);
static int decode_hex(char ch);
static void do_write(void);
static void extract_packet(struct string *out, struct string const *packet_in);
static int set_reg(reg32_t reg_file[SH4_REGISTER_COUNT],
                   unsigned reg_no, reg32_t reg_val);
static void handle_packet(struct string *pkt);
static void transmit_pkt(struct string const *pkt);
static bool next_packet(struct string *pkt);

static void handle_c_packet(struct string *out, struct string *dat);
static void handle_q_packet(struct string *out, struct string const *dat);
static void handle_g_packet(struct string *out, struct string const *dat);
static void handle_m_packet(struct string *out, struct string const *dat);
static void handle_M_packet(struct string *out, struct string const *dat);
static void handle_s_packet(struct string *out, struct string const *dat);
static void handle_G_packet(struct string *out, struct string const *dat);
static void handle_P_packet(struct string *out, struct string const *dat);
static void handle_D_packet(struct string *out, struct string const *dat);
static void handle_K_packet(struct string *out, struct string const *dat);
static void handle_Z_packet(struct string *out, struct string const *dat);
static void handle_z_packet(struct string *out, struct string const *dat);

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);
static void handle_events(struct bufferevent *bev, short events, void *arg);
static void handle_read(struct bufferevent *bev, void *arg);

static void gdb_stub_lock(void);
static void gdb_stub_unlock(void);

/*
 * gdb_stub_wait should only be called from the emulation thread and
 * gdb_stub_signal should only be called from the io thread
 */
static void gdb_stub_wait(void);
static void gdb_stub_signal(void);

static void gdb_state_transition(enum gdb_state new_state);

/*
 * libevent callback that gets called when the emulation thread
 * would like to listen for incoming connections.
 */
static void on_request_listen_event(evutil_socket_t fd, short ev, void *arg);

// libevent callback for when the debugger backend hits a hardware breakpoint
static void on_break_event(evutil_socket_t fd, short ev, void *arg);

// libevent callback for when the debugger backend hits a software breakpoint
static void on_softbreak_event(evutil_socket_t fd, short ev, void *arg);

//libevent callback for when the debugger backend hits a read-watchpoint
static void on_read_watchpoint_event(evutil_socket_t fd, short ev, void *arg);

//libevent callback for when the debugger backend hits a write-watchpoint
static void on_write_watchpoint_event(evutil_socket_t fd, short ev, void *arg);

/*
 * the following functions make use of the deferred command infrastructure (see
 * the bottom of this file) to call the corresponding debug_* functions from
 * within the emulation thread.  The point of this is that I want all access to
 * the emulation state to be thread-safe but I also don't want to adapt the
 * emulation code to suit the debugger.
 */
static int gdb_stub_read_mem(void *out, addr32_t addr, unsigned len);
static int gdb_stub_write_mem(void const *input, addr32_t addr, unsigned len);
static int gdb_stub_add_break(addr32_t addr);
static int gdb_stub_remove_break(addr32_t addr);
static int gdb_stub_add_write_watchpoint(addr32_t addr, unsigned len);
static int gdb_stub_remove_write_watchpoint(addr32_t addr, unsigned len);
static int gdb_stub_add_read_watchpoint(addr32_t addr, unsigned len);
static int gdb_stub_remove_read_watchpoint(addr32_t addr, unsigned len);

/*
 * drain the deferred cmd queue.  This should only be called from the emulation
 * thread
 */
static void deferred_cmd_run(void);

struct debug_frontend const gdb_frontend = {
    .attach = gdb_callback_attach,
    .on_break = gdb_callback_break,
    .on_read_watchpoint = gdb_callback_read_watchpoint,
    .on_write_watchpoint = gdb_callback_write_watchpoint,
    .on_softbreak = gdb_callback_softbreak,
    .run_once = gdb_callback_run_once
};

static size_t deserialize_data(struct string const *input_str,
                               void *out, size_t max_sz) {
    uint8_t *out8 = (uint8_t*)out;
    size_t bytes_written = 0;
    char ch;
    // char const eof = EOF;
    char const *input = string_get(input_str);

    while ((ch = *input++)/* && (ch != eof)*/) {
        if (bytes_written >= max_sz)
            return max_sz;

        *out8 = (uint8_t)decode_hex(ch);
        bytes_written++;

        if ((ch = *input++)) {
            *out8 <<= 4;
            *out8 |= (uint8_t)decode_hex(ch);
        } else {
            break;
        }

        out8++;
    }

    return bytes_written;
}

void gdb_init(void) {
    gdb_request_listen_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                         on_request_listen_event, NULL);
    gdb_inform_break_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                       on_break_event, NULL);
    gdb_inform_softbreak_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                           on_softbreak_event, NULL);
    gdb_inform_read_watchpoint_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                                 on_read_watchpoint_event, NULL);
    gdb_inform_write_watchpoint_event = event_new(io_thread_event_base, -1, EV_PERSIST,
                                                  on_write_watchpoint_event, NULL);

    string_init(&stub.unack_packet);
    string_init(&stub.input_packet);

    stub.frontend_supports_swbreak = false;
    stub.listener = NULL;
    stub.bev = NULL;
    stub.state = GDB_STATE_DISABLED;

    stub.output_buffer = evbuffer_new();
    if (!stub.output_buffer)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
}

void gdb_cleanup(void) {
    if (stub.bev) {
        /*
         * TODO - HACK
         * Some versions of gdb will hang after sending the K packet
         * unless there's a delay.  I'm pretty sure this is a bug in gdb itself
         * because not all versions have this bug, and also because the
         * documentation says that immediately closing the connection without
         * even sending an acknowledgement is a perfectly valid way to handle a
         * K packet.
         *
         * As for why the delay fixes things, I *think* it has something to do
         * with forcing gdb to send the packet again because it thinks we never
         * received the first one.
         *
         * I've seen this in Ubuntu's version of gdb-7.11, but gentoo's version
         * of 7.10 is clear.  I have no idea if this is a result of some
         * patch in the Ubuntu version or if I've actually stumbled across a
         * bug upstream.  I Need to remember to go back and try it out on some
         * more builds this weekend.
         *
         * ANYWAYS,  the 10 second delay is annoying but not nearly as annoying
         * as needing to killall gdb every time I quit.
         */
        washdc_log_info("Artificial 10-second delay to work around a bug present in "
                        "some gdb installations, please be patient...\n");
        sleep(10);
        bufferevent_free(stub.bev);
    }

    if (stub.listener)
        evconnlistener_free(stub.listener);

    string_cleanup(&stub.input_packet);
    string_cleanup(&stub.unack_packet);

    event_free(gdb_inform_write_watchpoint_event);
    event_free(gdb_inform_read_watchpoint_event);
    event_free(gdb_inform_softbreak_event);
    event_free(gdb_inform_break_event);
    event_free(gdb_request_listen_event);
}

static void gdb_callback_attach(void *argptr) {
    washdc_log_info("Awaiting remote GDB connection on port %d...\n",
                    GDB_PORT_NO);

    gdb_stub_lock();

    event_active(gdb_request_listen_event, 0, 0);

    gdb_stub_wait();

    // TODO: maybe verify that there was a successful connection here.

    gdb_stub_unlock();

    washdc_log_info("Connection established.\n");
}

static void gdb_callback_run_once(void *argptr) {
    deferred_cmd_run();
}

static void gdb_callback_break(enum dbg_context_id ctx, void *arg) {
    gdb_stub_lock();
    stub.dbg_ctx = ctx;
    gdb_stub_unlock();

    event_active(gdb_inform_break_event, 0, 0);
}

static void gdb_callback_softbreak(enum dbg_context_id ctx, cpu_inst_param inst,
                                   addr32_t addr, void *arg) {
    gdb_stub_lock();
    stub.break_addr = addr;
    stub.dbg_ctx = ctx;
    gdb_stub_unlock();

    event_active(gdb_inform_softbreak_event, 0, 0);
}

static void
gdb_callback_read_watchpoint(enum dbg_context_id ctx,
                             addr32_t addr, void *arg) {
    gdb_stub_lock();
    stub.break_addr = addr;
    stub.dbg_ctx = ctx;
    gdb_stub_unlock();

    event_active(gdb_inform_read_watchpoint_event, 0, 0);
}

static void
gdb_callback_write_watchpoint(enum dbg_context_id ctx,
                              addr32_t addr, void *arg) {
    gdb_stub_lock();
    stub.break_addr = addr;
    stub.dbg_ctx = ctx;
    gdb_stub_unlock();

    event_active(gdb_inform_write_watchpoint_event, 0, 0);
}

static void gdb_serialize_regs(struct string *out) {
    reg32_t reg_file[SH4_REGISTER_COUNT];
    debug_get_all_regs(DEBUG_CONTEXT_SH4, reg_file, sizeof(reg_file));
    reg32_t regs[N_REGS] = { 0 };

    // general-purpose registers
    for (int i = 0; i < 16; i++)
        regs[R0 + i] = reg_file[debug_gen_reg_idx(DEBUG_CONTEXT_SH4, i)];

    // banked registers
    for (int i = 0; i < 8; i++) {
        regs[R0B0 + i] = reg_file[debug_bank0_reg_idx(DEBUG_CONTEXT_SH4,
                                                      reg_file[SH4_REG_SR], i)];
        regs[R0B1 + i] = reg_file[debug_bank1_reg_idx(DEBUG_CONTEXT_SH4,
                                                      reg_file[SH4_REG_SR], i)];
    }

    // FPU registers
    // TODO: implement the other types of FPU registers
    // AFAIK, GDB only supports FRn, DRn and fVn.
    for (int i = 0; i < 16; i++)
        memcpy(regs + FR0 + i, reg_file + SH4_REG_FR0 + i, sizeof(float));

    // system/control registers
    regs[PC] = reg_file[SH4_REG_PC];
    regs[PR] = reg_file[SH4_REG_PR];
    regs[GBR] = reg_file[SH4_REG_GBR];
    regs[VBR] = reg_file[SH4_REG_VBR];
    regs[MACH] = reg_file[SH4_REG_MACH];
    regs[MACL] = reg_file[SH4_REG_MACL];
    regs[SR] = reg_file[SH4_REG_SR];
    regs[SSR] = reg_file[SH4_REG_SSR];
    regs[SPC] = reg_file[SH4_REG_SPC];

    // FPU system/control registers
    regs[FPUL] = reg_file[SH4_REG_FPUL];
    regs[FPSCR] = reg_file[SH4_REG_FPSCR];

    serialize_data(out, regs, sizeof(regs));
}

static void deserialize_regs(struct string const *input_str, reg32_t regs[N_REGS]) {
    size_t sz_expect = N_REGS * sizeof(*regs);

    size_t sz_actual = deserialize_data(input_str, regs, sz_expect);

    if (sz_expect != sz_actual) {
        // TODO: better error messages
        washdc_log_error("sz_expect is %u, az_actual is %u\n",
                         (unsigned)sz_expect, (unsigned)sz_actual);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}

static void serialize_data(struct string *out, void const *buf,
                           unsigned buf_len) {
    uint8_t const *buf8 = (uint8_t const*)buf;
    static const char hex_tbl[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    for (unsigned i = 0; i < buf_len; i++) {
        string_append_char(out, hex_tbl[(*buf8) >> 4]);
        string_append_char(out, hex_tbl[(*buf8) & 0xf]);
        buf8++;
    }
}

static int decode_hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 10;
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0';
    if ((ch >= 'A') && (ch <= 'F'))
        return ch - 'A' + 10;
    return -1;
}

static void transmit(struct string const *data) {
    int len = string_length(data);
    if (len > 0) {
        char const *data_c_str = string_get(data);

        if (evbuffer_add(stub.output_buffer, data_c_str,
                         sizeof(char) * len) < 0) {
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        }

        do_write();
    }
}

static void do_write(void) {
    bufferevent_write_buffer(stub.bev, stub.output_buffer);
}

static void craft_packet(struct string *out, struct string const *in) {
    uint8_t csum = 0;
    static const char hex_tbl[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    char const *str_ptr = string_get(in);
    while (*str_ptr) {
        csum += (uint8_t)*str_ptr;
        str_ptr++;
    }

    string_append(out, "$");
    string_append(out, string_get(in));
    string_append(out, "#");
    string_append_char(out, hex_tbl[csum >> 4]);
    string_append_char(out, hex_tbl[csum & 0xf]);
}

static void extract_packet(struct string *out, struct string const *packet_in) {
    int dollar_idx = string_find_first_of(packet_in, "$");
    int pound_idx = string_find_last_of(packet_in, "#");

    if (dollar_idx < 0 || pound_idx < 0)
        return;

    string_substr(out, packet_in, dollar_idx + 1, pound_idx - 1);
}

static int conv_reg_idx_to_sh4(unsigned reg_no, reg32_t reg_sr) {
    if (reg_no >= R0 && reg_no <= R15)
        return debug_gen_reg_idx(DEBUG_CONTEXT_SH4, reg_no - R0);
    else if (reg_no >= R0B0 && reg_no <= R7B0)
        return debug_bank0_reg_idx(DEBUG_CONTEXT_SH4, reg_sr, reg_no - R0B0);
    else if (reg_no >= R0B1 && reg_no <= R7B1)
        return debug_bank1_reg_idx(DEBUG_CONTEXT_SH4, reg_sr, reg_no - R0B1);
    else if (reg_no == PC)
        return SH4_REG_PC;
    else if (reg_no == PR)
        return SH4_REG_PR;
    else if (reg_no == GBR)
        return SH4_REG_GBR;
    else if (reg_no == VBR)
        return SH4_REG_VBR;
    else if (reg_no == MACH)
        return SH4_REG_MACH;
    else if (reg_no == MACL)
        return SH4_REG_MACL;
    else if (reg_no == SR)
        return SH4_REG_SR;
    else if (reg_no == SSR)
        return SH4_REG_SSR;
    else if (reg_no == SPC)
        return SH4_REG_SPC;
    else if (reg_no == FPUL)
        return SH4_REG_FPUL;
    else if (reg_no == FPSCR)
        return SH4_REG_FPSCR;
    else if (reg_no >= FR0 && reg_no <= FR15)
        return reg_no - FR0 + SH4_REG_FR0;

    washdc_log_warn("Error: unable to map register index %d\n", reg_no);
    return -1;
}

static int set_reg(reg32_t reg_file[SH4_REGISTER_COUNT],
                   unsigned reg_no, reg32_t reg_val) {

    if ((reg_no >= R0B0 && reg_no <= R7B0) ||
        (reg_no >= R0B1 && reg_no <= R7B1)) {
        washdc_log_warn("WARNING: this gdb stub does not allow writes to "
                        "banked registers\n");
        return 0;
    }

    int idx = conv_reg_idx_to_sh4(reg_no, reg_file[SH4_REG_SR]);

    if (idx >= 0) {
        reg_file[idx] = reg_val;
    } else {
#ifdef GDBSTUB_VERBOSE
        washdc_log_warn("WARNING: GdbStub unable to set value of register %x "
                        "to %x\n", reg_no, reg_val);
#endif
        return 1;
    }

    return 0;
}

static void err_str(struct string *err_out, unsigned err_val) {
    static char const hex_chars[] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    string_append_char(err_out, 'E');

    // dont print more than 2 digits
    err_val &= 0xff;
    string_append_char(err_out, hex_chars[err_val >> 4]);
    err_val &= 0x0f;
    string_append_char(err_out, hex_chars[err_val]);
}

static void handle_c_packet(struct string *out, struct string *dat) {
    debug_request_continue();
}

static void handle_q_packet(struct string *out, struct string const *dat_orig) {
    struct string dat, tok;
    string_init(&dat);
    string_init(&tok);
    string_copy(&dat, dat_orig);

    if (string_eq_n(&dat, "qSupported", 10)) {
        int semicolon_idx = string_find_first_of(&dat, ";");

        if (semicolon_idx == -1)
            goto cleanup;

        struct string tmp;
        string_init(&tmp);
        string_substr(&tmp, &dat, semicolon_idx + 1, string_length(&dat) - 1);
        string_cleanup(&dat);
        memcpy(&dat, &tmp, sizeof(dat));

        struct string_curs curs;
        string_tok_begin(&curs);
        while (string_tok_next(&tok, &curs, string_get(&dat), ";")) {
            bool supported = false;

            int plus_or_minus_idx = string_find_last_of(&tok, "+-");

            /*
             * ignore all the settings that try to set variables,
             * we're really only here for swbreak.
             */
            if (plus_or_minus_idx >= 0) {
                if (string_get(&tok)[plus_or_minus_idx] == '+')
                    supported = true;

                struct string tmp;
                string_init(&tmp);
                string_substr(&tmp, &tok, 0, plus_or_minus_idx - 1);
                string_cleanup(&tok);
                memcpy(&tok, &tmp, sizeof(tok));
            }

            if (strcmp(string_get(&tok), "swbreak") == 0) {
                if (supported) {
                    stub.frontend_supports_swbreak = true;
                    string_append(out, "swbreak+;");
                } else {
                    string_append(out, "swbreak-;");
                }
            } else {
                string_append(out, string_get(&tok));
                string_append(out, "-;");
            }
        }

        goto cleanup;
    }

cleanup:
    string_cleanup(&tok);
    string_cleanup(&dat);
}

static void handle_g_packet(struct string *out, struct string const *dat) {
    gdb_serialize_regs(out);
}

static void handle_m_packet(struct string *out, struct string const *dat) {
    int addr_idx = string_find_last_of(dat, "m");
    int comma_idx = string_find_last_of(dat, ",");
    int len_idx = comma_idx;

    if (addr_idx < 0 || comma_idx < 0 || len_idx < 0) {
        err_str(out, EINVAL);
        return;
    }

    addr_idx++;
    len_idx++;

    struct string len_str, addr_str;
    string_init(&len_str);
    string_init(&addr_str);

    string_substr(&len_str, dat, len_idx, string_length(dat) - 1);
    string_substr(&addr_str, dat, addr_idx, comma_idx - 1);

    uint32_t len = string_read_hex32(&len_str, 0);
    uint32_t addr = string_read_hex32(&addr_str, 0);

    string_cleanup(&addr_str);
    string_cleanup(&len_str);

    void *data_buf = malloc(len);
    if (!data_buf) {
        error_set_length(len);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    if (gdb_stub_read_mem(data_buf, addr, len) < 0) {
        err_str(out, EINVAL);
        goto cleanup;
    }
    serialize_data(out, data_buf, len);

cleanup:
    free(data_buf);
}

static void handle_M_packet(struct string *out, struct string const *dat) {
    int addr_idx = string_find_last_of(dat, "M");
    int comma_idx = string_find_last_of(dat, ",");
    int colon_idx = string_find_last_of(dat, ":");

    if (addr_idx < 0 || comma_idx < 0 || colon_idx < 0) {
        err_str(out, EINVAL);
        return;
    }

    int len_idx = comma_idx + 1;
    int dat_idx = colon_idx + 1;
    addr_idx++;

    struct string addr_substr;
    struct string len_substr;
    string_init(&addr_substr);
    string_init(&len_substr);
    string_substr(&addr_substr, dat, addr_idx, comma_idx - 1);
    string_substr(&len_substr, dat, len_idx, colon_idx - 1);

    uint32_t addr = string_read_hex32(&addr_substr, 0);
    uint32_t len = string_read_hex32(&len_substr, 0);

    string_cleanup(&addr_substr);
    string_cleanup(&len_substr);

    struct string new_dat;
    string_init(&new_dat);
    string_substr(&new_dat, dat, dat_idx, string_length(dat) - 1);
    if (len < 1024) {
        uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t) * len);
        deserialize_data(&new_dat, buf, len);
        int err = gdb_stub_write_mem(buf, addr, len);
        free(buf);

        if (err < 0) {
            err_str(out, EINVAL);
            string_cleanup(&new_dat);
            return;
        }
    } else {
        error_set_length(len);
        RAISE_ERROR(ERROR_INVALID_PARAM);
    }
    string_cleanup(&new_dat);

    string_set(out, "OK");
}

static void handle_s_packet(struct string *out, struct string const *dat) {
    debug_request_single_step();
}

static void handle_G_packet(struct string *out, struct string const *dat) {
    reg32_t regs[N_REGS];

    struct string tmp;
    string_init(&tmp);
    string_substr(&tmp, dat, 1, string_length(dat) - 1);
    deserialize_regs(&tmp, regs);
    string_cleanup(&tmp);

    reg32_t new_regs[SH4_REGISTER_COUNT];
    debug_get_all_regs(DEBUG_CONTEXT_SH4, new_regs, sizeof(new_regs));

    for (unsigned reg_no = 0; reg_no < N_REGS; reg_no++)
        set_reg(new_regs, reg_no, regs[reg_no]);
    debug_set_all_regs(DEBUG_CONTEXT_SH4, new_regs, sizeof(new_regs));

    string_set(out, "OK");
}

static void handle_P_packet(struct string *out, struct string const *dat) {
    int equals_idx = string_find_first_of(dat, "=");

    if ((equals_idx < 0) || ((size_t)equals_idx >= (string_length(dat) - 1))) {
#ifdef GDBSTUB_VERBOSE
        washdc_log_warn("WARNING: malformed P packet in gdbstub \"%s\"\n",
                        string_get(dat));
#endif

        string_set(out, "E16");
        return;
    }

    struct string reg_no_str, reg_val_str;
    string_init(&reg_no_str);
    string_init(&reg_val_str);

    string_substr(&reg_no_str, dat, 1, equals_idx - 1);
    string_substr(&reg_val_str, dat, equals_idx + 1, string_length(dat) - 1);

    unsigned reg_no = 0;
    reg32_t reg_val = 0;
    deserialize_data(&reg_no_str, &reg_no, sizeof(reg_no));
    deserialize_data(&reg_val_str, &reg_val, sizeof(reg_val));

    if (reg_no >= N_REGS) {
#ifdef GDBSTUB_VERBOSE
        washdc_log_error("ERROR: unable to write to register number %x\n",
                        reg_no);
#endif
        string_set(out, "E16");
        goto cleanup;
    }

    if ((reg_no >= R0B0 && reg_no <= R7B0) || (reg_no >= R0B1 && reg_no <= R7B1)) {
        washdc_log_warn("WARNING: this gdb stub does not allow writes to "
                        "banked registers\n");
    } else {
        int sh4_reg_no = conv_reg_idx_to_sh4(reg_no,
                                             debug_get_reg(DEBUG_CONTEXT_SH4,
                                                           SH4_REG_SR));

        if (sh4_reg_no >= 0)
            debug_set_reg(DEBUG_CONTEXT_SH4, sh4_reg_no, reg_val);
    }

    string_set(out, "OK");

cleanup:
    string_cleanup(&reg_val_str);
    string_cleanup(&reg_no_str);
}

static void handle_D_packet(struct string *out, struct string const *dat) {
    debug_request_detach();

    string_set(out, "OK");
}

static void handle_K_packet(struct string *out, struct string const *dat) {
    washdc_kill();

    string_set(out, "OK");
}

static void handle_Z_packet(struct string *out, struct string const *dat) {
    char const *dat_c_str = string_get(dat);
    struct string dat_local;
    string_init(&dat_local);
    string_copy(&dat_local, dat);

    if (dat_c_str[1] == '1') {
        //hardware breakpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string tmp_str;
        string_init(&tmp_str);
        string_substr(&tmp_str, &dat_local,
                      first_comma_idx + 1, last_comma_idx - 1);
        string_cleanup(&dat_local);
        memcpy(&dat_local, &tmp_str, sizeof(dat_local));

        addr32_t break_addr = string_read_hex32(&dat_local, 0);

        int err_code = gdb_stub_add_break(break_addr);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, ENOBUFS);

        goto cleanup;
    } else if (dat_c_str[1] == '2') {
        // write watchpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string len_str, addr_str;
        string_init(&len_str);
        string_init(&addr_str);

        string_substr(&len_str, &dat_local, last_comma_idx + 1,
                      string_length(&dat_local) - 1);
        string_substr(&addr_str, &dat_local,
                      first_comma_idx + 1, last_comma_idx - 1);

        uint32_t length = string_read_hex32(&len_str, 0);
        addr32_t watch_addr = string_read_hex32(&addr_str, 0);;

        string_cleanup(&addr_str);
        string_cleanup(&len_str);

        int err_code = gdb_stub_add_write_watchpoint(watch_addr, length);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, ENOBUFS);

        goto cleanup;
    } else if (dat_c_str[1] == '3') {
        // read watchpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string len_str, addr_str;
        string_init(&len_str);
        string_init(&addr_str);

        string_substr(&len_str, &dat_local,
                      last_comma_idx + 1, string_length(&dat_local) - 1);
        string_substr(&addr_str, &dat_local,
                      first_comma_idx + 1, last_comma_idx - 1);

        uint32_t length = string_read_hex32(&len_str, 0);
        addr32_t watch_addr = string_read_hex32(&addr_str, 0);;

        string_cleanup(&addr_str);
        string_cleanup(&len_str);

        int err_code = gdb_stub_add_read_watchpoint(watch_addr, length);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, ENOBUFS);

        goto cleanup;
    } else {
        // unsupported
        goto cleanup;
    }

cleanup:
    string_cleanup(&dat_local);
}

static void handle_z_packet(struct string *out, struct string const *dat) {
    char const *dat_c_str = string_get(dat);
    struct string dat_local;
    string_init(&dat_local);
    string_copy(&dat_local, dat);

    if (dat_c_str[1] == '1') {
        //hardware breakpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string tmp_str;
        string_init(&tmp_str);
        string_substr(&tmp_str, &dat_local,
                      first_comma_idx + 1, last_comma_idx - 1);
        string_cleanup(&dat_local);
        memcpy(&dat_local, &tmp_str, sizeof(dat_local));

        addr32_t break_addr = string_read_hex32(&dat_local, 0);

        int err_code = gdb_stub_remove_break(break_addr);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, err_code);

        goto cleanup;
    } else if (dat_c_str[1] == '2') {
        // write watchpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string len_str, addr_str;
        string_init(&len_str);
        string_init(&addr_str);

        string_substr(&len_str, &dat_local, last_comma_idx + 1,
                      string_length(&dat_local) - 1);
        string_substr(&addr_str, &dat_local, first_comma_idx + 1, last_comma_idx - 1);

        uint32_t length = string_read_hex32(&len_str, 0);
        addr32_t watch_addr = string_read_hex32(&addr_str, 0);

        string_cleanup(&addr_str);
        string_cleanup(&len_str);

        int err_code = gdb_stub_remove_write_watchpoint(watch_addr, length);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, EINVAL);

        goto cleanup;
    } else if (dat_c_str[1] == '3') {
        // read watchpoint

        if (string_find_first_of(&dat_local, ":") >= 0)
            goto cleanup; // we don't support conditions

        int first_comma_idx = string_find_first_of(&dat_local, ",");
        if ((first_comma_idx < 0) ||
            (first_comma_idx == (int)string_length(&dat_local) - 1)) {
            goto cleanup; // something is wrong and/or unexpected
        }

        int last_comma_idx = string_find_last_of(&dat_local, ",");
        if (last_comma_idx < 0)
            goto cleanup; // something is wrong and/or unexpected

        struct string len_str, addr_str;
        string_init(&len_str);
        string_init(&addr_str);

        string_substr(&len_str, &dat_local, last_comma_idx + 1,
                      string_length(&dat_local) - 1);
        string_substr(&addr_str, &dat_local,
                      first_comma_idx + 1, last_comma_idx - 1);

        uint32_t length = string_read_hex32(&len_str, 0);
        addr32_t watch_addr = string_read_hex32(&addr_str, 0);;

        string_cleanup(&addr_str);
        string_cleanup(&len_str);

        int err_code = gdb_stub_remove_read_watchpoint(watch_addr, length);

        if (err_code == 0)
            string_set(out, "OK");
        else
            err_str(out, EINVAL);

        goto cleanup;
    } else {
        // unsupported
        goto cleanup;
    }

cleanup:
    string_cleanup(&dat_local);
}

static void handle_packet(struct string *pkt) {
    struct string dat;
    struct string response;
    struct string resp_pkt;

    string_init(&dat);
    string_init(&response);
    string_init(&resp_pkt);

    extract_packet(&dat, pkt);

    if (string_length(&dat)) {
        char first_ch = string_get(&dat)[0];
        if (first_ch == 'q') {
            handle_q_packet(&response, &dat);
        } else if (first_ch == 'g') {
            handle_g_packet(&response, &dat);
        } else if (first_ch == 'G') {
            handle_G_packet(&response, &dat);
        } else if (first_ch == 'm') {
            handle_m_packet(&response, &dat);
        } else if (first_ch == 'M') {
            handle_M_packet(&response, &dat);
        } else if (first_ch == '?') {
            string_set(&response, "S05 create:");
        } else if (first_ch == 's') {
            handle_s_packet(&response, &dat);
            goto cleanup;
        } else if (first_ch == 'c') {
            handle_c_packet(&response, &dat);
            goto cleanup;
        } else if (first_ch == 'P') {
            handle_P_packet(&response, &dat);
        } else if (first_ch == 'D') {
            handle_D_packet(&response, &dat);
        } else if (first_ch == 'k') {
            handle_K_packet(&response, &dat);
        } else if (first_ch == 'z') {
            handle_z_packet(&response, &dat);
        } else if (first_ch == 'Z') {
            handle_Z_packet(&response, &dat);
        }
    }

    craft_packet(&resp_pkt, &response);
    transmit_pkt(&resp_pkt);

cleanup:
    string_cleanup(&resp_pkt);
    string_cleanup(&response);
    string_cleanup(&dat);
}

static void transmit_pkt(struct string const *pkt) {
#ifdef GDBSTUB_VERBOSE
    washdc_log_info(">>>> %s\n", string_get(pkt));
#endif

    string_copy(&stub.unack_packet, pkt);
    transmit(pkt);
}

static bool next_packet(struct string *pkt) {
    struct string pktbuf_tmp;
    struct string tmp_str;
    char ch;
    bool found_pkt = false;

    string_init(&pktbuf_tmp);
    string_copy(&pktbuf_tmp, &stub.input_packet);

    // wait around for the start character, ignore all other characters
    do {
        if (!string_length(&pktbuf_tmp))
            goto cleanup;

        ch = string_get(&pktbuf_tmp)[0];

        string_init(&tmp_str);
        string_substr(&tmp_str, &pktbuf_tmp, 1, string_length(&pktbuf_tmp) - 1);
        string_cleanup(&pktbuf_tmp);
        memcpy(&pktbuf_tmp, &tmp_str, sizeof(pktbuf_tmp));

        if (ch == EOF)
            goto cleanup;
    } while(ch != '$');
    string_append_char(pkt, ch);

    // now, read until a # or end of buffer is found
    do {
        if (!string_length(&pktbuf_tmp))
            goto cleanup;

        ch = string_get(&pktbuf_tmp)[0];

        string_init(&tmp_str);
        string_substr(&tmp_str, &pktbuf_tmp, 1, string_length(&pktbuf_tmp) - 1);
        string_cleanup(&pktbuf_tmp);
        memcpy(&pktbuf_tmp, &tmp_str, sizeof(pktbuf_tmp));

        if (ch == EOF)
            goto cleanup;

        string_append_char(pkt, ch);
    } while(ch != '#');
        
    // read the one-byte (two char) checksum
    if (!string_length(&pktbuf_tmp))
            goto cleanup;
    ch = string_get(&pktbuf_tmp)[0];
    if (ch == EOF)
            goto cleanup;
    string_init(&tmp_str);
    string_substr(&tmp_str, &pktbuf_tmp, 1, string_length(&pktbuf_tmp) - 1);
    string_cleanup(&pktbuf_tmp);
    memcpy(&pktbuf_tmp, &tmp_str, sizeof(pktbuf_tmp));
    string_append_char(pkt, ch);

    if (!string_length(&pktbuf_tmp))
            goto cleanup;
    ch = string_get(&pktbuf_tmp)[0];
    if (ch == EOF)
            goto cleanup;
    string_init(&tmp_str);
    string_substr(&tmp_str, &pktbuf_tmp, 1, string_length(&pktbuf_tmp) - 1);
    string_cleanup(&pktbuf_tmp);
    memcpy(&pktbuf_tmp, &tmp_str, sizeof(pktbuf_tmp));
    string_append_char(pkt, ch);

    string_set(&stub.input_packet, string_get(&pktbuf_tmp));

#ifdef GDBSTUB_VERBOSE
    washdc_log_info("<<<< %s\n", string_get(pkt));
#endif

    found_pkt = true;

cleanup:
    string_cleanup(&pktbuf_tmp);
    return found_pkt;
}

/*
 * this function gets called by libevent when a remote gdb stub connects
 */
static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    gdb_stub_lock();

    if (stub.state != GDB_STATE_LISTENING) {
        washdc_log_warn("WARNING: %s called when state is not "
                        "GDB_STATE_LISTENING (state is %u)\n",
                        __func__, (unsigned)stub.state);
        gdb_stub_unlock();
        return;
    }

    stub.bev = bufferevent_socket_new(io_thread_event_base, fd,
                                      BEV_OPT_CLOSE_ON_FREE);
    if (!stub.bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(stub.bev, handle_read,
                      NULL, handle_events, NULL);
    bufferevent_enable(stub.bev, EV_WRITE);
    bufferevent_enable(stub.bev, EV_READ);

    gdb_state_transition(GDB_STATE_NORM);

    gdb_stub_signal();

    // TODO: free stub.listener

    gdb_stub_unlock();
}

static void handle_events(struct bufferevent *bev, short events, void *arg) {
    exit(2);
}

static void handle_read(struct bufferevent *bev, void *arg) {
    struct evbuffer *read_buffer;

    if (!(read_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    gdb_stub_lock();

    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);

    for (unsigned i = 0; i < buflen; i++) {
        uint8_t tmp;
        if (evbuffer_remove(read_buffer, &tmp, sizeof(tmp)) < 0)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        char c = tmp;

        if (string_length(&stub.input_packet)) {

            if (string_length(&stub.unack_packet)) {
                washdc_log_warn("WARNING: new packet incoming; no "
                                "acknowledgement was ever received for "
                                "\"%s\"\n", string_get(&stub.unack_packet));
                string_set(&stub.unack_packet, "");
            }

            string_append_char(&stub.input_packet, c);

            struct string pkt;
            string_init(&pkt);
            bool pkt_valid = next_packet(&pkt);
            if (string_length(&pkt) && pkt_valid) {
                string_set(&stub.input_packet, "");

                // TODO: verify the checksum

#ifdef GDBSTUB_VERBOSE
                washdc_log_info(">>>> +\n");
#endif
                struct string plus_symbol;
                string_init_txt(&plus_symbol, "+");
                transmit(&plus_symbol);
                string_cleanup(&plus_symbol);
                handle_packet(&pkt);
            }

            string_cleanup(&pkt);
        } else {
            if (c == '+') {
#ifdef GDBSTUB_VERBOSE
                washdc_log_info("<<<< +\n");
#endif
                if (!string_length(&stub.unack_packet))
                    washdc_log_warn("WARNING: received acknowledgement for "
                                    "unsent packet\n");
                string_set(&stub.unack_packet, "");
            } else if (c == '-') {
#ifdef GDBSTUB_VERBOSE
                washdc_log_info("<<<< -\n");
#endif
                if (!string_length(&stub.unack_packet)) {
                    washdc_log_warn("WARNING: received negative "
                                    "acknowledgement for unsent packet\n");
                } else {
#ifdef GDBSTUB_VERBOSE
                    washdc_log_info(">>>> %s\n",
                                    string_get(&stub.unack_packet));
#endif
                    transmit(&stub.unack_packet);
                }
            } else if (c == '$') {
                // new packet
                string_set(&stub.input_packet, "$");
            } else if (c == 3) {
                // user pressed ctrl+c (^C) on the gdb frontend
                washdc_log_info("GDBSTUB: user requested breakpoint "
                                "(ctrl-C)\n");
                debug_request_break();
            } else {
                washdc_log_warn("WARNING: ignoring unexpected character %c\n",
                                c);
            }
        }
    }

    gdb_stub_unlock();
}

static void gdb_stub_lock(void) {
    if (pthread_mutex_lock(&stub.lock) < 0)
        abort(); // TODO error handling
}

static void gdb_stub_unlock(void) {
    if (pthread_mutex_unlock(&stub.lock) < 0)
        abort(); // TODO error handling
}

static void gdb_stub_wait(void) {
    if (pthread_cond_wait(&stub.cond, &stub.lock) < 0)
        abort(); // TODO error handling
}

static void gdb_stub_signal(void) {
    if (pthread_cond_signal(&stub.cond) < 0)
        abort(); // TODO error handling
}

static void gdb_state_transition(enum gdb_state new_state) {
    stub.state = new_state;
}

static void on_request_listen_event(evutil_socket_t fd, short ev, void *arg) {
    gdb_stub_lock();

    gdb_state_transition(GDB_STATE_LISTENING);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(GDB_PORT_NO);
    unsigned event_flags = LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE;
    stub.listener = evconnlistener_new_bind(io_thread_event_base, listener_cb, NULL,
                                            event_flags, -1,
                                            (struct sockaddr*)&sin, sizeof(sin));

    if (!stub.listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    gdb_stub_unlock();
}

static void on_break_event(evutil_socket_t fd, short ev, void *arg) {
    struct string resp, pkt;
    string_init(&pkt);
    string_init_txt(&resp, "S05");

    craft_packet(&pkt, &resp);

    gdb_stub_lock();
    transmit_pkt(&pkt);
    gdb_stub_unlock();

    string_cleanup(&resp);
    string_cleanup(&pkt);
}

static void
on_softbreak_event(evutil_socket_t fd, short ev, void *arg) {
    struct string resp, pkt;
    string_init(&pkt);
    string_init(&resp);

    gdb_stub_lock();

    if (stub.frontend_supports_swbreak) {
        string_set(&resp, "T05swbreak:");
        string_append_hex32(&resp, stub.break_addr);
        string_append_char(&resp, ';');
    } else {
        string_set(&resp, "T05swbreak:");
    }

    craft_packet(&pkt, &resp);


    transmit_pkt(&pkt);
    gdb_stub_unlock();

    string_cleanup(&resp);
    string_cleanup(&pkt);
}

static void on_read_watchpoint_event(evutil_socket_t fd, short ev, void *arg) {
    struct string resp, pkt;
    string_init(&pkt);
    string_init_txt(&resp, "S05");

    craft_packet(&pkt, &resp);

    gdb_stub_lock();
    transmit_pkt(&pkt);
    gdb_stub_unlock();

    string_cleanup(&resp);
    string_cleanup(&pkt);
}

static void on_write_watchpoint_event(evutil_socket_t fd, short ev, void *arg) {
    struct string resp, pkt;
    string_init(&pkt);
    string_init_txt(&resp, "S05");

    craft_packet(&pkt, &resp);

    gdb_stub_lock();
    transmit_pkt(&pkt);
    gdb_stub_unlock();

    string_cleanup(&resp);
    string_cleanup(&pkt);
}

/*
 * For functions that read/write to the sh4 or memory, I need to be able to
 * move data from the emulation thread into io_thread.  To do this safely, I
 * can either adapt the emulation code to suit the debugger (such as locking) or
 * I can shift all the responsibility onto the gdb_stub.  The latter is more
 * complicated but it keeps all the synchronization code in one place and it
 * doesn't impact performance when I'm not using the debugger so that's what I
 * went with.
 *
 * The deferred_cmd infra below queues up data buffers in a fifo protected by a
 * mutex so that I only have to poke around at the hardware from within the
 * emulation thread.  The deferred commands are executed within the emulation
 * thread by the gdb_stub's run_once handler.
 */
enum deferred_cmd_type {
    DEFERRED_CMD_GET_ALL_REGS,
    DEFERRED_CMD_SET_ALL_REGS,
    DEFERRED_CMD_SET_REG,
    DEFERRED_CMD_READ_MEM,
    DEFERRED_CMD_WRITE_MEM,

    DEFERRED_CMD_ADD_BREAK,
    DEFERRED_CMD_REMOVE_BREAK,
    DEFERRED_CMD_ADD_WRITE_WATCH,
    DEFERRED_CMD_REMOVE_WRITE_WATCH,
    DEFERRED_CMD_ADD_READ_WATCH,
    DEFERRED_CMD_REMOVE_READ_WATCH,

    N_DEFERRED_CMD
};

char const *deferred_cmd_names[N_DEFERRED_CMD] = {
    "DEFERRED_CMD_GET_ALL_REGS",
    "DEFERRED_CMD_SET_ALL_REGS",
    "DEFERRED_CMD_SET_REG",
    "DEFERRED_CMD_READ_MEM",
    "DEFERRED_CMD_WRITE_MEM",

    "DEFERRED_CMD_ADD_BREAK",
    "DEFERRED_CMD_REMOVE_BREAK",
    "DEFERRED_CMD_ADD_WRITE_WATCH",
    "DEFERRED_CMD_REMOVE_WRITE_WATCH",
    "DEFERRED_CMD_ADD_READ_WATCH",
    "DEFERRED_CMD_REMOVE_READ_WATCH"
};

struct meta_deferred_cmd_get_all_regs {
    reg32_t *reg_file_out;
};

struct meta_deferred_cmd_set_all_regs {
    reg32_t const *reg_file_in;
};

struct meta_deferred_cmd_set_reg {
    unsigned idx;
    reg32_t val;
};

struct meta_deferred_cmd_read_mem {
    void *out_buf;
    unsigned len;
    addr32_t addr;
};

struct meta_deferred_cmd_write_mem {
    void const *in_buf;
    unsigned len;
    addr32_t addr;
};

struct meta_deferred_cmd_add_break {
    addr32_t addr;
};

struct meta_deferred_cmd_remove_break {
    addr32_t addr;
};

struct meta_deferred_cmd_add_write_watch {
    addr32_t addr;
    unsigned len;
};

struct meta_deferred_cmd_remove_write_watch {
    addr32_t addr;
    unsigned len;
};

struct meta_deferred_cmd_add_read_watch {
    addr32_t addr;
    unsigned len;
};

struct meta_deferred_cmd_remove_read_watch {
    addr32_t addr;
    unsigned len;
};

union deferred_cmd_meta {
    struct meta_deferred_cmd_get_all_regs get_all_regs;
    struct meta_deferred_cmd_set_all_regs set_all_regs;
    struct meta_deferred_cmd_set_reg set_reg;
    struct meta_deferred_cmd_read_mem read_mem;
    struct meta_deferred_cmd_write_mem write_mem;
    struct meta_deferred_cmd_add_break add_break;
    struct meta_deferred_cmd_remove_break remove_break;
    struct meta_deferred_cmd_add_write_watch add_write_watch;
    struct meta_deferred_cmd_remove_write_watch remove_write_watch;
    struct meta_deferred_cmd_add_read_watch add_read_watch;
    struct meta_deferred_cmd_remove_read_watch remove_read_watch;
};

enum deferred_cmd_status {
    DEFERRED_CMD_IN_PROGRESS,
    DEFERRED_CMD_SUCCESS,
    DEFERRED_CMD_FAILURE
};

struct deferred_cmd {
    enum deferred_cmd_type cmd_type;
    union deferred_cmd_meta meta;
    enum deferred_cmd_status status;
    struct fifo_node fifo;
};

static void deferred_cmd_init(struct deferred_cmd *cmd) {
    memset(cmd, 0, sizeof(*cmd));
    cmd->status = DEFERRED_CMD_IN_PROGRESS;
}

static pthread_mutex_t deferred_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t deferred_cmd_cond = PTHREAD_COND_INITIALIZER;
static struct fifo_head deferred_cmd_fifo =
    FIFO_HEAD_INITIALIZER(deferred_cmd_fifo);

static void deferred_cmd_lock(void) {
    if (pthread_mutex_lock(&deferred_cmd_mutex) < 0)
        abort(); // TODO error handling
}

static void deferred_cmd_unlock(void) {
    if (pthread_mutex_unlock(&deferred_cmd_mutex) < 0)
        abort(); // TODO error handling
}

static void deferred_cmd_wait(void) {
    if (pthread_cond_wait(&deferred_cmd_cond, &deferred_cmd_mutex) != 0)
        abort(); // TODO error handling
}

static void deferred_cmd_signal(void) {
    if (pthread_cond_signal(&deferred_cmd_cond) != 0)
        abort(); // TODO error handling
}

static void deferred_cmd_push_nolock(struct deferred_cmd *cmd) {
    fifo_push(&deferred_cmd_fifo, &cmd->fifo);
}

static void deferred_cmd_exec(struct deferred_cmd *cmd) {
    deferred_cmd_lock();

    deferred_cmd_push_nolock(cmd);

    while (cmd->status == DEFERRED_CMD_IN_PROGRESS)
        deferred_cmd_wait();

    deferred_cmd_unlock();
}

static struct deferred_cmd *deferred_cmd_pop_nolock(void) {
    struct fifo_node *ret = fifo_pop(&deferred_cmd_fifo);
    if (ret)
        return &FIFO_DEREF(ret, struct deferred_cmd, fifo);
    return NULL;
}

int gdb_stub_read_mem(void *out, addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_READ_MEM;
    cmd.meta.read_mem.out_buf = out;
    cmd.meta.read_mem.addr = addr;
    cmd.meta.read_mem.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

int gdb_stub_write_mem(void const *input, addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_WRITE_MEM;
    cmd.meta.write_mem.in_buf = input;
    cmd.meta.write_mem.addr = addr;
    cmd.meta.write_mem.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_add_break(addr32_t addr) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_ADD_BREAK;
    cmd.meta.add_break.addr = addr;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_remove_break(addr32_t addr) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_REMOVE_BREAK;
    cmd.meta.remove_break.addr = addr;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_add_write_watchpoint(addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_ADD_WRITE_WATCH;
    cmd.meta.add_write_watch.addr = addr;
    cmd.meta.add_write_watch.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_remove_write_watchpoint(addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_REMOVE_WRITE_WATCH;
    cmd.meta.remove_write_watch.addr = addr;
    cmd.meta.remove_write_watch.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_add_read_watchpoint(addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_ADD_READ_WATCH;
    cmd.meta.add_read_watch.addr = addr;
    cmd.meta.add_read_watch.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static int gdb_stub_remove_read_watchpoint(addr32_t addr, unsigned len) {
    struct deferred_cmd cmd;

    deferred_cmd_init(&cmd);
    cmd.cmd_type = DEFERRED_CMD_REMOVE_READ_WATCH;
    cmd.meta.remove_read_watch.addr = addr;
    cmd.meta.remove_read_watch.len = len;

    deferred_cmd_exec(&cmd);

    if (cmd.status == DEFERRED_CMD_SUCCESS)
        return 0;
    return -1;
}

static void deferred_cmd_do_get_all_regs(struct deferred_cmd *cmd) {
    debug_get_all_regs(DEBUG_CONTEXT_SH4, cmd->meta.get_all_regs.reg_file_out,
                       sizeof(cmd->meta.get_all_regs.reg_file_out));
    cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_set_all_regs(struct deferred_cmd *cmd) {
    debug_set_all_regs(DEBUG_CONTEXT_SH4, cmd->meta.set_all_regs.reg_file_in,
                       sizeof(cmd->meta.set_all_regs.reg_file_in));
    cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_set_reg(struct deferred_cmd *cmd) {
    debug_set_reg(DEBUG_CONTEXT_SH4, cmd->meta.set_reg.idx, cmd->meta.set_reg.val);
    cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_read_mem(struct deferred_cmd *cmd) {
    void *out = cmd->meta.read_mem.out_buf;
    unsigned len = cmd->meta.read_mem.len;
    addr32_t addr = cmd->meta.read_mem.addr;

    if (debug_read_mem(DEBUG_CONTEXT_SH4, out, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_write_mem(struct deferred_cmd *cmd) {
    void const *input = cmd->meta.write_mem.in_buf;
    unsigned len = cmd->meta.write_mem.len;
    addr32_t addr = cmd->meta.write_mem.addr;

    if (debug_write_mem(DEBUG_CONTEXT_SH4, input, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_add_break(struct deferred_cmd *cmd) {
    addr32_t addr = cmd->meta.add_break.addr;
    if (debug_add_break(DEBUG_CONTEXT_SH4, addr) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_remove_break(struct deferred_cmd *cmd) {
    addr32_t addr = cmd->meta.remove_break.addr;
    if (debug_remove_break(DEBUG_CONTEXT_SH4, addr) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
}

static void deferred_cmd_do_add_write_watch(struct deferred_cmd *cmd) {
#ifdef ENABLE_WATCHPOINTS
    addr32_t addr = cmd->meta.add_write_watch.addr;
    unsigned len = cmd->meta.add_write_watch.len;
    if (debug_add_w_watch(DEBUG_CONTEXT_SH4, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
#else
    cmd->status = DEFERRED_CMD_FAILURE;
#endif
}

static void deferred_cmd_do_remove_write_watch(struct deferred_cmd *cmd) {
#ifdef ENABLE_WATCHPOINTS
    addr32_t addr = cmd->meta.remove_write_watch.addr;
    unsigned len = cmd->meta.remove_write_watch.len;
    if (debug_remove_w_watch(DEBUG_CONTEXT_SH4, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
#else
    cmd->status = DEFERRED_CMD_FAILURE;
#endif
}

static void deferred_cmd_do_add_read_watch(struct deferred_cmd *cmd) {
#ifdef ENABLE_WATCHPOINTS
    addr32_t addr = cmd->meta.add_read_watch.addr;
    unsigned len = cmd->meta.add_read_watch.len;
    if (debug_add_r_watch(DEBUG_CONTEXT_SH4, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
#else
    cmd->status = DEFERRED_CMD_FAILURE;
#endif
}

static void deferred_cmd_do_remove_read_watch(struct deferred_cmd *cmd) {
#ifdef ENABLE_WATCHPOINTS
    addr32_t addr = cmd->meta.remove_read_watch.addr;
    unsigned len = cmd->meta.remove_read_watch.len;
    if (debug_remove_r_watch(DEBUG_CONTEXT_SH4, addr, len) != 0)
        cmd->status = DEFERRED_CMD_FAILURE;
    else
        cmd->status = DEFERRED_CMD_SUCCESS;
#else
    cmd->status = DEFERRED_CMD_FAILURE;
#endif
}

static void deferred_cmd_run(void) {
    struct deferred_cmd *cmd;

    deferred_cmd_lock();

    while ((cmd = deferred_cmd_pop_nolock())) {
        washdc_log_debug("gdb_stub: deferred cmd %s\n",
                         deferred_cmd_names[cmd->cmd_type]);
        switch (cmd->cmd_type) {
        case DEFERRED_CMD_GET_ALL_REGS:
            deferred_cmd_do_get_all_regs(cmd);
            break;
        case DEFERRED_CMD_SET_ALL_REGS:
            deferred_cmd_do_set_all_regs(cmd);
            break;
        case DEFERRED_CMD_SET_REG:
            deferred_cmd_do_set_reg(cmd);
            break;
        case DEFERRED_CMD_READ_MEM:
            deferred_cmd_do_read_mem(cmd);
            break;
        case DEFERRED_CMD_WRITE_MEM:
            deferred_cmd_do_write_mem(cmd);
            break;
        case DEFERRED_CMD_ADD_BREAK:
            deferred_cmd_do_add_break(cmd);
            break;
        case DEFERRED_CMD_REMOVE_BREAK:
            deferred_cmd_do_remove_break(cmd);
            break;
        case DEFERRED_CMD_ADD_WRITE_WATCH:
            deferred_cmd_do_add_write_watch(cmd);
            break;
        case DEFERRED_CMD_REMOVE_WRITE_WATCH:
            deferred_cmd_do_remove_write_watch(cmd);
            break;
        case DEFERRED_CMD_ADD_READ_WATCH:
            deferred_cmd_do_add_read_watch(cmd);
            break;
        case DEFERRED_CMD_REMOVE_READ_WATCH:
            deferred_cmd_do_remove_read_watch(cmd);
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }
    }

    deferred_cmd_signal();

    deferred_cmd_unlock();
}
