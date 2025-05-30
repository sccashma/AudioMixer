# AudioMixer

AudioMixer is a C++ application that communicates with an Arduino device via a serial port. The program performs a handshake with the device, reads formatted data (knob/slider values), validates and extracts the data using regular expressions, and then uses the extracted values to update audio volumes (such as system or application volumes). It uses Boost.Asio for asynchronous I/O and is designed to run on Windows (with plans for cross‑platform extensions).

## Features

- **Serial Communication:** Scans for available serial ports, connects, and maintains communication with the Arduino.
- **Handshake & Heartbeat:** Implements a handshake with a custom protocol and regularly checks for a heartbeat from the device.
- **Data Processing:** Extracts numeric values from data strings, normalizes them, and updates audio settings accordingly.
- **Threading & Logging:** Uses separate threads for serial communication and audio processing alongside robust logging.

## Dependencies

- **C++17 or Newer:** The codebase employs modern C++ language features.
- **Boost.Asio:** Used for asynchronous serial communication.
- **CMake:** For configuring the build system.
- **Windows SDK:** For Windows API calls (when building on Windows).
- **Arduino:** The expected device that sends data to AudioMixer (your Arduino sketch should match the protocol expected by AudioMixer).

## Building the Application

1. **Clone the Repository with Submodules:**
   To ensure all submodules are cloned along with the repository, run:
   ```bash
   git clone --recursive https://github.com/sccashma/AudioMixer.git
   cd AudioMixer
   ```
   If you already cloned the repository without submodules, you can initialize them later with:
   ```bash
   git submodule update --init --recursive
   ```

2. **Configure with CMake:**
   From the root of the repository, run:
   ```bash
   cmake -S . -B build
   ```

3. **Build the Project:**
   For a Release build, run:
   ```bash
   cmake --build build --config Release
   ```
   This will generate the executable in the build directory.

4. **Opening in Visual Studio Code or Visual Studio:**
   Open the root folder in your IDE and use the integrated CMake tools to configure and build the project.

## Installing on Windows 11 Using Task Scheduler

To have AudioMixer start automatically after user logon on a Windows 11 machine, follow these steps:

1. **Locate the Executable:**
   Ensure you have built the executable (for example, `AudioMixer.exe`) from the build folder.

2. **Open Task Scheduler:**
   Press `Win + R`, type `taskschd.msc`, and press Enter to open Task Scheduler.

3. **Create a New Basic Task:**
   - In the right-hand pane, click **Create Basic Task…**
   - Enter a name (e.g., "AudioMixer Startup") and a description, then click **Next**.

4. **Set the Trigger:**
   - Choose **When I log on** and click **Next**.

5. **Set the Action:**
   - Choose **Start a program** and click **Next**.
   - Click **Browse…** and navigate to your `AudioMixer.exe`.

6. **Finish:**
   - Click **Finish** to create the task.

7. **Test:**
   Log off and log on again, or run the task manually from Task Scheduler to verify the application starts as expected.

## Usage

Once running, AudioMixer will:
- Continuously scan for the Arduino device on available serial ports.
- Handle the handshake and then receive continuous data streams.
- Parse and process these data values to update the system or application audio volumes.

## Troubleshooting

- **Serial Connection Issues:** Verify that the Arduino is connected properly and the correct COM port is available.
- **Handshake Failures:** Ensure the Arduino firmware sends the expected handshake key, and that the baud rate matches on both sides.
- **Logging:** If issues occur, try enabling more verbose logging (uncomment debug log lines) to get more insight into the connection process.
- **Race Conditions:** If you experience intermittent crashes, review asynchronous operations and variable lifetimes – check that all shared resources are managed using `std::shared_ptr` as needed.

## Acknowledgements

- Boost.Asio for asynchronous serial communication.
