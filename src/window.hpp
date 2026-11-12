#pragma once

#include <functional>
#include <string>

// Runs the native UI on the calling (main) thread until the window closes or
// should_stop returns true. The HTTP server runs independently.
void run_native_window(const std::string& url, const std::string& title,
                       const std::function<bool()>& should_stop);
