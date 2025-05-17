#include "stack.hpp"

#include "logger.hpp"

namespace audio_mixer {
// Push an element onto the stack
void stack_c::push(const std::string& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stack_.size() >= AUDIO_MIXER_STACK_MAX_SIZE) {
        stack_.pop();
    }

    stack_.emplace(value);
}

// Pop an element from the stack (returns std::optional)
std::optional<std::string> stack_c::pop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stack_.empty()) {
        return std::nullopt; // Indicate that the stack is empty.
    }
    std::string topValue = stack_.top();
    stack_.pop();
    return topValue;
}

// Check if the stack is empty
bool stack_c::empty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stack_.empty();
}

// Get the size of the stack
size_t stack_c::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stack_.size();
}

// Returns the most recent element that matches the provided regex pattern
std::optional<std::string> stack_c::get_latest_match(std::regex const& pattern)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<std::string> result = std::nullopt;

    // Find the most recently match
    while (!stack_.empty()) {
        std::string topElement = stack_.top();
        stack_.pop();
        if (std::regex_match(topElement, pattern)) {
            result = topElement; // Save the most recent match
            break;
        }
    }

    clear();
    return result;
}

void stack_c::clear() {
    std::stack<std::string> empty_stack;
    stack_ = std::move(empty_stack); // Replace with a new, empty stack
}
};
