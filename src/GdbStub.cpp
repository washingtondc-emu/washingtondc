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

#include <sstream>
#include <iostream>
#include <iomanip>

#include <boost/bind.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "hw/sh4/sh4.hpp"
#include "Dreamcast.hpp"

#include "GdbStub.hpp"

// uncomment this to log all traffic in/out of the debugger to stdout
// #define GDBSTUB_VERBOSE

static void gdb_on_break(void *arg);
static void gdb_on_read_watchpoint(addr32_t addr, void *arg);
static void gdb_on_write_watchpoint(addr32_t addr, void *arg);
static void gdb_on_softbreak(inst_t inst, addr32_t addr, void *arg);
static int decode_hex(char ch);

static std::string craft_packet(std::string data_in);
static std::string gdb_serialize_regs(struct gdb_stub *stub);
static void deserialize_regs(std::string input_str, reg32_t regs[N_REGS]);
static std::string serialize_data(void const *buf, unsigned buf_len);
static int decode_hex(char ch);
static void write_start(struct gdb_stub *stub);
static std::string extract_packet(std::string packet_in);
static int set_reg(reg32_t reg_file[SH4_REGISTER_COUNT], Sh4::FpuReg *fpu,
                   unsigned reg_no, reg32_t reg_val, bool bank);
static std::string err_str(unsigned err_val);
static void handle_packet(struct gdb_stub *stub,  std::string pkt);
static void transmit_pkt(struct gdb_stub *stub, const std::string& pkt);
static std::string next_packet(struct gdb_stub *stub);
// static void errror_handler(int error_tp, void *argptr);
static void expect_mem_access_error(struct gdb_stub *stub, bool should);

static void handle_c_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_q_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_g_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_m_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_M_packet(struct gdb_stub *stub, std::string dat);
static void handle_s_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_G_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_P_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_D_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_K_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_Z_packet(struct gdb_stub *stub, std::string dat);
static std::string handle_z_packet(struct gdb_stub *stub, std::string dat);

static std::string read_mem_4(struct gdb_stub *stub,
                              addr32_t addr, unsigned len);
static std::string read_mem_2(struct gdb_stub *stub, addr32_t addr,
                              unsigned len);
static std::string read_mem_1(struct gdb_stub *stub, addr32_t addr,
                              unsigned len);

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg);
static void handle_events(struct bufferevent *bev, short events, void *arg);
static void handle_read(struct bufferevent *bev, void *arg);
static void handle_write(struct bufferevent *bev, void *arg);

template<class Stream>
static size_t deserialize_data(Stream& input, void *out, size_t max_sz) {
    uint8_t *out8 = (uint8_t*)out;
    size_t bytes_written = 0;
    char ch;
    char const eof = std::char_traits<char>::eof();
    while ((ch = input.get()) != eof) {
        if (bytes_written >= max_sz)
            return max_sz;
        *out8 = uint8_t(decode_hex(ch));
        bytes_written++;

        if ((ch = input.get()) != eof) {
            *out8 <<= 4;
            *out8 |= uint8_t(decode_hex(ch));
        } else {
            break;
        }

        out8++;
    }

    return bytes_written;
}

void gdb_init(struct gdb_stub *stub, struct debugger *dbg) {
    stub->frontend_supports_swbreak = false;
    stub->is_writing = false;
    stub->should_expect_mem_access_error =false;
    stub->listener = NULL;
    stub->is_listening = false;
    stub->bev = NULL;
    stub->dbg = dbg;

    dbg->frontend.step = NULL;
    dbg->frontend.attach = gdb_attach;
    dbg->frontend.on_break = gdb_on_break;
    dbg->frontend.on_read_watchpoint = gdb_on_read_watchpoint;
    dbg->frontend.on_write_watchpoint = gdb_on_write_watchpoint;
    dbg->frontend.on_softbreak = gdb_on_softbreak;
    dbg->frontend.arg = stub;
}

void gdb_cleanup(struct gdb_stub *stub) {
    // TODO: cleanup
}

