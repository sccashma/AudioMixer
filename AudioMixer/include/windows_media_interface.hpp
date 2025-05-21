#ifndef __MEDIA_INTERFACE__HPP__
#define __MEDIA_INTERFACE__HPP__

#include <algorithm>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <iostream>
#include <map>
#include <mmdeviceapi.h>
#include <psapi.h>
#include <set>
#include <string>
#include <tuple>
#include <Windows.h>
#include <wrl/client.h>

#include "logger.hpp"
#include "os_media_interface.hpp"

namespace audio_mixer {

class com_initializer_c
{
public:
    com_initializer_c()
        : m_initialized(false)
    {
    }

    void init()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        if (hr == S_FALSE) {
            CoUninitialize();
        }

        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to initialize COM library.");
            throw std::runtime_error("COM initialization failed");
        }

        m_initialized = (hr == S_OK);
    }

    ~com_initializer_c()
    {
        if (m_initialized) {
            CoUninitialize();
        }
    }
private:
    bool m_initialized;
};

class windows_media_interface_c : public os_media_interface_c
{
public:
    void initialize()
    {
        m_com.init();
    }
    
    // Constructor that initializes COM and sets up necessary resources
    windows_media_interface_c()
        : m_currentDeviceId("")
        , m_deviceEnumerator(nullptr)
        , m_device(nullptr)
        , m_sessionManager(nullptr)
        , m_endpointVolume(nullptr)
        , m_com()
    {
        this->initialize();

        // Create and cache device enumerator and session manager
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), &m_deviceEnumerator);
        if (FAILED(hr)) {
           audio_mixer::log_error("Failed to create device enumerator.");
           throw std::runtime_error("Failed to create device enumerator");
        }

        // Get Default Audio Endpoint (for audio rendering, e.g., speakers/headphones)
        hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
        if (FAILED(hr)) {
           audio_mixer::log_error("Failed to get default audio endpoint.");
           throw std::runtime_error("Failed to get default audio endpoint");
        }

        // Activate Audio Session Manager to control and monitor sessions
        hr = m_device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &m_sessionManager);
        if (FAILED(hr)) {
           audio_mixer::log_error("Failed to activate audio session manager.");
           throw std::runtime_error("Failed to activate audio session manager");
        }

        // Activate IAudioEndpointVolume interface to control volume
        hr = m_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &m_endpointVolume);
        if (FAILED(hr)) {
           audio_mixer::log_error("Failed to activate IAudioEndpointVolume interface.");
           throw std::runtime_error("Failed to activate IAudioEndpointVolume interface");
        }
    }

    // Destructor cleans up resources properly
    ~windows_media_interface_c() {
    }

    // Set the master volume control
    void set_master_volume(float volume_level) override {
        if (volume_level < 0.0f || volume_level > 1.0f) {
            audio_mixer::log_error("Volume level must be between 0.0 and 1.0");
            return;
        }

        ensure_default_device();

        if (!m_endpointVolume) {
            // Activate and cache the endpoint volume interface
            HRESULT hr = m_device->Activate(
                __uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                nullptr, reinterpret_cast<void**>(m_endpointVolume.GetAddressOf()));
            if (FAILED(hr)) {
                audio_mixer::log_error("Failed to activate IAudioEndpointVolume interface.");
                return;
            }
        }

        HRESULT hr = m_endpointVolume->SetMasterVolumeLevelScalar(volume_level, nullptr);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to set master volume.");
        }
    }

    // List application volumes and process IDs
    std::vector<endpoint> get_endpoints() override {
        ensure_default_device();
        
        std::vector<endpoint> appVolumeMap;
        if (!m_sessionManager) {
            audio_mixer::log_error("Session manager not initialized.");
            return appVolumeMap;
        }

        Microsoft::WRL::ComPtr<IAudioSessionEnumerator> pSessionEnumerator;
        HRESULT hr = m_sessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to get session enumerator.");
            return appVolumeMap;
        }

        int sessionCount = 0;
        hr = pSessionEnumerator->GetCount(&sessionCount);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to get session count.");
            return appVolumeMap;
        }

        for (int i = 0; i < sessionCount; ++i) {
            Microsoft::WRL::ComPtr<IAudioSessionControl> pSessionControl;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);
            if (FAILED(hr) || !pSessionControl) continue;

            Microsoft::WRL::ComPtr<IAudioSessionControl2> pSessionControl2;
            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(pSessionControl2.GetAddressOf()));
            if (FAILED(hr) || !pSessionControl2) continue;

            DWORD processId = 0;
            hr = pSessionControl2->GetProcessId(&processId);
            if (FAILED(hr) || processId == 0) continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            std::string processName = "<unknown>";
            if (hProcess) {
                wchar_t process_name[MAX_PATH] = L"<Unknown>";
                if (K32GetProcessImageFileNameW(hProcess, process_name, MAX_PATH)) {
                    processName = wide_char_proc_to_executable(process_name);
                }
                CloseHandle(hProcess);
            }

            Microsoft::WRL::ComPtr<ISimpleAudioVolume> pSimpleVolume;
            hr = pSessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(pSimpleVolume.GetAddressOf()));
            if (FAILED(hr) || !pSimpleVolume) continue;

            float volume = 0.0f;
            hr = pSimpleVolume->GetMasterVolume(&volume);
            if (FAILED(hr)) continue;

            endpoint app(processName);
            app.current_volume = volume;
            app.pid = static_cast<uint32_t>(processId);
            appVolumeMap.emplace_back(app);
        }
        return appVolumeMap;
    }

    // Set the volume for a specific application
    bool set_application_volume(endpoint const& app) override {
        ensure_default_device();

        if (!m_sessionManager) {
            audio_mixer::log_error("Session manager not initialized.");
            return false;
        }
        if (app.set_volume < 0.0f || app.set_volume > 1.0f) {
            audio_mixer::log_error("Volume must be between 0.0 and 1.0");
            return false;
        }

        Microsoft::WRL::ComPtr<IAudioSessionEnumerator> pSessionEnumerator;
        HRESULT hr = m_sessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to get session enumerator.");
            return false;
        }

        int sessionCount = 0;
        hr = pSessionEnumerator->GetCount(&sessionCount);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to get session count.");
            return false;
        }

        bool found = false;
        for (int i = 0; i < sessionCount; ++i) {
            Microsoft::WRL::ComPtr<IAudioSessionControl> pSessionControl;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);
            if (FAILED(hr) || !pSessionControl) continue;

            Microsoft::WRL::ComPtr<IAudioSessionControl2> pSessionControl2;
            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(pSessionControl2.GetAddressOf()));
            if (FAILED(hr) || !pSessionControl2) continue;

            DWORD sessionProcessId = 0;
            hr = pSessionControl2->GetProcessId(&sessionProcessId);
            if (FAILED(hr) || sessionProcessId == 0) continue; // skip system sounds

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, sessionProcessId);
            std::string processName = "<unknown>";
            if (hProcess) {
                wchar_t process_name[MAX_PATH] = L"<Unknown>";
                if (K32GetProcessImageFileNameW(hProcess, process_name, MAX_PATH)) {
                    processName = wide_char_proc_to_executable(process_name);
                }
                CloseHandle(hProcess);
            }

            // Match by name (case-insensitive)
            if (toLower(processName) == toLower(app.name)) {
                Microsoft::WRL::ComPtr<ISimpleAudioVolume> pSimpleVolume;
                hr = pSessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(pSimpleVolume.GetAddressOf()));
                if (FAILED(hr) || !pSimpleVolume) continue;

                if (app.set_volume == 0.0f) {
                    hr = pSimpleVolume->SetMute(TRUE, nullptr);
                    if (FAILED(hr)) {
                        audio_mixer::log_error("Failed to mute " + app.name);
                    }
                } else {
                    hr = pSimpleVolume->SetMasterVolume(app.set_volume, nullptr);
                    if (FAILED(hr)) {
                        audio_mixer::log_error("Failed to set volume for " + app.name);
                    }
                    // Unmute if muted
                    BOOL is_muted = FALSE;
                    hr = pSimpleVolume->GetMute(&is_muted);
                    if (SUCCEEDED(hr) && is_muted) {
                        hr = pSimpleVolume->SetMute(FALSE, nullptr);
                        if (FAILED(hr)) {
                            audio_mixer::log_error("Failed to unmute " + app.name);
                        }
                    }
                }
                found = true;
            }
        }
        if (!found) {
            audio_mixer::log_error("No session found for " + app.name);
        }
        return found;
    }

    // Set an audio input volume
    void set_microphone_volume(float volume_level) {
        if (volume_level < 0.0f || volume_level > 1.0f) {
            audio_mixer::log_error("Volume level must be between 0.0 and 1.0");
            return;
        }

        Microsoft::WRL::ComPtr<IMMDeviceEnumerator> micEnumerator;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(micEnumerator.GetAddressOf()));
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to create device enumerator for mic.");
            return;
        }

        Microsoft::WRL::ComPtr<IMMDevice> micDevice;
        hr = micEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &micDevice);
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to get default audio capture endpoint.");
            return;
        }

        Microsoft::WRL::ComPtr<IAudioEndpointVolume> micEndpointVolume;
        hr = micDevice->Activate(
            __uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(micEndpointVolume.GetAddressOf()));
        if (FAILED(hr)) {
            audio_mixer::log_error("Failed to activate IAudioEndpointVolume for mic.");
            return;
        }

        if (volume_level == 0.0f) {
            hr = micEndpointVolume->SetMute(TRUE, nullptr);
            if (FAILED(hr)) {
                audio_mixer::log_error("Failed to mute microphone.");
            }
        } else {
            hr = micEndpointVolume->SetMasterVolumeLevelScalar(volume_level, nullptr);
            if (FAILED(hr)) {
                audio_mixer::log_error("Failed to set microphone volume.");
            } else {
                BOOL is_muted = FALSE;
                hr = micEndpointVolume->GetMute(&is_muted);
                if (SUCCEEDED(hr) && is_muted) {
                    hr = micEndpointVolume->SetMute(FALSE, nullptr);
                    if (FAILED(hr)) {
                        audio_mixer::log_error("Failed to unmute microphone.");
                    }
                }
            }
        }
    }


