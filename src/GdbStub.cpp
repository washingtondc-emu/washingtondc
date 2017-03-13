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

GdbStub::GdbStub() : Debugger(),
                     tcp_endpoint(boost::asio::ip::tcp::v4(),
                                  PORT_NO),
                     tcp_acceptor(io_service, tcp_endpoint),
                     tcp_socket(io_service) {
    frontend_supports_swbreak = false;
}

GdbStub::~GdbStub() {
}

void GdbStub::attach() {
    std::cout << "Awaiting remote GDB connection on port " << PORT_NO <<
        "..." << std::endl;
    tcp_acceptor.accept(tcp_socket);
    std::cout << "Connection established." << std::endl;
    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_buffer),
                            boost::bind(&GdbStub::handle_read, this,
                                        boost::asio::placeholders::error));
}

void GdbStub::on_break() {
    transmit_pkt(craft_packet(std::string("S05")));
}


void GdbStub::on_softbreak(inst_t inst, addr32_t addr) {
    std::stringstream pkt_txt;

    if (frontend_supports_swbreak)
        pkt_txt << "T05swbreak:" << std::hex << addr << ";";
    else
        pkt_txt << "S05";

    cur_state = STATE_BREAK;

    transmit_pkt(craft_packet(pkt_txt.str()));
}

void GdbStub::on_read_watchpoint(addr32_t addr) {
    std::stringstream pkt_txt;

    // pkt_txt << "T05rwatch:" << std::hex << addr << ";";
    pkt_txt << "S05";

    transmit_pkt(craft_packet(pkt_txt.str()));
}

void GdbStub::on_write_watchpoint(addr32_t addr) {
    std::stringstream pkt_txt;

    // pkt_txt << "T05watch:" << std::hex << addr << ";";
    pkt_txt << "S05";

    transmit_pkt(craft_packet(pkt_txt.str()));
}


std::string GdbStub::serialize_regs() const {
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

void GdbStub::deserialize_regs(std::string input_str, reg32_t regs[N_REGS]) {
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

std::string GdbStub::serialize_data(void const *buf, unsigned buf_len) {
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

int GdbStub::decode_hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 10;
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0';
    if ((ch >= 'A') && (ch <= 'F'))
        return ch - 'A' + 10;
    return -1;
}

bool GdbStub::step(addr32_t pc) {
    bool res = Debugger::step(pc);

    if (!res)
        io_service.poll();
    else
        io_service.run_one();

    return res;
}

void GdbStub::transmit(const std::string& data) {
    for (std::string::const_iterator it = data.begin(); it != data.end(); it++)
        output_queue.push(*it);

    write_start();
}

void GdbStub::write_start() {
    size_t idx = 0;

    if (is_writing || output_queue.empty())
        return;

    while (idx < write_buffer.size() && !output_queue.empty()) {
        write_buffer[idx++] = output_queue.front();
        output_queue.pop();
    }

    boost::asio::async_write(tcp_socket,
                             boost::asio::buffer(write_buffer.c_array(), idx),
                             boost::bind(&GdbStub::handle_write, this,
                                         boost::asio::placeholders::error));
}

/*
 * this function gets called when boost is done writing
 * and is hungry for more data
 */
void GdbStub::handle_write(const boost::system::error_code& error) {
    is_writing = false;

    if (output_queue.empty())
        return;

    write_start();
}

void GdbStub::handle_read(const boost::system::error_code& error) {

    if (error)
        return;
    for (unsigned i = 0; i < read_buffer.size(); i++) {
        input_queue.push(read_buffer.at(i));
    }

    while (input_queue.size()) {
        char c = input_queue.front();
        input_queue.pop();

        if (input_packet.size()) {
            input_packet += c;

            std::string pkt = next_packet();
            if (pkt.size()) {
                input_packet = "";

                // TODO: verify the checksum

#ifdef GDBSTUB_VERBOSE
                std::cout << ">>>> +" << std::endl;
#endif
                transmit(std::string("+"));
                handle_packet(pkt);
            }
        } else {
            if (c == '+') {
#ifdef GDBSTUB_VERBOSE
                std::cout << "<<<< +" << std::endl;
#endif
                if (!unack_packet.size())
                    std::cerr << "WARNING: received acknowledgement for unsent " <<
                        "packet" << std::endl;
                unack_packet = "";
            } else if (c == '-') {
#ifdef GDBSTUB_VERBOSE
                std::cout << "<<<< -" << std::endl;
#endif
                if (!unack_packet.size()) {
                    std::cerr << "WARNING: received negative acknowledgement for " <<
                        "unsent packet" << std::endl;
                } else {
#ifdef GDBSTUB_VERBOSE
                    std::cout << ">>>>" << unack_packet << std::endl;
#endif
                    transmit(unack_packet);
                }
            } else if (c == '$') {
                // new packet
                input_packet = '$';
            } else if (c == 3) {
                // user pressed ctrl+c (^C) on the gdb frontend
                std::cout << "GDBSTUB: user requested breakpoint (ctrl-C)" <<
                    std::endl;
                if (cur_state == STATE_NORM) {
                    on_break();
                    cur_state = STATE_BREAK;
                }
            } else {
                std::cerr << "WARNING: ignoring unexpected character " << c <<
                    std::endl;
            }
        }
    }

    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_buffer),
                            boost::bind(&GdbStub::handle_read, this,
                                        boost::asio::placeholders::error));
}

