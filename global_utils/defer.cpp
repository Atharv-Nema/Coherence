#pragma once
#include <functional>

struct Defer {
    std::function<void(void)> cleanup;
    Defer(std::function<void()> cleanup)
        : cleanup(std::move(cleanup)) {}
    ~Defer() {
        if (cleanup) cleanup();
    }
};