#ifndef __SERIAL__HPP__
#define __SERIAL__HPP__

// Libraries
#include <iostream>
#include <boost/asio.hpp>
#include <thread>

// Local libraries
#include "stack.hpp"

namespace audio_mixer {
class serial_connection_c
{
    using baud_rate_t = boost::asio::serial_port_base::baud_rate;
public:
    serial_connection_c(
        boost::asio::io_context& context,
        std::shared_ptr<stack_c> stack,
        std::string const& port,
        baud_rate_t const& baud,
        uint16_t const& rate);

    ~serial_connection_c();

    void run(bool& exit_app);

private:
    void connect();

    void read(bool& exit_app);

    boost::asio::io_context& m_context;
    boost::asio::serial_port m_serial;
    std::string m_port;
    baud_rate_t m_baud;
    boost::asio::streambuf m_buffer;
    std::shared_ptr<stack_c> m_data_stack;
    uint16_t m_rate; // May be unused
};

}  // namespace audio_mixer

#endif  // __SERIAL__HPP__
