/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef SERIALSERVER_HPP_
#define SERIALSERVER_HPP_

#include <queue>

#include <boost/cstdint.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>

#include "hw/sh4/sh4.hpp"

#ifndef ENABLE_SERIAL_SERVER
#error This file should not be included unless the serial server is enabled
#endif

class SerialServer {
public:
    // it's 'cause 1998 is the year the Dreamcast came out in Japan
    static const unsigned PORT_NO = 1998;

    SerialServer(Sh4 *cpu);
    ~SerialServer();

    void attach();

    void put(uint8_t dat);

    /*
     * The SCIF calls this to let us know that it has data ready to transmit.
     * If the SerialServer is idling, it will immediately call sh4_scif_cts, and the
     * sh4 will send the data to the SerialServer via the SerialServer's put method
     *
     * If the SerialServer is active, this function does nothing and the server will call
     * sh4_scif_cts later when it is ready.
     */
    void notify_tx_ready();

private:
    boost::asio::ip::tcp::endpoint tcp_endpoint;
    boost::asio::ip::tcp::acceptor tcp_acceptor;
    boost::asio::ip::tcp::socket tcp_socket;

    boost::array<uint8_t, 1> read_buffer;
    boost::array<uint8_t, 128> write_buffer;

    std::queue<uint8_t> input_queue;
    std::queue<uint8_t> output_queue;

    bool is_writing;

    Sh4 *cpu;

    void handle_read(const boost::system::error_code& error);
    void handle_write(const boost::system::error_code& error);

    // schedule queued data for transmission
    void write_start();
};

#endif