void gdb_attach(void *argptr) {
    struct gdb_stub *stub = (struct gdb_stub*)argptr;

    std::cout << "Awaiting remote GDB connection on port " << GDB_PORT_NO <<
        "..." << std::endl;

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(GDB_PORT_NO);
    unsigned event_flags = LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE;
    stub->listener = evconnlistener_new_bind(dc_event_base, listener_cb, stub,
                                             event_flags, -1,
                                             (struct sockaddr*)&sin, sizeof(sin));

    if (!stub->listener)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    // the listener_cb will set is_listening = false when we have a connection
    stub->is_listening = true;
    do {
        std::cout << "still waiting..." << std::endl;
        if (event_base_loop(dc_event_base, EVLOOP_ONCE) != 0)
            exit(4);
    } while (stub->is_listening);

    std::cout << "Connection established." << std::endl;
}

static void gdb_on_break(void *arg) {
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    transmit_pkt(stub, craft_packet(std::string("S05")));
}

static void gdb_on_softbreak(inst_t inst, addr32_t addr, void *arg) {
    std::stringstream pkt_txt;
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    if (stub->frontend_supports_swbreak)
        pkt_txt << "T05swbreak:" << std::hex << addr << ";";
    else
        pkt_txt << "S05";

    stub->dbg->cur_state = DEBUG_STATE_BREAK;

    transmit_pkt(stub, craft_packet(pkt_txt.str()));
}

static void gdb_on_read_watchpoint(addr32_t addr, void *arg) {
    std::stringstream pkt_txt;
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    // pkt_txt << "T05rwatch:" << std::hex << addr << ";";
    pkt_txt << "S05";

    transmit_pkt(stub, craft_packet(pkt_txt.str()));
}

static void gdb_on_write_watchpoint(addr32_t addr, void *arg) {
    std::stringstream pkt_txt;
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    // pkt_txt << "T05watch:" << std::hex << addr << ";";
    pkt_txt << "S05";

    transmit_pkt(stub, craft_packet(pkt_txt.str()));
}

static std::string gdb_serialize_regs(struct gdb_stub *stub) {
    Sh4 *cpu = dreamcast_get_cpu();
    reg32_t reg_file[SH4_REGISTER_COUNT];
    sh4_get_regs(cpu, reg_file);
    Sh4::FpuReg fpu_reg = sh4_get_fpu(cpu);
    reg32_t regs[N_REGS] = { 0 };

    // general-purpose registers
    for (int i = 0; i < 16; i++) {
        if (i < 8) {
            if (reg_file[SH4_REG_SR] & SH4_SR_RB_MASK)
                regs[R0 + i] = reg_file[SH4_REG_R0_BANK1 + i];
            else
                regs[R0 + i] = reg_file[SH4_REG_R0_BANK0 + i];
        }else {
            regs[R0 + i] = reg_file[SH4_REG_R8 + (i - 8)];
        }
    }

    // banked registers
    for (int i = 0; i < 8; i++) {
        regs[R0B0 + i] = reg_file[SH4_REG_R0_BANK0 + i];
        regs[R0B1 + i] = reg_file[SH4_REG_R0_BANK1 + i];
    }

    // TODO: floating point registers

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
    regs[FPUL] = fpu_reg.fpul;
    regs[FPSCR] = fpu_reg.fpscr;

    return serialize_data(regs, sizeof(regs));
}

static void deserialize_regs(std::string input_str, reg32_t regs[N_REGS]) {
    std::stringstream stream(input_str);
    size_t sz_expect = N_REGS * sizeof(*regs);

    size_t sz_actual = deserialize_data<std::stringstream>(stream, regs,
                                                           sz_expect);

    if (sz_expect != sz_actual) {
        // TODO: better error messages
        std::cout << "sz_expect is " << sz_expect << ", sz_actual is " <<
            sz_actual << std::endl;
        BOOST_THROW_EXCEPTION(IntegrityError("Some shit with gdb i guess"));
    }
}

