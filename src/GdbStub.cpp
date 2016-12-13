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

GdbStub::GdbStub(Dreamcast *dc) : Debugger(dc),
                                  tcp_acceptor(io_service, tcp_endpoint),
                                  tcp_endpoint(boost::asio::ip::tcp::v4(),
                                               PORT_NO),
                                  tcp_socket(io_service)//,
                                  // tcp_buffer(read_buffer, TCP_BUFFER_LEN) 
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
                            boost::bind(&GdbStub::handle_read, this, boost::asio::placeholders::error));
}

std::string GdbStub::serialize_regs() const {
    Sh4 *cpu = dc->get_cpu();
    Sh4::RegFile reg_file = cpu->get_regs();
    reg32_t regs[N_REGS] = { 0 };

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

    regs[PC] = reg_file.pc;
    regs[PR] = reg_file.pr;
    regs[GBR] = reg_file.gbr;
    regs[VBR] = reg_file.vbr;
    regs[MACH] = reg_file.mach;
    regs[MACL] = reg_file.macl;
    regs[SR] = reg_file.sr;

    return serialize_data(regs, sizeof(regs));
}

void GdbStub::deserialize_regs(std::string input_str, reg32_t regs[N_REGS]) {
    std::stringstream stream(input_str);
    size_t sz_expect = N_REGS * sizeof(*regs);

    size_t sz_actual = deserialize_data<std::stringstream>(stream, regs,
                                                           sz_expect);

    if (sz_expect != sz_actual) {
        // TODO: better error messages
        throw IntegrityError("Some shit with gdb i guess");
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

    for (unsigned i = 0; i < buf_len; i++)
        ss << hex_tbl[(*buf8) >> 4] << hex_tbl[(*buf8) & 0xf];

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

void GdbStub::step(inst_t pc) {
    Debugger::step(pc);

    io_service.run();
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
        // std::cout << "sending char " << write_buffer[idx - 1] << std::endl;
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
        // std::cout << "received char " << read_buffer.at(i) << std::endl;
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
                transmit(std::string("+"));// TODO: checksum
                handle_packet(pkt);
            }
        } else {
            if (c == '+') {
                if (!unack_packet.size())
                    std::cerr << "WARNING: received acknowledgement for unsent " <<
                        "packet" << std::endl;
                unack_packet = "";
            } else if (c == '-') {
                if (!unack_packet.size()) {
                    std::cerr << "WARNING: received negative acknowledgement for " <<
                        "unsent packet" << std::endl;
                } else {
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

std::string GdbStub::handle_q_packet(std::string dat) {
    if (dat == "qC")
        return std::string("QC0");
    // boost::char_delimiters_separator<char> sep(';');
    // boost::tokenizer<> tokenizer(dat.substr(dat.find_last_of(':') + 1), sep);
    // std::string response("stubfeature ");

    // for (boost::tokenizer<>::iterator it = tokenizer.begin();
    //      it != tokenizer.end(); ++it) {
        
    //     response +=
    // }

    return std::string();
}

std::string GdbStub::handle_g_packet(std::string dat) {
    return serialize_regs();
}

std::string GdbStub::handle_m_packet(std::string dat) {
    size_t addr_idx = dat.find_last_of('m') + 1;
    size_t comma_idx = dat.find_last_of(',');
    size_t len_idx = comma_idx + 1;

    unsigned addr = atoi(dat.substr(addr_idx, comma_idx).c_str());
    unsigned len = atoi(dat.substr(len_idx).c_str());

    std::stringstream ss;
    while (len--) {
        uint8_t val;

        dc->get_cpu()->read_mem(&val, addr++, sizeof(val));
        ss << std::setfill('0') << std::setw(2) << unsigned(val);
    }

    return ss.str();
}

std::string GdbStub::handle_G_packet(std::string dat) {
    reg32_t regs[N_REGS];

    deserialize_regs(dat.substr(1), regs);

    Sh4::RegFile new_regs, old_regs = dc->get_cpu()->get_regs();

    // there is some ambiguity over whether register banking should be based off
    // of the old sr or the new sr.  For now, it's based off of the old sr.
    for (int i = 0; i < 16; i++) {
        if (i < 8) {
            if (old_regs.sr & Sh4::SR_RB_MASK)
                new_regs.r_bank1[i] = regs[R0 + i];
            else
                new_regs.r_bank0[i] = regs[R0 + i];
        }else {
            new_regs.rgen[i - 8] = regs[R0 + i];
        }
    }

    new_regs.pc = regs[PC];
    new_regs.pr = regs[PR];
    new_regs.gbr = regs[GBR];
    new_regs.vbr = regs[VBR];
    new_regs.mach = regs[MACH];
    new_regs.macl = regs[MACL];
    new_regs.sr = regs[SR];

    // return serialize_data(regs, sizeof(regs));

    dc->get_cpu()->set_regs(new_regs);


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
    std::cout << "data received " << dat << std::endl;

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
            response = craft_packet(std::string("S00"));
        }
    }

    // std::cout << "response outgoing " << response << std::endl;
    transmit_pkt(response);
}

void GdbStub::transmit_pkt(const std::string& pkt) {
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

    // std::cout << "packet received: \"" << pkt << "\"" << std::endl;

    return pkt;
}
