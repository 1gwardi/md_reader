#pragma once

int check_review_window(const char* url, const char* script_path) {
    @autoreleasepool {
        struct State { bool busy = false, started = false, done = false; NSString* failure = nil; } storage;
        State* state = &storage;
        NSString* script = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:script_path] encoding:NSUTF8StringEncoding error:nil];
        if (!script) return 2;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
        run_native_window(url, "mdread review regression", [&] {
            if (std::chrono::steady_clock::now() > deadline) { state->failure = @"Review test timed out"; return true; }
            if (state->done) return true;
            if (state->busy) return false;
            WKWebView* view = (WKWebView*)NSApp.keyWindow.contentView;
            if (![view isKindOfClass:WKWebView.class]) return false;
            state->busy = true;
            [view evaluateJavaScript:@"window.__reviewResult || (document.querySelector('#status')?.hasAttribute('data-error') ? 'ready' : 'loading')"
                completionHandler:^(id value, NSError* error) {
                state->busy = false;
                if (error || ![value isKindOfClass:NSString.class]) return;
                if ([value isEqualToString:@"ready"] && !state->started) {
                    state->started = true;
                    [view evaluateJavaScript:script completionHandler:^(id, NSError* error) {
                        if (error) { state->failure = error.localizedDescription; state->done = true; }
                    }];
                } else if ([value isEqualToString:@"passed"]) state->done = true;
                else if ([value hasPrefix:@"FAILED:"]) { state->failure = value; state->done = true; }
            }];
            return false;
        });
        if (state->failure) { std::cerr << state->failure.UTF8String << '\n'; return 1; }
        std::cout << "Review UI regressions passed\n";
        return 0;
    }
}
