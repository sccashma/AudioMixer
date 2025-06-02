#include "AudioMixerConfig.h"
#include "audio_mixer.hpp"
#include "logger.hpp"
#include "serial.hpp"

#include <boost/asio.hpp>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

// Global exit flag
bool exit_app = false;

#ifdef _WIN32
// Forward declaration for main
int main();

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return main(); }

BOOL WINAPI HandlerRoutine(_In_ DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT)
    {
        exit_app = true;
        return TRUE;
    }
    return FALSE;
}
#endif

int main()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(HandlerRoutine, TRUE);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

    // Initialize logging
    // TODO: Make this configurable using command line arguments or config file
    audio_mixer::logger_c::instance().set_log_level(audio_mixer::logger_c::LogLevel::DEBUG);
    audio_mixer::log_info("AudioMixer starting");
    audio_mixer::log_debug("AudioMixer build version: " + std::to_string(AudioMixer_VERSION_MAJOR) + "." +
                     std::to_string(AudioMixer_VERSION_MINOR));
    audio_mixer::log_debug("Build date: " __DATE__ " " __TIME__);
    audio_mixer::log_debug("Platform: " + std::string(
#ifdef _WIN32
        "Windows"
#else
        "Linux"
#endif
    ));

    try
    {
        boost::asio::io_context io_context;
        audio_mixer::audio_mixer_c app(io_context);
        audio_mixer::serial_connection_c connection(io_context, app.get_data_stack(), app.get_baud_rate());

        // Run Serial interface in a separate thread
        auto serial_run = [&connection]()
        {
            try
            {
                connection.run(exit_app);
            }
            catch (const std::exception &e)
            {
                audio_mixer::log_error(std::string("Serial Thread|Exception: ") + e.what());
            }
            catch (...)
            {
                audio_mixer::log_error("Serial Thread|Unknown exception occurred");
            }
        };
        std::thread serial_thread(serial_run);

        while (!exit_app)
        {
            app.run(exit_app);
        }

        serial_thread.join();
    }
    catch (const std::exception &e)
    {
        audio_mixer::log_error(std::string("Exception: ") + e.what());
    }
    catch (...)
    {
        audio_mixer::log_error("Main Thread|Unknown exception occurred");
    }

    audio_mixer::log_info("AudioMixer exiting");

    return 0;
}
