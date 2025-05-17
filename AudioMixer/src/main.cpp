#include "audio_mixer.hpp"
#include "AudioMixerConfig.h"
#include "logger.hpp"
#include "Serial.hpp"

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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}

BOOL WINAPI HandlerRoutine(_In_ DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT) {
        exit_app = true;
        return TRUE;
    }
    return FALSE;
}
#endif

int main() {
#ifdef _WIN32
    SetConsoleCtrlHandler(HandlerRoutine, TRUE);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

    audio_mixer::log(
        "AudioMixer Version " +
        std::to_string(AudioMixer_VERSION_MAJOR) +
        "." +
        std::to_string(AudioMixer_VERSION_MINOR));

    try {
        boost::asio::io_context io_context;
        audio_mixer::audio_mixer_c app(io_context);
        audio_mixer::serial_connection_c connection(
            io_context, app.get_data_stack(), app.get_port(), app.get_baud_rate(), app.get_data_rate());

        // Run Serial interface in a separate thread
        auto serial_run = [&connection]() { connection.run(exit_app); };
        std::thread serial_thread(serial_run);

        while (!exit_app) {
            app.run(exit_app);
        }

        serial_thread.join();

    } catch (const std::exception& e) {
        audio_mixer::log_error(std::string("Exception: ") + e.what());
    }

    audio_mixer::log("AudioMixer exiting");

#ifdef _DEBUG
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}