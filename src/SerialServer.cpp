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

#include <iostream>

#include <boost/bind.hpp>

#include "Dreamcast.hpp"

#include "SerialServer.hpp"

#ifndef ENABLE_SERIAL_SERVER
#error This file should not be built unless the serial server is enabled
#endif

SerialServer::SerialServer(Sh4 *cpu) : tcp_endpoint(boost::asio::ip::tcp::v4(),
                                                    PORT_NO),
                                       tcp_acceptor(dc_io_service,
                                                    tcp_endpoint),
                                       tcp_socket(dc_io_service) {
    is_writing = false;
    this->cpu = cpu;
}

SerialServer::~SerialServer() {
    /*
     * IDK, I just don't feel comfortable not having this
     * even if it doesn't do anything
     */
}

void SerialServer::attach() {
    std::cout << "Awaiting serial connection on port " << PORT_NO << "..." <<
        std::endl;
    tcp_acceptor.accept(tcp_socket);
    std::cout << "Connection established." << std::endl;
    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_buffer),
                            boost::bind(&SerialServer::handle_read, this,
                                        boost::asio::placeholders::error));
}

void SerialServer::handle_read(const boost::system::error_code& error) {
    if (error)
        return;

    // TODO: input_queue doesn't actually need to exist, does it ?
    for (unsigned idx = 0; idx < read_buffer.size(); idx++)
        input_queue.push(read_buffer.at(idx));

    // now send the data to the SCIF one char at a time
    while (input_queue.size()) {
        sh4_scif_rx(cpu, input_queue.front());
        input_queue.pop();
    }

    boost::asio::async_read(tcp_socket, boost::asio::buffer(read_buffer),
                            boost::bind(&SerialServer::handle_read, this,
                                        boost::asio::placeholders::error));
}

void SerialServer::write_start() {
    size_t idx = 0;

    if (is_writing || output_queue.empty())
        return;

    while (idx < write_buffer.size() && !output_queue.empty()) {
        write_buffer[idx++] = output_queue.front();
        output_queue.pop();
    }

    boost::asio::async_write(tcp_socket,
                             boost::asio::buffer(write_buffer.c_array(), idx),
                             boost::bind(&SerialServer::handle_write, this,
                                         boost::asio::placeholders::error));
}

/*
 * this function gets called when boost is done writing
 * and is hungry for more data
 */
void SerialServer::handle_write(const boost::system::error_code& error) {
    is_writing = false;

    if (output_queue.empty()) {
        sh4_scif_cts(cpu);

        if (output_queue.empty())
            return;
    }

    write_start();
}

void SerialServer::put(uint8_t dat) {
    output_queue.push(dat);

    write_start();
}

void SerialServer::notify_tx_ready() {
    if (output_queue.empty())
        sh4_scif_cts(cpu);
}
