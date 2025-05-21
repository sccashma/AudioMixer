#ifndef __SERIAL__HPP__
#define __SERIAL__HPP__

#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <memory>
#include "stack.hpp"

namespace audio_mixer
{

    class serial_connection_c
    {
        using baud_rate_t = boost::asio::serial_port_base::baud_rate;

    public:
        serial_connection_c(boost::asio::io_context &, std::shared_ptr<stack_c>, baud_rate_t const &);

        ~serial_connection_c();

        bool try_connect_and_handshake(boost::asio::serial_port &, const std::string &);
        void run(bool &exit_app);

    private:
        void main_read_loop(bool &exit_app);

        boost::asio::io_context &m_context;
        boost::asio::serial_port m_serial;
        std::string m_port;
        baud_rate_t m_baud;
        std::shared_ptr<stack_c> m_data_stack;
    };

} // namespace audio_mixer

#endif // __SERIAL__HPP__