std::string GdbStub::craft_packet(std::string data_in) {
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

std::string GdbStub::extract_packet(std::string packet_in) {
    size_t dollar_idx = packet_in.find_first_of('$');
    size_t pound_idx = packet_in.find_last_of('#');

    return packet_in.substr(dollar_idx + 1, pound_idx - dollar_idx - 1);
}

int GdbStub::set_reg(reg32_t reg_file[SH4_REGISTER_COUNT], Sh4::FpuReg *fpu,
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

std::string GdbStub::err_str(unsigned err_val) {
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

void GdbStub::handle_c_packet(std::string dat) {
    cur_state = Debugger::STATE_NORM;
}

std::string GdbStub::handle_q_packet(std::string dat) {
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
                    frontend_supports_swbreak = true;
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

std::string GdbStub::handle_g_packet(std::string dat) {
    return serialize_regs();
}

std::string GdbStub::handle_m_packet(std::string dat) {
    size_t addr_idx = dat.find_last_of('m') + 1;
    size_t comma_idx = dat.find_last_of(',');
    size_t len_idx = comma_idx + 1;

    unsigned len;
    unsigned addr;
    std::stringstream(dat.substr(addr_idx, comma_idx)) >> std::hex >> addr;
    std::stringstream(dat.substr(len_idx)) >> std::hex >> len;

    if (len % 4 == 0)
        return read_mem_4(addr, len);
    else if (len % 2 == 0)
        return read_mem_2(addr, len);
    else
        return read_mem_1(addr, len);
}

std::string GdbStub::read_mem_4(addr32_t addr, unsigned len) {
    std::stringstream ss;
    while (len) {
        uint32_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr += 4;
        } catch (BaseException& exc) {
            // std::cerr << boost::diagnostic_information(exc);
            return err_str(EINVAL);
        }

        ss << serialize_data(&val, sizeof(val));
        len -= 4;
    }

    return ss.str();
}

std::string GdbStub::read_mem_2(addr32_t addr, unsigned len) {
    std::stringstream ss;
    while (len) {
        uint16_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr += 2;
        } catch (BaseException& exc) {
            // std::cerr << boost::diagnostic_information(exc);
            return err_str(EINVAL);
        }

        ss << serialize_data(&val, sizeof(val));
        len -= 2;
    }

    return ss.str();
}

std::string GdbStub::read_mem_1(addr32_t addr, unsigned len) {
    std::stringstream ss;
    while (len--) {
        uint8_t val;

        try {
            sh4_read_mem(dreamcast_get_cpu(), &val, addr, sizeof(val));
            addr++;
        } catch (BaseException& exc) {
            // std::cerr << boost::diagnostic_information(exc);
            return err_str(EINVAL);
        }

        // val = boost::endian::endian_reverse(val);
        ss << serialize_data(&val, sizeof(val));
    }

    return ss.str();
}

/*
 * TODO: bounds checking (not that I expect there to be any hackers going in
 * through the debugger of all places)
 */
std::string GdbStub::handle_M_packet(std::string dat) {
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
                deserialize_data(ss, buf, len);
                sh4_write_mem(dreamcast_get_cpu(), buf, addr, len);
            } catch (BaseException& exc) {
                delete[] buf;
                throw;
            }
            delete[] buf;
        } else {
            BOOST_THROW_EXCEPTION(InvalidParamError());
        }
    } catch (BaseException& exc) {
        std::cerr << boost::diagnostic_information(exc);
        return err_str(EINVAL);
    }

    return "OK";
}