private:
    com_initializer_c m_com;
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_deviceEnumerator;
    Microsoft::WRL::ComPtr<IMMDevice> m_device;
    Microsoft::WRL::ComPtr<IAudioSessionManager2> m_sessionManager;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> m_endpointVolume;
    std::string m_currentDeviceId;

    std::string wide_char_proc_to_executable(const wchar_t* wideStr)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
        if (size == 0) {
            return "";  // If conversion fails, return an empty string
        }

        // Currently, assumes null-terminated string. This is not a guaruntee, future work is to
        // first check for a null-terminator and then remove if it exists.
        std::string s(size - 1, '\0');  // Subtract 1 to exclude the null terminator
        WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, &s[0], size, nullptr, nullptr);

        std::string result = "<unknown>";
        size_t pos = s.rfind('\\');
        if (pos != std::string::npos) {
            result = s.substr(pos + 1);
        }
        return result;
    }

    // Helper to get the current default device ID
    std::string get_default_device_id() {
        LPWSTR deviceId = nullptr;
        HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
        if (FAILED(hr) || !m_device) return "";
        hr = m_device->GetId(&deviceId);
        if (FAILED(hr) || !deviceId) return "";
        std::wstring ws(deviceId);
        CoTaskMemFree(deviceId);
        return std::string(ws.begin(), ws.end());
    }

    // Call this before any operation that needs the default device
    void ensure_default_device() {
        std::string newId = get_default_device_id();
        if (newId.empty()) return;
        if (newId != m_currentDeviceId) {
            // Reinitialize everything
            m_device.Reset();
            m_sessionManager.Reset();
            m_endpointVolume.Reset();

            HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
            if (FAILED(hr)) return;
            hr = m_device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &m_sessionManager);
            if (FAILED(hr)) return;
            hr = m_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &m_endpointVolume);
            if (FAILED(hr)) return;
            m_currentDeviceId = newId;
        }
    }
};

} // end of audio_mixer namespace

#endif // __MEDIA_INTERFACE__HPP__