static std::string serialize_data(void const *buf, unsigned buf_len) {
    uint8_t const *buf8 = (uint8_t const*)buf;
    std::stringstream ss;
    static const char hex_tbl[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    for (unsigned i = 0; i < buf_len; i++) {
        ss << hex_tbl[(*buf8) >> 4] << hex_tbl[(*buf8) & 0xf];
        buf8++;
    }

    return ss.str();
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

static void transmit(struct gdb_stub *stub, const std::string& data) {
    for (std::string::const_iterator it = data.begin(); it != data.end(); it++)
        stub->output_queue.push(*it);

    write_start(stub);
}

static void write_start(struct gdb_stub *stub) {
    if (stub->output_queue.empty())
        stub->is_writing = false;

    if (stub->is_writing || stub->output_queue.empty())
        return;

    struct evbuffer *write_buffer;
    if (!(write_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    while (!stub->output_queue.empty()) {
        uint8_t dat = stub->output_queue.front();
        evbuffer_add(write_buffer, &dat, sizeof(dat));
        stub->output_queue.pop();
    }

    bufferevent_write_buffer(stub->bev, write_buffer);
    evbuffer_free(write_buffer);
}

static std::string craft_packet(std::string data_in) {
    std::stringstream ss;
    uint8_t csum = 0;
    static const char hex_tbl[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };


    for (std::string::iterator it = data_in.begin(); it != data_in.end(); it++)
        csum += *it;

    ss << "$" << data_in << "#" << hex_tbl[csum >> 4] << hex_tbl[csum & 0xf];
    return ss.str();
}

static std::string extract_packet(std::string packet_in) {
    size_t dollar_idx = packet_in.find_first_of('$');
    size_t pound_idx = packet_in.find_last_of('#');

    return packet_in.substr(dollar_idx + 1, pound_idx - dollar_idx - 1);
}

static int set_reg(reg32_t reg_file[SH4_REGISTER_COUNT], Sh4::FpuReg *fpu,
                   unsigned reg_no, reg32_t reg_val, bool bank) {
    // there is some ambiguity over whether register banking should be based off
    // of the old sr or the new sr.  For now, it's based off of the old sr.

    // TODO: floating point registers
    if (reg_no >= R0 && reg_no <= R15) {
        unsigned idx = reg_no - R0;

        if (idx < 8) {
            if (bank)
                reg_file[SH4_REG_R0_BANK1 + idx] = reg_val;
            else
                reg_file[SH4_REG_R0_BANK0 + idx] = reg_val;
        } else {
            reg_file[SH4_REG_R8 + (idx + 8)] = reg_val;
        }
    } else if (reg_no >= R0B0 && reg_no <= R7B0) {
        reg_file[reg_no - R0B0 + SH4_REG_R0_BANK0] = reg_val;
    } else if (reg_no >= R0B1 && reg_no <= R7B1) {
        reg_file[reg_no - R0B1 + SH4_REG_R0_BANK1] = reg_val;
    } else if (reg_no == PC) {
        reg_file[SH4_REG_PC] =reg_val;
    } else if (reg_no == PR) {
        reg_file[SH4_REG_PR] = reg_val;
    } else if (reg_no == GBR) {
        reg_file[SH4_REG_GBR] = reg_val;
    } else if (reg_no == VBR) {
        reg_file[SH4_REG_VBR] = reg_val;
    } else if (reg_no == MACH) {
        reg_file[SH4_REG_MACH] = reg_val;
    } else if (reg_no == MACL) {
        reg_file[SH4_REG_MACL] = reg_val;
    } else if (reg_no == SR) {
        reg_file[SH4_REG_SR] = reg_val;
    } else if (reg_no == SSR) {
        reg_file[SH4_REG_SSR] = reg_val;
    } else if (reg_no == SPC) {
        reg_file[SH4_REG_SPC] = reg_val;
    } else if (reg_no == FPUL) {
        fpu->fpul = reg_val;
    } else if (reg_no == FPSCR) {
        fpu->fpscr = reg_val;
    } else {
#ifdef GDBSTUB_VERBOSE
        std::cout << "WARNING: GdbStub unable to set value of register " <<
            std::hex << reg_no << " to " << reg_val << std::endl;
#endif
        return 1;
    }

    return 0;
}

static std::string err_str(unsigned err_val) {
    static char const hex_chars[] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    // dont print more than 2 digits
    err_val &= 0xff;
    std::string err_str = std::string() + hex_chars[err_val >> 4];
    err_val &= 0x0f;
    err_str += hex_chars[err_val];

    return "E" + err_str;
}

static void handle_c_packet(struct gdb_stub *stub, std::string dat) {
    stub->dbg->cur_state = DEBUG_STATE_NORM;
}

static std::string handle_q_packet(struct gdb_stub *stub, std::string dat) {
    if (dat.substr(0, 10) == "qSupported") {
        std::string reply;
        size_t semicolon_idx = dat.find_first_of(';');

        if (semicolon_idx == std::string::npos)
            return std::string();

        dat = dat.substr(semicolon_idx + 1);

        std::vector<std::string> features;
        boost::algorithm::split(features, dat,
                                boost::algorithm::is_any_of(";"));
        for (std::vector<std::string>::iterator it = features.begin();
             it != features.end(); it++) {
            std::string feat = *it;
            bool supported = false;

            size_t plus_or_minus_idx = feat.find_last_of("+-");

            /*
             * ignore all the settings that try to set variables,
             * we're really only here for swbreak.
             */

            if (plus_or_minus_idx != std::string::npos) {
                if (feat.at(plus_or_minus_idx) == '+')
                    supported = true;
                feat = feat.substr(0, plus_or_minus_idx);
            }

            if (feat == "swbreak") {
                if (supported) {
                    stub->frontend_supports_swbreak = true;
                    reply += "swbreak+;";
                } else {
                    reply += "swbreak-;";
                }
            } else {
                reply += feat + "-;";
            }
        }

        return reply;
    }

    return std::string();
}

static std::string handle_g_packet(struct gdb_stub *stub, std::string dat) {
    return gdb_serialize_regs(stub);
}

static std::string handle_m_packet(struct gdb_stub *stub, std::string dat) {
    size_t addr_idx = dat.find_last_of('m') + 1;
    size_t comma_idx = dat.find_last_of(',');
    size_t len_idx = comma_idx + 1;

    unsigned len;
    unsigned addr;
    std::stringstream(dat.substr(addr_idx, comma_idx)) >> std::hex >> addr;
    std::stringstream(dat.substr(len_idx)) >> std::hex >> len;

    if (len % 4 == 0)
        return read_mem_4(stub, addr, len);
    else if (len % 2 == 0)
        return read_mem_2(stub, addr, len);
    else
        return read_mem_1(stub, addr, len);
}

static std::string read_mem_4(struct gdb_stub *stub,
                              addr32_t addr, unsigned len) {
    std::stringstream ss;
    expect_mem_access_error(stub, true);

    while (len) {
        uint32_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr += 4;
        } catch (BaseException& exc) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        if (stub->mem_access_error) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        ss << serialize_data(&val, sizeof(val));
        len -= 4;
    }

    expect_mem_access_error(stub, false);
    return ss.str();
}

static std::string read_mem_2(struct gdb_stub *stub, addr32_t addr,
                              unsigned len) {
    std::stringstream ss;
    expect_mem_access_error(stub, true);

    while (len) {
        uint16_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr += 2;
        } catch (BaseException& exc) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        if (stub->mem_access_error) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        ss << serialize_data(&val, sizeof(val));
        len -= 2;
    }

    expect_mem_access_error(stub, false);
    return ss.str();
}

static std::string read_mem_1(struct gdb_stub *stub, addr32_t addr,
                              unsigned len) {
    std::stringstream ss;
    expect_mem_access_error(stub, true);

    while (len--) {
        uint8_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr++;
        } catch (BaseException& exc) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        if (stub->mem_access_error) {
            expect_mem_access_error(stub, false);
            return err_str(EINVAL);
        }

        ss << serialize_data(&val, sizeof(val));
    }

    expect_mem_access_error(stub, false);
    return ss.str();
}

/*
 * TODO: bounds checking (not that I expect there to be any hackers going in
 * through the debugger of all places)
 */
static std::string handle_M_packet(struct gdb_stub *stub, std::string dat) {
    size_t addr_idx = dat.find_last_of('M') + 1;
    size_t comma_idx = dat.find_last_of(',');
    size_t colon_idx = dat.find_last_of(':');
    size_t len_idx = comma_idx + 1;
    size_t dat_idx = colon_idx + 1;

    std::string addr_substr = dat.substr(addr_idx, comma_idx - addr_idx);
    std::string len_substr = dat.substr(len_idx, colon_idx - len_idx);

    unsigned addr;
    unsigned len;

    std::stringstream(addr_substr) >> std::hex >> addr;
    std::stringstream(len_substr) >> std::hex >> len;

    try {
        std::stringstream ss(dat.substr(dat_idx));
        if (len < 1024) {
            uint8_t *buf = new uint8_t[len];
            try {
                expect_mem_access_error(stub, true);
                deserialize_data(ss, buf, len);
                sh4_write_mem(dreamcast_get_cpu(), buf, addr, len);

                if (stub->mem_access_error) {
                    expect_mem_access_error(stub, false);
                    return err_str(EINVAL);
                }

            } catch (BaseException& exc) {
                expect_mem_access_error(stub, false);
                delete[] buf;
                throw;
            }
            delete[] buf;
        } else {
            BOOST_THROW_EXCEPTION(InvalidParamError());
        }
    } catch (BaseException& exc) {
        expect_mem_access_error(stub, false);
        std::cerr << boost::diagnostic_information(exc);
        return err_str(EINVAL);
    }

    expect_mem_access_error(stub, false);
    return "OK";
}

static void handle_s_packet(struct gdb_stub *stub, std::string dat) {
    stub->dbg->cur_state = DEBUG_STATE_PRE_STEP;
}

static std::string handle_G_packet(struct gdb_stub *stub, std::string dat) {
    reg32_t regs[N_REGS];

    deserialize_regs(dat.substr(1), regs);

    reg32_t new_regs[SH4_REGISTER_COUNT];
    sh4_get_regs(dreamcast_get_cpu(), new_regs);
    Sh4::FpuReg new_fpu = sh4_get_fpu(dreamcast_get_cpu());
    bool bank = new_regs[SH4_REG_SR] & SH4_SR_RB_MASK;

    for (unsigned reg_no = 0; reg_no < N_REGS; reg_no++)
        set_reg(new_regs, &new_fpu, reg_no, regs[reg_no], bank);
    return "OK";
}

static std::string handle_P_packet(struct gdb_stub *stub, std::string dat) {
    size_t equals_idx = dat.find_first_of('=');

    if (equals_idx >= dat.size() - 1) {
#ifdef GDBSTUB_VERBOSE
        std::cout << "WARNING: malformed P packet in gdbstub \"" << dat <<
            "\"" << std::endl;
#endif

        return "E16";
    }

    std::string reg_no_str = dat.substr(1, equals_idx - 1);
    std::string reg_val_str = dat.substr(equals_idx + 1);

    unsigned reg_no = 0;
    reg32_t reg_val = 0;
    std::stringstream reg_no_stream(reg_no_str);
    std::stringstream reg_val_stream(reg_val_str);
    deserialize_data<std::stringstream>(reg_no_stream, &reg_no, sizeof(reg_no));
    deserialize_data<std::stringstream>(reg_val_stream, &reg_val,
                                        sizeof(reg_val));

    if (reg_no >= N_REGS) {
#ifdef GDBSTUB_VERBOSE
        std::cout << "ERROR: unable to write to register number " <<
            std::hex << reg_no << std::endl;
#endif
        return "E16";
    }

    Sh4 *cpu = dreamcast_get_cpu();
    reg32_t regs[SH4_REGISTER_COUNT];
    sh4_get_regs(cpu, regs);
    Sh4::FpuReg fpu = sh4_get_fpu(cpu);
    set_reg(regs, &fpu, reg_no, reg_val,
            bool(regs[SH4_REG_SR] & SH4_SR_RB_MASK));
    sh4_set_regs(cpu, regs);

    return "OK";
}

static std::string handle_D_packet(struct gdb_stub *stub, std::string dat) {
    stub->dbg->cur_state = DEBUG_STATE_NORM;

    debug_on_detach(stub->dbg);

    return "OK";
}

static std::string handle_K_packet(struct gdb_stub *stub, std::string dat) {
    dreamcast_kill();

    return "OK";
}

static std::string handle_Z_packet(struct gdb_stub *stub, std::string dat) {
    if (dat.at(1) == '1') {
        //hardware breakpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if ((first_comma_idx == std::string::npos) ||
            (first_comma_idx == dat.size() - 1)) {
            return std::string(); // something is wrong and/or unexpected
        }

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        dat = dat.substr(first_comma_idx + 1, last_comma_idx - first_comma_idx);

        addr32_t break_addr;
        std::stringstream(dat) >> std::hex >> break_addr;

        int err_code = debug_add_break(stub->dbg, break_addr);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else if (dat.at(1) == '2') {
        // write watchpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if ((first_comma_idx == std::string::npos) ||
            (first_comma_idx == dat.size() - 1)) {
            return std::string(); // something is wrong and/or unexpected
        }

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        size_t last_hash_idx = dat.find_last_of('#');
        std::string len_str = dat.substr(last_comma_idx + 1,
                                         last_hash_idx - last_comma_idx);
        std::string addr_str = dat.substr(first_comma_idx + 1,
                                          last_comma_idx - first_comma_idx);

        unsigned length;
        addr32_t watch_addr;
        std::stringstream(addr_str) >> std::hex >> watch_addr;
        std::stringstream(len_str) >> std::hex >> length;

        int err_code = debug_add_w_watch(stub->dbg, watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else if (dat.at(1) == '3') {
        // read watchpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if ((first_comma_idx == std::string::npos) ||
            (first_comma_idx == dat.size() - 1)) {
            return std::string(); // something is wrong and/or unexpected
        }

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        size_t last_hash_idx = dat.find_last_of('#');
        std::string len_str = dat.substr(last_comma_idx + 1,
                                         last_hash_idx - last_comma_idx);
        std::string addr_str = dat.substr(first_comma_idx + 1,
                                          last_comma_idx - first_comma_idx);

        unsigned length;
        addr32_t watch_addr;
        std::stringstream(addr_str) >> std::hex >> watch_addr;
        std::stringstream(len_str) >> std::hex >> length;

        int err_code = debug_add_r_watch(stub->dbg, watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else {
        // unsupported
        return std::string();
    }
}

static std::string handle_z_packet(struct gdb_stub *stub, std::string dat) {
    if (dat.at(1) == '1') {
        //hardware breakpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if (first_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        dat = dat.substr(first_comma_idx + 1, last_comma_idx - first_comma_idx);

        addr32_t break_addr;
        std::stringstream(dat) >> std::hex >> break_addr;

        int err_code = debug_remove_break(stub->dbg, break_addr);

        if (err_code == 0)
            return "OK";

        return err_str(err_code);
    } else if (dat.at(1) == '2') {
        // write watchpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if ((first_comma_idx == std::string::npos) ||
            (first_comma_idx == dat.size() - 1)) {
            return std::string(); // something is wrong and/or unexpected
        }

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        size_t last_hash_idx = dat.find_last_of('#');
        std::string len_str = dat.substr(last_comma_idx + 1,
                                         last_hash_idx - last_comma_idx);
        std::string addr_str = dat.substr(first_comma_idx + 1,
                                          last_comma_idx - first_comma_idx);

        unsigned length;
        addr32_t watch_addr;
        std::stringstream(addr_str) >> std::hex >> watch_addr;
        std::stringstream(len_str) >> std::hex >> length;

        int err_code = debug_remove_w_watch(stub->dbg, watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else if (dat.at(1) == '3') {
        // read watchpoint

        if (dat.find_first_of(';') != std::string::npos)
            return std::string(); // we don't support conditions

        size_t first_comma_idx = dat.find_first_of(',');
        if ((first_comma_idx == std::string::npos) ||
            (first_comma_idx == dat.size() - 1)) {
            return std::string(); // something is wrong and/or unexpected
        }

        size_t last_comma_idx = dat.find_last_of(',');
        if (last_comma_idx == std::string::npos)
            return std::string(); // something is wrong and/or unexpected

        size_t last_hash_idx = dat.find_last_of('#');
        std::string len_str = dat.substr(last_comma_idx + 1,
                                         last_hash_idx - last_comma_idx);
        std::string addr_str = dat.substr(first_comma_idx + 1,
                                          last_comma_idx - first_comma_idx);

        unsigned length;
        addr32_t watch_addr;
        std::stringstream(addr_str) >> std::hex >> watch_addr;
        std::stringstream(len_str) >> std::hex >> length;

        int err_code = debug_remove_r_watch(stub->dbg, watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else {
        // unsupported
        return std::string();
    }
}

static void handle_packet(struct gdb_stub *stub,  std::string pkt) {
    std::string response;
    std::string dat = extract_packet(pkt);

    response = craft_packet(std::string());

    if (dat.size()) {
        if (dat.at(0) == 'q') {
            response = craft_packet(handle_q_packet(stub, dat));
        } else if (dat.at(0) == 'g') {
            response = craft_packet(handle_g_packet(stub, dat));
        } else if (dat.at(0) == 'G') {
            response = craft_packet(handle_G_packet(stub, dat));
        } else if (dat.at(0) == 'm') {
            response = craft_packet(handle_m_packet(stub, dat));
        } else if (dat.at(0) == 'M') {
            response = craft_packet(handle_M_packet(stub, dat));
        } else if (dat.at(0) == '?') {
            response = craft_packet(std::string("S05 create:"));
        } else if (dat.at(0) == 's') {
            handle_s_packet(stub, dat);
            return;
        } else if (dat.at(0) == 'c') {
            handle_c_packet(stub, dat);
            return;
        } else if (dat.at(0) == 'P') {
            response = craft_packet(handle_P_packet(stub, dat));
        } else if (dat.at(0) == 'D') {
            response = craft_packet(handle_D_packet(stub, dat));
        } else if (dat.at(0) == 'k') {
            response = craft_packet(handle_K_packet(stub, dat));
        } else if (dat.at(0) == 'z') {
            response = craft_packet(handle_z_packet(stub, dat));
        } else if (dat.at(0) == 'Z') {
            response = craft_packet(handle_Z_packet(stub, dat));
        }
    }

    transmit_pkt(stub, response);
}

static void transmit_pkt(struct gdb_stub *stub, const std::string& pkt) {
#ifdef GDBSTUB_VERBOSE
    std::cout << ">>>> " << pkt << std::endl;
#endif

    stub->unack_packet = pkt;
    transmit(stub, pkt);
}

static std::string next_packet(struct gdb_stub *stub) {
    std::string pkt;
    std::string pktbuf_tmp(stub->input_packet);
    char ch;

    // wait around for the start character, ignore all other characters
    do {
        if (!pktbuf_tmp.size())
            return std::string();

        ch = pktbuf_tmp.at(0);
        pktbuf_tmp = pktbuf_tmp.substr(1);

        if (ch == std::char_traits<char>::eof())
            return std::string();
    } while(ch != '$');
    pkt += ch;

    // now, read until a # or end of buffer is found
    do {
        if (!pktbuf_tmp.size())
            return std::string();

        ch = pktbuf_tmp.at(0);
        pktbuf_tmp = pktbuf_tmp.substr(1);

        if (ch == std::char_traits<char>::eof())
            return std::string();

        pkt += ch;
    } while(ch != '#');
        
    // read the one-byte (two char) checksum
    if (!pktbuf_tmp.size())
        return std::string();
    ch = pktbuf_tmp.at(0);
    if (ch == std::char_traits<char>::eof())
        return std::string();
    pktbuf_tmp = pktbuf_tmp.substr(1);
    pkt += ch;

    if (!pktbuf_tmp.size())
        return std::string();
    ch = pktbuf_tmp.at(0);
    if (ch == std::char_traits<char>::eof())
        return std::string();
    pktbuf_tmp = pktbuf_tmp.substr(1);
    pkt += ch;

    stub->input_packet = pktbuf_tmp;

#ifdef GDBSTUB_VERBOSE
    std::cout << "<<<< " << pkt << std::endl;
#endif

    return pkt;
}

// static void errror_handler(int error_tp, void *argptr) {
//     struct gdb_stub* stub = (struct gdb_stub*)argptr;

//     if (stub->should_expect_mem_access_error) {
//         stub->mem_access_error = true;
//     } else {
//         error_print();
//         exit(1);
//     }
// }

static void expect_mem_access_error(struct gdb_stub *stub, bool should) {
    stub->mem_access_error = false;
    stub->should_expect_mem_access_error = true;
}

static void
listener_cb(struct evconnlistener *listener,
            evutil_socket_t fd, struct sockaddr *saddr,
            int socklen, void *arg) {
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    if (!stub->is_listening)
        return;

    stub->bev = bufferevent_socket_new(dc_event_base, fd,
                                       BEV_OPT_CLOSE_ON_FREE);
    if (!stub->bev)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_setcb(stub->bev, handle_read,
                      handle_write, handle_events, stub);
    bufferevent_enable(stub->bev, EV_WRITE);
    bufferevent_enable(stub->bev, EV_READ);

    stub->is_listening = false;
}

static void handle_events(struct bufferevent *bev, short events, void *arg) {
    exit(2);
}

static void handle_read(struct bufferevent *bev, void *arg) {
    struct evbuffer *read_buffer;
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    if (!(read_buffer = evbuffer_new()))
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    bufferevent_read_buffer(bev, read_buffer);
    size_t buflen = evbuffer_get_length(read_buffer);

    for (unsigned i = 0; i < buflen; i++) {
        uint8_t tmp;
        if (evbuffer_remove(read_buffer, &tmp, sizeof(tmp)) < 0)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        stub->input_queue.push(tmp);
    }

    while (stub->input_queue.size()) {
        char c = stub->input_queue.front();
        stub->input_queue.pop();

        if (stub->input_packet.size()) {
            stub->input_packet += c;

            std::string pkt = next_packet(stub);
            if (pkt.size()) {
                stub->input_packet = "";

                // TODO: verify the checksum

#ifdef GDBSTUB_VERBOSE
                std::cout << ">>>> +" << std::endl;
#endif
                transmit(stub, std::string("+"));
                handle_packet(stub, pkt);
            }
        } else {
            if (c == '+') {
#ifdef GDBSTUB_VERBOSE
                std::cout << "<<<< +" << std::endl;
#endif
                if (!stub->unack_packet.size())
                    std::cerr << "WARNING: received acknowledgement for unsent " <<
                        "packet" << std::endl;
                stub->unack_packet = "";
            } else if (c == '-') {
#ifdef GDBSTUB_VERBOSE
                std::cout << "<<<< -" << std::endl;
#endif
                if (!stub->unack_packet.size()) {
                    std::cerr << "WARNING: received negative acknowledgement for " <<
                        "unsent packet" << std::endl;
                } else {
#ifdef GDBSTUB_VERBOSE
                    std::cout << ">>>>" << stub->unack_packet << std::endl;
#endif
                    transmit(stub, stub->unack_packet);
                }
            } else if (c == '$') {
                // new packet
                stub->input_packet = '$';
            } else if (c == 3) {
                // user pressed ctrl+c (^C) on the gdb frontend
                std::cout << "GDBSTUB: user requested breakpoint (ctrl-C)" <<
                    std::endl;
                if (stub->dbg->cur_state == DEBUG_STATE_NORM) {
                    gdb_on_break(stub);
                    stub->dbg->cur_state = DEBUG_STATE_BREAK;
                }
            } else {
                std::cerr << "WARNING: ignoring unexpected character " << c <<
                    std::endl;
            }
        }
    }
}

/*
 * this function gets called when libevent is done writing
 * and is hungry for more data
 */
static void handle_write(struct bufferevent *bev, void *arg) {
    struct gdb_stub *stub = (struct gdb_stub*)arg;

    stub->is_writing = false;

    if (stub->output_queue.empty())
        return;

    write_start(stub);
}
