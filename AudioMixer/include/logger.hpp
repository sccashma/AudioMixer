#ifndef __AUDIO_MIXER_LOGGER_HPP__
#define __AUDIO_MIXER_LOGGER_HPP__

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <filesystem> // C++17
#include <ctime>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace audio_mixer
{

    class logger_c
    {
    public:
        enum LogLevel
        {
            DEBUG,
            INFO,
            WARNING,
            LOG_ERROR
        };

        LogLevel m_log_level;

        void set_log_level(LogLevel level)
        {
            m_log_level = level;
        }

        static logger_c &instance()
        {
            static logger_c inst;
            return inst;
        }

        void log(LogLevel level, const std::string &msg)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (level < m_log_level)
            {
                return;
            }

            checkRolling();
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

        void log_debug(std::string const &msg)
        {
            log(LogLevel::DEBUG, "[DEBUG] " + msg);
        }

        void log_info(std::string const &msg)
        {
            log(LogLevel::INFO, "[INFO] " + msg);
        }

        void log_warning(std::string const &msg)
        {
            log(LogLevel::WARNING, "[WARNING] " + msg);
        }
   
        void log_error(std::string const &msg)
        {
            log(LogLevel::LOG_ERROR, "[ERROR] " + msg);
        }

    private: 
        // Maximum number of rolled log files to keep.
        uint8_t const m_max_log_files = 5; 

        logger_c()
        {
            // Determine log file path
            std::string path = "audiomixer.log";
#ifdef _WIN32
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string exeDir(exePath);
            auto pos = exeDir.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                exeDir = exeDir.substr(0, pos + 1);
                path = exeDir + "audiomixer.log";
            }
#endif
            logPath_ = path;
            openLogFile();

            // Set default log level
            m_log_level = LogLevel::INFO; // Default to INFO level
        }

        ~logger_c()
        {
            if (logfile_.is_open())
                logfile_.close();
        }

        // Check file size and perform rolling if needed.
        void checkRolling()
        {
            constexpr std::uintmax_t MAX_SIZE = 5 * 1024 * 1024; // 5 MB
            std::error_code ec;
            if (std::filesystem::exists(logPath_, ec))
            {
                auto size = std::filesystem::file_size(logPath_, ec);
                if (!ec && size >= MAX_SIZE)
                {
                    // Close current file
                    logfile_.close();
                    
                    // Create a timestamp suffix for the backup filename
                    auto t = std::time(nullptr);
                    std::tm tm_snapshot;
#ifdef _WIN32
                    localtime_s(&tm_snapshot, &t);
#else
                    localtime_r(&t, &tm_snapshot);
#endif
                    std::ostringstream oss;
                    oss << std::put_time(&tm_snapshot, "%Y%m%d_%H%M%S");
                    std::string backupName = logPath_ + "." + oss.str();

                    // Rename the current log file
                    std::filesystem::rename(logPath_, backupName, ec);

                    // Clean up old rolled logs if too many exist.
                    cleanupOldLogs();

                    // Reopen a new log file
                    openLogFile();
                }
            }
        }

        // Delete the oldest log files if we have more than m_max_log_files.
        void cleanupOldLogs()
        {
            namespace fs = std::filesystem;
            fs::path logFilePath(logPath_);
            fs::path logDir = logFilePath.parent_path();
            std::string baseName = logFilePath.filename().string(); // e.g. "audiomixer.log"
            std::vector<fs::directory_entry> rolledFiles;

            // Iterate over directory contents and filter those matching the backup pattern.
            for (const auto &entry : fs::directory_iterator(logDir))
            {
                if (entry.is_regular_file())
                {
                    std::string fname = entry.path().filename().string();
                    // Check if the filename starts with the base name plus a dot.
                    if (fname.rfind(baseName + ".", 0) == 0)
                    {
                        rolledFiles.push_back(entry);
                    }
                }
            }

            // If there are too many, sort by last write time and remove the oldest.
            if (rolledFiles.size() > m_max_log_files)
            {
                std::sort(rolledFiles.begin(), rolledFiles.end(), [](const fs::directory_entry &a, const fs::directory_entry &b)
                {
                    return fs::last_write_time(a.path()) < fs::last_write_time(b.path());
                });
                std::size_t numToDelete = rolledFiles.size() - m_max_log_files;
                for (std::size_t i = 0; i < numToDelete; ++i)
                {
                    fs::remove(rolledFiles[i].path());
                }
            }
        }

        void openLogFile()
        {
            logfile_.open(logPath_, std::ios::app);
        }

        std::ofstream logfile_;
        std::mutex mutex_;
        std::string logPath_;
    };

    inline void log_debug(const std::string &msg)
    {
        logger_c::instance().log_debug(msg);
    }

    inline void log_info(const std::string &msg)
    {
        logger_c::instance().log_info(msg);
    }

    inline void log_warning(const std::string &msg)
    {
        logger_c::instance().log_warning(msg);
    }

    inline void log_error(const std::string &msg)
    {
        logger_c::instance().log_error(msg);
    }

} // namespace audio_mixer

#endif // __AUDIO_MIXER_LOGGER_HPP__
