#ifndef __OS__MEDIA_INTERFACE__HPP__
#define __OS__MEDIA_INTERFACE__HPP__

#include <vector>

#include "endpoint.hpp"

namespace audio_mixer
{
    // Abstract class to allow cross-platform support.
    class os_media_interface_c
    {
    public:
        /// Brief: Initialize the com object
        virtual void initialize() = 0;

        /// Brief: Set the master volume control
        /// param[in] float: a float representing the desired volume.
        virtual void set_master_volume(float) = 0;

        /// Brief: List application volumes and process IDs
        /// returns: A list of discoverable endpoints
        virtual std::vector<endpoint> get_endpoints() = 0;

        /// Brief: Set the volume for a specific application
        /// param[in] endpoint: The endpoint to update.
        /// returns: If the correct application was found and updated or not.
        virtual bool set_application_volume(endpoint const &) = 0;

        /// Brief: Set an audio input volume
        /// param[in] float: a float representing the desired volume.
        virtual void set_microphone_volume(float) = 0;
    };
} // end of audio_mixer namespace

#endif // __OS__MEDIA_INTERFACE__HPP__
