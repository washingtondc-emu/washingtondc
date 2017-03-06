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

#ifndef GDBSTUB_H_
#define GDBSTUB_H_

#ifndef ENABLE_DEBUGGER
#error This file should not be included unless the debugger is enabled
#endif

#include <string>
#include <sstream>
#include <queue>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

#include "common/BaseException.hpp"
#include "Debugger.hpp"
#include "types.hpp"
#include "hw/sh4/sh4.hpp"

class GdbStub : public Debugger {
public:
    // it's 'cause 1999 is the year the Dreamcast came out in America
    static const unsigned PORT_NO = 1999;

    GdbStub();
    ~GdbStub();

    void attach();

    bool step(addr32_t pc);

    void on_break();
    void on_read_watchpoint(addr32_t addr);
    void on_write_watchpoint(addr32_t addr);

    void on_softbreak(inst_t inst, addr32_t addr);

    // see sh_sh4_register_name in gdb/sh-tdep.c in the gdb source code
    enum RegOrder {
        R0, R1, R2, R3, R4, R5, R6, R7,
        R8, R9, R10, R11, R12, R13, R14, R15,

        PC, PR, GBR, VBR, MACH, MACL, SR, FPUL, FPSCR,

        FR0, FR1, FR2, FR3, FR4, FR5, FR6, FR7,
        FR8, FR9, FR10, FR11, FR12, FR13, FR14, FR15,

        SSR, SPC,

        R0B0, R1B0, R2B0, R3B0, R4B0, R5B0, R6B0, R7B0,
        R0B1, R1B1, R2B1, R3B1, R4B1, R5B1, R6B1, R7B1,

        N_REGS
    };

private:
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::endpoint tcp_endpoint;
    boost::asio::ip::tcp::acceptor tcp_acceptor;
    boost::asio::ip::tcp::socket tcp_socket;

    bool is_writing;
    boost::array<char, 1> read_buffer;
    boost::array<char, 128> write_buffer;

    std::queue<char> input_queue;
    std::queue<char> output_queue;

    // the last unsuccessfully acknowledged packet, or empty if there is none
    std::string unack_packet;

    std::string input_packet;

    bool frontend_supports_swbreak;

    // enqueue data for transmit
    void transmit(const std::string& data);

    // set unack_packet = pkt and transmit
    void transmit_pkt(const std::string& pkt);

    // schedule queued data for transmission
    void write_start();

    std::string next_packet();

    void handle_packet(std::string pkt);
    std::string craft_packet(std::string data_in);
    std::string extract_packet(std::string packet_in);

    static std::string err_str(unsigned err_val);

    static int decode_hex(char ch);
    
    std::string serialize_regs() const;

    static std::string serialize_data(void const *buf, unsigned buf_len);

    template<class Stream>
    size_t deserialize_data(Stream& input, void *out, size_t max_sz) {
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

    void deserialize_regs(std::string input_str, reg32_t regs[N_REGS]);

    // returns 0 on success, 1 on failure
    int set_reg(reg32_t reg_file[SH4_REGISTER_COUNT], Sh4::FpuReg *fpu,
                unsigned reg_no, reg32_t reg_val, bool bank);

    void handle_read(const boost::system::error_code& error);

    void handle_write(const boost::system::error_code& error);

    void handle_c_packet(std::string dat);
    void handle_s_packet(std::string dat);
    std::string handle_g_packet(std::string dat);
    std::string handle_m_packet(std::string dat);
    std::string handle_q_packet(std::string dat);
    std::string handle_z_packet(std::string dat);
    std::string handle_G_packet(std::string dat);
    std::string handle_M_packet(std::string dat);
    std::string handle_P_packet(std::string dat);
    std::string handle_D_packet(std::string dat);
    std::string handle_K_packet(std::string dat);
    std::string handle_Z_packet(std::string dat);

    /*
     * read memory in 4, 2, or 1 byte increments and return it as a hex-string.
     * len must be evenly-divisible by 4/2/1.
     */
    std::string read_mem_4(addr32_t addr, unsigned len);
    std::string read_mem_2(addr32_t addr, unsigned len);
    std::string read_mem_1(addr32_t addr, unsigned len);
};

#endif
