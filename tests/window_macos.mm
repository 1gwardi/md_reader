#include "window.hpp"

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include <chrono>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) return 2;
    @autoreleasepool {
        struct State {
            int stage = 0;
            bool pending = false;
            bool closed = false;
            NSString* failure = nil;
        } storage;
        State* state = &storage;
        const bool directory = argc == 4;
        if (directory) state->stage = -2;
        const std::string file = argv[2];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        run_native_window(argv[1], "mdread native window test", [&] {
            if (std::chrono::steady_clock::now() > deadline) {
                state->failure = @"Timed out waiting for Markdown in WKWebView";
                return true;
            }
            if (state->failure) return true;
            if (state->pending || state->stage == (directory ? 5 : 2)) return false;
            NSWindow* window = NSApp.keyWindow;
            if (!window || ![window.contentView isKindOfClass:WKWebView.class]) return false;
            WKWebView* view = (WKWebView*)window.contentView;
            state->pending = true;
            NSString* script;
            switch (state->stage) {
            case -2:
                script = @"(()=>{const a=document.querySelector('#files a[href=\"/files/chapters/\"]');if(!a)return false;a.click();return true;})()";
                break;
            case -1:
                script = @"(()=>{const a=document.querySelector('#files a[href=\"/files/chapters/native%20window.md\"]');if(!a)return false;a.click();return true;})()";
                break;
            case 0:
                script = @"(()=>{const main=document.querySelector('main');const details=main?.querySelector('#html-details');const styled=main?.querySelector('#html-styled');const img=main?.querySelector('#html-image');if(!main?.textContent.includes('Native window test') || !document.querySelector('#status')?.textContent.startsWith('Live') || !details || !styled || !img?.naturalWidth)return false;if(window.htmlExecuted || main.querySelector('script,iframe,style,[onerror]') || main.querySelector('#unsafe-link')?.hasAttribute('href'))return false;if(getComputedStyle(styled).color!=='rgb(12, 34, 56)' || !main.querySelector('h2#custom-anchor') || main.querySelector('pre code')?.textContent.trim()!=='<em>HTML code example</em>')return false;details.querySelector('summary').click();if(!details.open)return false;scrollTo(0,300);return scrollY>200;})()";
                break;
            case 1:
                script = @"document.querySelector('main')?.textContent.includes('Updated native document') && document.querySelector('main #updated-html')?.textContent==='Updated HTML' && !window.htmlExecuted && !document.querySelector('main script, main [onerror]') && scrollY>200";
                break;
            case 2:
                script = @"(()=>{const a=document.querySelector('main a[href=\"second.md\"]');if(!a)return false;a.click();return true;})()";
                break;
            case 3:
                script = @"(()=>{if(!document.querySelector('main')?.textContent.includes('Second document'))return false;document.querySelector('.root-link').click();return true;})()";
                break;
            default:
                script = @"document.querySelector('#directory-list')?.textContent.includes('chapters')";
            }
            [view evaluateJavaScript:script completionHandler:^(id result, NSError* error) {
                state->pending = false;
                // A click may replace the JavaScript context while its result is in flight.
                if (error) { return; }
                if (![result respondsToSelector:@selector(boolValue)] || ![result boolValue]) return;
                if (state->stage < 0) { ++state->stage; }
                else if (state->stage == 0) {
                    if (![window.title isEqualToString:@"native window.md — mdread"]) {
                        state->failure = @"Incorrect document window title";
                        return;
                    }
                    std::ofstream output(file);
                    output << "# Updated native document\n\n";
                    output << "<div id=\"updated-html\" style=\"color: rgb(12, 34, 56)\">Updated HTML</div>\n\n"
                              "<img src=\"missing.png\" onerror=\"window.htmlExecuted=true\">\n\n"
                              "<script>window.htmlExecuted=true</script>\n\n";
                    if (directory) output << "<a href=\"second.md\">Next document</a>\n\n";
                    for (int i = 0; i < 100; ++i) output << "Paragraph " << i << " preserved for scrolling.\n\n";
                    if (!output) { state->failure = @"Unable to update Markdown fixture"; return; }
                    state->stage = 1;
                } else if (!directory || state->stage == 4) {
                    state->stage = directory ? 5 : 2;
                    [window performClose:nil];
                    state->closed = !window.visible;
                } else { ++state->stage; }
            }];
            return false;
        });
        if (state->failure || state->stage != (directory ? 5 : 2) || !state->closed) {
            std::cerr << (state->failure ? state->failure.UTF8String : "Window did not close successfully") << '\n';
            return 1;
        }
        std::cout << "Native window: rendered Markdown, live update, preserved scroll, close passed\n";
        std::cout << "HTML: details, inline styles, anchors, images, escaped code and script filtering passed\n";
        if (directory) std::cout << "Directory browser: folder click, file click, Markdown link, return to root passed\n";
    }
    return 0;
}
