#include "window.hpp"

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include <chrono>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    @autoreleasepool {
        struct State {
            int stage = 0;
            bool pending = false;
            bool closed = false;
            NSString* failure = nil;
        } storage;
        State* state = &storage;
        const std::string file = argv[2];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        run_native_window(argv[1], "mdread native window test", [&] {
            if (std::chrono::steady_clock::now() > deadline) {
                state->failure = @"Timed out waiting for Markdown in WKWebView";
                return true;
            }
            if (state->failure) return true;
            if (state->pending || state->stage == 2) return false;
            NSWindow* window = NSApp.keyWindow;
            if (!window || ![window.contentView isKindOfClass:WKWebView.class]) return false;
            if (![window.title isEqualToString:@"mdread native window test"]) {
                state->failure = @"Incorrect native window title";
                return true;
            }
            WKWebView* view = (WKWebView*)window.contentView;
            state->pending = true;
            NSString* script = state->stage == 0
                ? @"(()=>{if(document.querySelector('main')?.textContent.includes('Native window test') && document.querySelector('#status')?.textContent.startsWith('Live')){scrollTo(0,300);return scrollY>200;}return false;})()"
                : @"document.querySelector('main')?.textContent.includes('Updated native document') && scrollY>200";
            [view evaluateJavaScript:script completionHandler:^(id result, NSError* error) {
                state->pending = false;
                if (error) { state->failure = error.localizedDescription; return; }
                if (![result respondsToSelector:@selector(boolValue)] || ![result boolValue]) return;
                if (state->stage == 0) {
                    std::ofstream output(file);
                    output << "# Updated native document\n\n";
                    for (int i = 0; i < 100; ++i) output << "Paragraph " << i << " preserved for scrolling.\n\n";
                    if (!output) { state->failure = @"Unable to update Markdown fixture"; return; }
                    state->stage = 1;
                } else {
                    state->stage = 2;
                    [window performClose:nil];
                    state->closed = !window.visible;
                }
            }];
            return false;
        });
        if (state->failure || state->stage != 2 || !state->closed) {
            std::cerr << (state->failure ? state->failure.UTF8String : "Window did not close successfully") << '\n';
            return 1;
        }
        std::cout << "Native window: rendered Markdown, live update, preserved scroll, close passed\n";
    }
    return 0;
}
