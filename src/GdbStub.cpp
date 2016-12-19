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

#include <sstream>
#include <iostream>
#include <iomanip>

#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>

#include "hw/sh4/sh4.hpp"
#include "Dreamcast.hpp"

#include "GdbStub.hpp"

// uncomment this to log all traffic in/out of the debugger to stdout
// #define GDBSTUB_VERBOSE

GdbStub::GdbStub(Dreamcast *dc) : Debugger(dc),
                                  tcp_acceptor(io_service, tcp_endpoint),
                                  tcp_endpoint(boost::asio::ip::tcp::v4(),
                                               PORT_NO),
                                  tcp_socket(io_service)
{
    this->dc = dc;
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

std::string GdbStub::serialize_regs() const {
    Sh4 *cpu = dc->get_cpu();
    Sh4::RegFile reg_file = cpu->get_regs();
    reg32_t regs[N_REGS] = { 0 };

    // general-purpose registers
    for (int i = 0; i < 16; i++) {
        if (i < 8) {
            if (reg_file.sr & Sh4::SR_RB_MASK)
                regs[R0 + i] = reg_file.r_bank1[i];
            else
                regs[R0 + i] = reg_file.r_bank0[i];
        }else {
            regs[R0 + i] = reg_file.rgen[i - 8];
        }
    }

    // banked registers
    for (int i = 0; i < 8; i++) {
        regs[R0B0 + i] = reg_file.r_bank0[i];
        regs[R0B1 + i] = reg_file.r_bank1[i];
    }

    // TODO: floating point registers

    // system/control registers
    regs[PC] = reg_file.pc;
    regs[PR] = reg_file.pr;
    regs[GBR] = reg_file.gbr;
    regs[VBR] = reg_file.vbr;
    regs[MACH] = reg_file.mach;
    regs[MACL] = reg_file.macl;
    regs[SR] = reg_file.sr;
    regs[SSR] = reg_file.ssr;
    regs[SPC] = reg_file.spc;

    // FPU system/control registers
    regs[FPUL] = reg_file.fpul;
    regs[FPSCR] = reg_file.fpscr;

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

bool GdbStub::step(inst_t pc) {
    bool res = Debugger::step(pc);

    io_service.poll();

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
                input_packet += c;
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

int GdbStub::set_reg(Sh4::RegFile *file, unsigned reg_no,
                      reg32_t reg_val, bool bank) {
    // there is some ambiguity over whether register banking should be based off
    // of the old sr or the new sr.  For now, it's based off of the old sr.

    // TODO: floating point registers
    if (reg_no >= R0 && reg_no <= R15) {
        unsigned idx = reg_no - R0;

        if (idx < 8) {
            if (bank)
                file->r_bank1[idx] = reg_val;
            else
                file->r_bank0[idx] = reg_val;
        } else {
            file->rgen[idx - 8] = reg_val;
        }
    } else if (reg_no >= R0B0 && reg_no <= R7B0) {
        file->r_bank0[reg_no - R0B0] = reg_val;
    } else if (reg_no >= R0B1 && reg_no <= R7B1) {
        file->r_bank1[reg_no - R0B1] = reg_val;
    } else if (reg_no == PC) {
        file->pc = reg_val;
    } else if (reg_no == PR) {
        file->pr = reg_val;
    } else if (reg_no == GBR) {
        file->gbr = reg_val;
    } else if (reg_no == VBR) {
        file->vbr = reg_val;
    } else if (reg_no == MACH) {
        file->mach = reg_val;
    } else if (reg_no == MACL) {
        file->macl = reg_val;
    } else if (reg_no == SR) {
        file->sr = reg_val;
    } else if (reg_no == SSR) {
        file->ssr = reg_val;
    } else if (reg_no == SPC) {
        file->spc = reg_val;
    } else if (reg_no == FPUL) {
        file->fpul = reg_val;
    } else if (reg_no == FPSCR) {
        file->fpscr = reg_val;
    } else {
#ifdef GDBSTUB_VERBOSE
        std::cout << "WARNING: GdbStub unable to set value of register " <<
            std::hex << reg_no << " to " << reg_val << std::endl;
#endif
        return 1;
    }

    return 0;
}

void GdbStub::handle_c_packet(std::string dat) {
    cur_state = Debugger::STATE_NORM;
}

std::string GdbStub::handle_q_packet(std::string dat) {
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

    std::stringstream ss;
    while (len--) {
        uint8_t val;

        // for now don't cooperate if it wants memory-mapped registers
        if (addr >= 0xe0000000)
            return std::string("E01"); // EINVAL
        else
            dc->get_cpu()->read_mem(&val, addr++, sizeof(val));
        ss << std::setfill('0') << std::setw(2) << std::hex << unsigned(val);
    }

    return ss.str();
}

void GdbStub::handle_s_packet(std::string dat) {
    cur_state = Debugger::STATE_PRE_STEP;
}

std::string GdbStub::handle_G_packet(std::string dat) {
    reg32_t regs[N_REGS];

    deserialize_regs(dat.substr(1), regs);

    Sh4::RegFile new_regs = dc->get_cpu()->get_regs();
    bool bank = new_regs.sr & Sh4::SR_RB_MASK;

    for (unsigned reg_no = 0; reg_no < N_REGS; reg_no++)
        set_reg(&new_regs, reg_no, regs[reg_no], bank);
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
    deserialize_data<std::stringstream>(reg_val_stream, &reg_val, sizeof(reg_val));

    if (reg_no >= N_REGS) {
#ifdef GDBSTUB_VERBOSE
        std::cout << "ERROR: unable to write to register number " <<
            std::hex << reg_no << std::endl;
#endif
        return "E16";
    }

    Sh4::RegFile regs = dc->get_cpu()->get_regs();
    set_reg(&regs, reg_no, reg_val, bool(regs.sr & Sh4::SR_RB_MASK));
    dc->get_cpu()->set_regs(regs);

    return "OK";
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

    unsigned addr = atoi(dat.substr(addr_idx, comma_idx).c_str());
    unsigned len = atoi(dat.substr(len_idx, colon_idx).c_str());

    std::stringstream ss;
    while (len--) {
        uint8_t val =
            uint8_t(atoi(dat.substr(dat_idx, dat_idx + 1).c_str()) << 4) |
            uint8_t(atoi(dat.substr(dat_idx + 1, dat_idx + 2).c_str()));

        dc->get_cpu()->write_mem(&val, addr++, sizeof(val));
    }

    return "OK";
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

    unsigned char checksum;
    unsigned char xmitcsum;
    int count;
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
