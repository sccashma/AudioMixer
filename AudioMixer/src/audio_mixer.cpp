
#include "audio_mixer.hpp"

#include <filesystem>
#include <yaml-cpp/yaml.h>

#include "logger.hpp"


namespace audio_mixer {

audio_mixer_c::audio_mixer_c(boost::asio::io_context& context)
    : m_context(context),
      m_data_stack(std::make_shared<stack_c>()),
      m_num_of_knobs(5),
      m_baud_rate(9600U),
      m_data_rate_ms(50U),
      m_port("COM12")
{
    m_data_stack = std::make_shared<stack_c>();
    load_configs();

    // Start the context
    std::thread con_thread(
        [this](){
            boost::asio::io_context::work work(m_context);
            m_context.run();});
    con_thread.detach();
}

void audio_mixer_c::load_configs()
{
    // Find path to executable
    std::string exe_path;
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    exe_path = exePath;
    exe_path = exe_path.substr(0, exe_path.find_last_of("\\/") + 1);
#else
    exe_path = "./";
#endif
    std::string config_path = exe_path + "config.yaml";

    try {
        YAML::Node config = YAML::LoadFile(config_path);

        m_num_of_knobs = config["num_of_knobs"].as<uint16_t>(5);
        m_baud_rate = baud_rate_t(config["baud_rate"].as<uint32_t>(9600));
        m_data_rate_ms = config["data_rate_ms"].as<uint16_t>(50);
        m_port = config["port"].as<std::string>("COM12");
        m_data_pattern = create_regex(m_num_of_knobs);

        m_endpoints.clear();
        if (config["endpoints"]) {
            for (const auto& ep : config["endpoints"]) {
                m_endpoints.emplace_back(endpoint(ep.as<std::string>()));
            }
        }

        for (auto& app : m_endpoints) {
            audio_mixer::log("Loaded: " + app.name);
        }
    } catch (const std::exception& e) {
        audio_mixer::log_error(std::string("Failed to load config.yaml: ") + e.what());
        // Fallback to defaults if needed...
        m_num_of_knobs = 5U;
        m_baud_rate = baud_rate_t(9600U);
        m_data_pattern = create_regex(m_num_of_knobs);
        m_data_rate_ms = 50U;
        m_port = "COM12";

        m_endpoints.emplace_back(endpoint("master"));
    }
}

std::shared_ptr<stack_c> audio_mixer_c::get_data_stack() const
{
    return this->m_data_stack;
}

uint16_t audio_mixer_c::get_data_rate() const
{
    return this->m_data_rate_ms;
}

audio_mixer_c::baud_rate_t audio_mixer_c::get_baud_rate() const
{
    return this->m_baud_rate;
}

std::string audio_mixer_c::get_port() const
{
    return this->m_port;
}

void audio_mixer_c::run(bool& exit_app)
{
    while(!exit_app) {
        // Get data from serial
        std::optional<std::string> data = m_data_stack->get_latest_match(m_data_pattern);

        if (data) {
            // Make a decision based on the data.
            auto vals = extract_values(data.value());
            if (vals.size() != static_cast<int>(m_num_of_knobs)) {
                audio_mixer::log_error(
                    "knobs[" + std::to_string(m_num_of_knobs) + "] != vals[" + std::to_string(vals.size()) + "]");
                continue;
            } else {
                // Process the values
                update(vals);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(this->m_data_rate_ms)));
    }
}

std::regex audio_mixer_c::create_regex(uint16_t count)
{
    // Accept any 1-4 digit number (0-1023 from Arduino)
    std::string numberPattern = "(?:[0-9]{1,4})";

    std::string fullPattern = "^" + numberPattern;
    for (int i = 1; i < count; ++i) {
        fullPattern += "\\|" + numberPattern;
    }
    fullPattern += "$";

    return std::regex(fullPattern);
}

std::vector<int> audio_mixer_c::extract_values(std::string& values)
{
    std::vector<int> result;
    size_t pos = 0;
    std::string token;
    while ((pos = values.find('|')) != std::string::npos) {
        token = values.substr(0, pos);
        result.emplace_back(std::atoi(token.c_str()));
        values.erase(0, pos + 1);
    }
    result.emplace_back(std::atoi(values.c_str()));
    return result;
}

void audio_mixer_c::update_volumes(std::vector<int> const& values) {
    auto volumes = scale_values(values);
    // Assumes the volume and endpoints have corresponding indexes
    for (size_t i = 0; i < volumes.size(); i++) {
        m_endpoints.at(i).set_volume = volumes.at(i);
    }
}

void audio_mixer_c::update(std::vector<int> const& values)
{
    // TODO: map the values to the corresponding application
    update_volumes(values);

    // Get updated endpoints, filtering out ones that are not desired
#ifdef _WIN32
    auto available_endpoints = m_media.get_endpoints();
    for (auto & avail_endpoint : available_endpoints) {
        auto it = std::find(m_endpoints.begin(), m_endpoints.end(), avail_endpoint);
        if (it != m_endpoints.end()) {
            it->name = avail_endpoint.name;
            it->current_volume = avail_endpoint.current_volume;
        }
    }
    
    for (auto & endpoint : m_endpoints) {
        if (endpoint.name == "master") {
            m_media.set_master_volume(endpoint.set_volume);
        } else if (endpoint.name == "mic") {
             //Support for mic input devices
            m_media.set_microphone_volume(endpoint.set_volume); // untested
        } else if (std::find(available_endpoints.begin(), available_endpoints.end(), endpoint) != available_endpoints.end()) {
             //Set the volume for the application
            m_media.set_application_volume(endpoint);
        }
    }
#endif
    // For debugging
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

std::vector<float> audio_mixer_c::scale_values(std::vector<int> const& values)
{
    std::vector<float> output;
    output.reserve(values.size());

    for (int value : values) {
        // Normalize each value from [0, 1023] to [0.0, 1.0]
        output.emplace_back(value / 1023.0f);
    }

    return output;
}

} // namespace audio_mixer
