# CMakeList.txt : CMake project for AudioMixer, include source and define
# project specific logic here.
#

project("AudioMixer" VERSION 0.1)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# For windows 10/11
add_definitions(-D_WIN32_WINNT=0x0A00)

# Add source files
file(GLOB SOURCES "src/*.cpp")
include_directories("include")

# Find external libraries (e.g., Windows SDK)
find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)

# Boost for Asio
set(BOOST_ROOT "C:/Program Files/boost/boost_1_86_0")
include_directories(${BOOST_ROOT})

# Add executable
add_executable(AudioMixer ${SOURCES})
configure_file(${PROJECT_SOURCE_DIR}/AudioMixerConfig.h.in ${PROJECT_SOURCE_DIR}/AudioMixerConfig.h)

# Link system libraries
target_link_libraries(AudioMixer
    PRIVATE
    ole32
    oleaut32
    psapi
    Threads::Threads
    Uiautomationcore
    uuid
    yaml-cpp
)

if (WIN32)
    set_target_properties(AudioMixer PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()