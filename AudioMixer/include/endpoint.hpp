#ifndef __ENDPOINT__HPP__
#define __ENDPOINT__HPP__

namespace audio_mixer
{
    inline std::string toLower(const std::string &str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // TODO: add support for an endpoint linking to a list of applications.
    struct endpoint
    {
        std::string name;
        float current_volume;
        float set_volume;
        uint32_t pid;

        endpoint(std::string const name)
            : name(name),
              current_volume(0),
              set_volume(0),
              pid(0) {
              };

        inline bool operator==(endpoint const &other) const
        {
            return toLower(this->name) == toLower(other.name);
        }
    };
}

#endif // end __ENDPOINT__HPP__
