#ifndef __AUDIO_MIXER_LOGGER_HPP__
#define __AUDIO_MIXER_LOGGER_HPP__

#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace audio_mixer {

class logger_c {
public:
    static logger_c& instance() {
        static logger_c inst;
        return inst;
    }

    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Get current time
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &now_c);
    #else
        localtime_r(&now_c, &tm_buf);
    #endif
        logfile_ << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] " << msg << std::endl;
        logfile_.flush();
    }

    void log_error(const std::string& msg) {
        log("[ERROR] " + msg);
    }

private:
    logger_c() {
        std::string path = "audiomixer.log";
#ifdef _WIN32
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string exeDir(exePath);
        auto pos = exeDir.find_last_of("\\/");
        if (pos != std::string::npos) {
            exeDir = exeDir.substr(0, pos + 1);
            path = exeDir + "audiomixer.log";
        }
#endif
        logfile_.open(path, std::ios::app);
    }
    ~logger_c() {
        logfile_.close();
    }
    std::ofstream logfile_;
    std::mutex mutex_;
};

inline void log(const std::string& msg) {
    logger_c::instance().log(msg);
}
inline void log_error(const std::string& msg) {
    logger_c::instance().log_error(msg);
}

} // namespace audio_mixer

#endif // __AUDIO_MIXER_LOGGER_HPP__