#ifndef __STACK__HPP__
#define __STACK__HPP__

// Libraries
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <stack>
#include <string>

#define AUDIO_MIXER_STACK_MAX_SIZE 64

namespace audio_mixer
{
    class stack_c
    {
    public:
        // Push an element onto the stack
        void push(const std::string &value);

        // Pop an element from the stack (returns std::optional)
        std::optional<std::string> pop();

        // Check if the stack is empty
        bool empty() const;

        // Get the size of the stack
        size_t size() const;

        // Returns the most recent element that matches the provided regex pattern
        std::optional<std::string> get_latest_match(std::regex const &pattern);

    private:
        void clear();

        std::stack<std::string> stack_;
        mutable std::mutex mutex_; // Mutable allows const methods to lock the mutex.
    };

} // end namespace audio_mixer

#endif // __STACK__HPP__
