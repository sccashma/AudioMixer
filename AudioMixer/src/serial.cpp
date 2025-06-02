#include "serial.hpp"
#include "logger.hpp"

#ifdef _WIN32
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace audio_mixer
{

    namespace
    {
        const std::string HEARTBEAT = "AUDIOMIXER_V1_HEARTBEAT";
        const std::string HANDSHAKE_KEY = "AUDIOMIXER_HELLO";
        const std::string HANDSHAKE_RESPONSE = "AUDIOMIXER_READY";
        constexpr int HANDSHAKE_TIMEOUT_MS = 1000;
        constexpr int HEARTBEAT_TIMEOUT_MS = 1500;
    } // namespace

    // Cross-platform serial port enumeration
    std::vector<std::string> list_serial_ports()
    {
        std::vector<std::string> ports;
#ifdef _WIN32
        HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);
        if (hDevInfo == INVALID_HANDLE_VALUE)
            return ports;

        SP_DEVINFO_DATA DeviceInfoData;
        DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); ++i)
        {
            char buf[256];
            if (SetupDiGetDeviceRegistryPropertyA(
                hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, nullptr, (PBYTE)buf, sizeof(buf), nullptr))
            {
                std::string name(buf);
                auto pos = name.find(" (COM");
                if (pos != std::string::npos)
                {
                    auto start = name.find("COM", pos);
                    auto end = name.find(")", start);
                    if (start != std::string::npos && end != std::string::npos)
                    {
                        ports.emplace_back(name.substr(start, end - start));
                        audio_mixer::log_debug("Found serial port: " + ports.back());
                    }
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
#else
        // INFO: UNTESTED: This code is for Linux/macOS
        const char *dirs[] = {"/dev/"};
        const char *prefixes[] = {"ttyACM", "ttyUSB", "cu.usb"};
        for (auto dir : dirs)
        {
            DIR *dp = opendir(dir);
            if (!dp)
                continue;
            struct dirent *ep;
            while ((ep = readdir(dp)))
            {
                for (auto prefix : prefixes)
                {
                    if (strncmp(ep->d_name, prefix, strlen(prefix)) == 0)
                    {
                        ports.emplace_back(std::string(dir) + ep->d_name);
                    }
                }
            }
            closedir(dp);
        }
#endif
        return ports;
    }

    // Read a line with timeout
    // This function uses async_read_until and a timer to implement the timeout
    bool read_line_with_timeout(
        boost::asio::io_context &io_ctx, boost::asio::serial_port &serial, std::string &line, int timeout_ms)
    {
        using namespace boost;
        auto buf = std::make_shared<asio::streambuf>();
        auto timer = std::make_shared<asio::deadline_timer>(io_ctx);
        auto data_ready = std::make_shared<bool>(false);
        auto ec = std::make_shared<system::error_code>();

        asio::async_read_until(serial, *buf, "\n",
                               [ec, data_ready, timer](const system::error_code &error, std::size_t)
                               {
                                   *ec = error;
                                   *data_ready = true;
                                   timer->cancel();
                               });

        timer->expires_from_now(boost::posix_time::milliseconds(timeout_ms));
        timer->async_wait(
            [data_ready, &serial](const system::error_code &error)
            {
                if (!error && !*data_ready)
                {
                    serial.cancel();
                }
            });

        io_ctx.reset();
        io_ctx.run();

        if (*data_ready && !*ec)
        {
            std::istream is(buf.get());
            std::getline(is, line);
            return true;
        }
        return false;
    }

    // Update try_connect_and_handshake to use an open serial port:
    bool serial_connection_c::try_connect_and_handshake(boost::asio::serial_port &serial, const std::string &port)
    {
        boost::asio::io_context io_ctx;
        std::string line;
        if (read_line_with_timeout(io_ctx, serial, line, HANDSHAKE_TIMEOUT_MS))
        {
            audio_mixer::log_debug("Received handshake line: " + line);
            if (line.find(HANDSHAKE_KEY) != std::string::npos)
            {
                std::string response = HANDSHAKE_RESPONSE + "\n";
                boost::asio::write(serial, boost::asio::buffer(response));
                audio_mixer::log_info("Handshake successful on port: " + port);
                return true;
            }
            else
            {
                audio_mixer::log_debug("Invalid handshake response on port: " + port);
            }
        }
        return false;
    }

    // Constructor/Destructor
    serial_connection_c::serial_connection_c(
        boost::asio::io_context &context, std::shared_ptr<stack_c> stack, baud_rate_t const &baud)
        : m_context(context), m_serial(context), m_data_stack(stack), m_port(""), m_baud(baud)
    {
        // Connection handled in run()
    }

    serial_connection_c::~serial_connection_c()
    {
        if (m_serial.is_open())
            m_serial.close();
        m_context.stop();
    }

    // Main read loop: wait for heartbeat and process data
    void serial_connection_c::main_read_loop(bool &exit_app)
    {
        auto last_heartbeat = std::chrono::steady_clock::now();
        boost::asio::streambuf buf;
        std::istream is(&buf);

        bool disconnected_logged = false;

        while (!exit_app && m_serial.is_open())
        {
            boost::system::error_code ec;
            size_t n = boost::asio::read_until(m_serial, buf, "\n", ec);
            if (ec)
            {
                audio_mixer::log_error("main_read_loop: read_until error: " + ec.message());
                break;
            }

            std::string line;
            std::getline(is, line);

            if (line.find(HEARTBEAT) != std::string::npos)
            {
                std::string response = HEARTBEAT + "\n";
                boost::asio::write(m_serial, boost::asio::buffer(response));
                last_heartbeat = std::chrono::steady_clock::now();
                audio_mixer::log_debug("Heartbeat received from serial port: " + m_port + " at:" 
                                       + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             last_heartbeat.time_since_epoch())
                                             .count()));
                disconnected_logged = false; // Reset on heartbeat
            }
            else
            {
                // remove trailing newline
                line.erase(line.find_last_not_of("\r\n") + 1); // Removes trailing \r or \n
                audio_mixer::log_debug("Data received from serial port: " + m_port + " - " + line);
                m_data_stack->push(line);
            }

            if (std::chrono::steady_clock::now() - last_heartbeat > std::chrono::milliseconds(HEARTBEAT_TIMEOUT_MS))
            {
                if (!disconnected_logged)
                {
                    audio_mixer::log_warning("Lost heartbeat, serial device disconnected from port: " + m_port);
                    disconnected_logged = true;
                }
                else
                {
                    // TODO: Confirming if this check is actually neeeded.
                    audio_mixer::log_debug("logging disconnection \"disconnected_logged\" required!");
                }
                break;
            }
        }
        if (m_serial.is_open())
        {
            m_serial.close();
            audio_mixer::log_error("Serial port closed: " + m_port + (exit_app ? ". Exiting application." : ""));
        }
    }

    // Main run() function: scan, connect, and maintain connection
    void serial_connection_c::run(bool &exit_app)
    {
        std::string last_connected_port;
        while (!exit_app)
        {
            bool connected = false;
            for (auto const &port : list_serial_ports())
            {
                try
                {
                    if (m_serial.is_open())
                        m_serial.close(); // Ensure clean state
                    audio_mixer::log_debug("Trying to connect to port: " + port);
                    m_serial.open(port);
                    m_serial.set_option(m_baud);
                    m_serial.set_option(boost::asio::serial_port::character_size(8));
                    m_serial.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none));
                    m_serial.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one));
                    m_serial.set_option(
                        boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none));
                    std::this_thread::sleep_for(std::chrono::seconds(2));

                    if (try_connect_and_handshake(m_serial, port))
                    {
                        m_port = port;
                        connected = true;
                        if (last_connected_port != port)
                        {
                            last_connected_port = port;
                        }
                        audio_mixer::log_info("Connected to serial port: " + port);
                        break;
                    }
                    m_serial.close();
                }
                catch (const std::exception &ex)
                {
                    audio_mixer::log_warning("Exception opening port " + port + ": " + ex.what());
                    if (m_serial.is_open())
                    {
                        audio_mixer::log_warning("Failed to connect to port: " + port + ". Closing serial port.");
                        m_serial.close();
                    }
                }
            }
            if (!connected)
            {
                if (!last_connected_port.empty())
                {
                    last_connected_port.clear();
                }
                m_port.clear(); // Clear current port on disconnect
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            // --- Data/heartbeat phase ---
            main_read_loop(exit_app);

            m_port.clear(); // Clear current port after disconnect
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
} // namespace audio_mixer
