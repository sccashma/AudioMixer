#ifndef __AUDIO__MIXER___HPP__
#define __AUDIO__MIXER___HPP__

#include <boost/asio.hpp>
#include <map>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "endpoint.hpp"
#include "stack.hpp"
#ifdef _WIN32
#include "windows_media_interface.hpp"
#endif

namespace audio_mixer
{
    class audio_mixer_c
    {
        using baud_rate_t = boost::asio::serial_port_base::baud_rate;

    public:
        audio_mixer_c(boost::asio::io_context &context);

        // TODO: implement config stack
        void load_configs();

        std::shared_ptr<stack_c> get_data_stack() const;

        uint16_t get_data_rate() const;

        baud_rate_t get_baud_rate() const;

        void run(bool &exit_app);
        void update(std::vector<int> const &values);

    private:
        boost::asio::io_context &m_context;
        std::shared_ptr<stack_c> m_data_stack;
#ifdef _WIN32
        audio_mixer::windows_media_interface_c m_media;
#endif
        baud_rate_t m_baud_rate;
        uint16_t m_data_rate_ms;
        std::regex m_data_pattern;
        uint16_t m_num_of_knobs;
        std::vector<endpoint> m_endpoints;

        void update_volumes(std::vector<int> const &values);
        std::regex create_regex(uint16_t count);
        std::vector<int> extract_values(std::string &values);
        std::vector<float> scale_values(std::vector<int> const &values);

    }; // end class audio_mixer_c

} // namespace audio_mixer

#endif // __AUDIO__MIXER___HPP__
