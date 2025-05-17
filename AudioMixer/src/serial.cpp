#include "serial.hpp"

#include "logger.hpp"

namespace audio_mixer {

serial_connection_c::serial_connection_c(
    boost::asio::io_context& context,
    std::shared_ptr<stack_c> stack,
    std::string const& port,
    baud_rate_t const& baud,
    uint16_t const& rate)
: m_context(context),
m_serial(context),
m_data_stack(stack),
m_port(port),
m_baud(baud),
m_rate(rate)
{
    while (!m_serial.is_open()) {
        connect();
    }
}

serial_connection_c::~serial_connection_c()
{
    m_serial.close();
    m_context.stop();
}

void serial_connection_c::run(bool& exit_app)
{
    read(exit_app);
}

void serial_connection_c::connect()
{
    while (true) {
        try {
            m_serial.open(this->m_port);
            m_serial.set_option(baud_rate_t(this->m_baud));
            m_serial.set_option(boost::asio::serial_port::character_size(8));
            m_serial.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none));
            m_serial.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one));
            m_serial.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none));
            audio_mixer::log("Opened: " + m_port);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        } catch (const boost::system::system_error& e) {
            audio_mixer::log_error(std::string("Error opening serial port: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}

void serial_connection_c::read(bool& exit_app)
{
    if (!exit_app) {
        boost::asio::async_read_until(m_serial, m_buffer, "\r",
            [this, &exit_app](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (!ec) {
                    std::istream is(&m_buffer);
                    std::string line;
                    std::getline(is, line);

                    // Remove trailing '\r' if present
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }

                    m_data_stack->push(line);
                    m_buffer.consume(bytes_transferred);

                    // Continue reading
                    read(exit_app);

                }
                else {
                    audio_mixer::log_error(ec.message());
                }
            });
    }
}

}  // namespace audio_mixer