void GdbStub::handle_s_packet(std::string dat) {
    cur_state = Debugger::STATE_PRE_STEP;
}

std::string GdbStub::handle_G_packet(std::string dat) {
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

std::string GdbStub::handle_P_packet(std::string dat) {
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

std::string GdbStub::handle_D_packet(std::string dat) {
    cur_state = Debugger::STATE_NORM;

    on_detach();

    return "OK";
}

std::string GdbStub::handle_K_packet(std::string dat) {
    dreamcast_kill();

    return "OK";
}

std::string GdbStub::handle_Z_packet(std::string dat) {
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

        int err_code = add_break(break_addr);

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

        int err_code = add_w_watch(watch_addr, length);

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

        int err_code = add_r_watch(watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else {
        // unsupported
        return std::string();
    }
}

std::string GdbStub::handle_z_packet(std::string dat) {
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

        int err_code = remove_break(break_addr);

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

        int err_code = remove_w_watch(watch_addr, length);

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

        int err_code = remove_r_watch(watch_addr, length);

        if (err_code == 0)
            return std::string("OK");

        return err_str(err_code);
    } else {
        // unsupported
        return std::string();
    }
}

void GdbStub::handle_packet(std::string pkt) {
    std::string response;
    std::string dat = extract_packet(pkt);

    response = craft_packet(std::string());

    if (dat.size()) {
        if (dat.at(0) == 'q') {
            response = craft_packet(handle_q_packet(dat));
        } else if (dat.at(0) == 'g') {
            response = craft_packet(handle_g_packet(dat));
        } else if (dat.at(0) == 'G') {
            response = craft_packet(handle_G_packet(dat));
        } else if (dat.at(0) == 'm') {
            response = craft_packet(handle_m_packet(dat));
        } else if (dat.at(0) == 'M') {
            response = craft_packet(handle_M_packet(dat));
        } else if (dat.at(0) == '?') {
            response = craft_packet(std::string("S05 create:"));
        } else if (dat.at(0) == 's') {
            handle_s_packet(dat);
            return;
        } else if (dat.at(0) == 'c') {
            handle_c_packet(dat);
            return;
        } else if (dat.at(0) == 'P') {
            response = craft_packet(handle_P_packet(dat));
        } else if (dat.at(0) == 'D') {
            response = craft_packet(handle_D_packet(dat));
        } else if (dat.at(0) == 'k') {
            response = craft_packet(handle_K_packet(dat));
        } else if (dat.at(0) == 'z') {
            response = craft_packet(handle_z_packet(dat));
        } else if (dat.at(0) == 'Z') {
            response = craft_packet(handle_Z_packet(dat));
        }
    }

    transmit_pkt(response);
}

void GdbStub::transmit_pkt(const std::string& pkt) {
#ifdef GDBSTUB_VERBOSE
    std::cout << ">>>> " << pkt << std::endl;
#endif

    unack_packet = pkt;
    transmit(pkt);
}

std::string GdbStub::next_packet() {
    std::string pkt;
    std::string pktbuf_tmp(input_packet);
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

    input_packet = pktbuf_tmp;

#ifdef GDBSTUB_VERBOSE
    std::cout << "<<<< " << pkt << std::endl;
#endif

    return pkt;
}
