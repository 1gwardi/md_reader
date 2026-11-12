#include "window.hpp"
#include "reader_checks.hpp"

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include <chrono>
#include <fstream>
#include <iostream>

void screenshot(WKWebView* view, NSString* name, void (^done)(NSString*)) {
    const char* directory = std::getenv("MDREAD_TEST_SCREENSHOTS");
    if (!directory) { done(nil); return; }
    NSString* folder = [NSString stringWithUTF8String:directory];
    [NSFileManager.defaultManager createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil];
    [view takeSnapshotWithConfiguration:nil completionHandler:^(NSImage* image, NSError* error) {
        if (error) { done(error.localizedDescription); return; }
        NSBitmapImageRep* bitmap = [NSBitmapImageRep imageRepWithData:image.TIFFRepresentation];
        NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (![png writeToFile:[folder stringByAppendingPathComponent:name] atomically:YES]) {
            done(@"Unable to save UI preview"); return;
        }
        done(nil);
    }];
}

#include "math_corpus.hpp"

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) return 2;
    if (argc == 4 && std::string(argv[3]) == "corpus") return check_math_corpus(argv[1], argv[2]);
    @autoreleasepool {
        struct State {
            int stage = 0;
            bool pending = false;
            bool closed = false;
            NSString* failure = nil;
        } storage;
        State* state = &storage;
        const bool directory = argc == 4;
        if (directory) state->stage = -5;
        const std::string file = argv[2];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        run_native_window(argv[1], "mdread native window test", [&] {
            if (std::chrono::steady_clock::now() > deadline) {
                state->failure = @"Timed out waiting for Markdown in WKWebView";
                return true;
            }
            if (state->failure) return true;
            if (state->pending || state->stage == (directory ? 7 : 2)) return false;
            NSWindow* window = NSApp.keyWindow;
            if (!window || ![window.contentView isKindOfClass:WKWebView.class]) return false;
            WKWebView* view = (WKWebView*)window.contentView;
            if (!view.allowsBackForwardNavigationGestures) {
                state->failure = @"Native back/forward trackpad gestures are disabled";
                return true;
            }
            state->pending = true;
            NSString* script;
            switch (state->stage) {
            case -5:
                script = @"(()=>{if(!document.querySelector('#files a[href=\"/files/chapters/\"]'))return false;const q=document.querySelector('#file-search');q.value='NO MATCH';q.dispatchEvent(new Event('input'));if(document.querySelector('#files a') || !document.querySelector('#files').textContent.includes('Ничего не найдено'))throw new Error('Search empty state failed');q.value='CHAPTERS';q.dispatchEvent(new Event('input'));if(document.querySelectorAll('#files a').length!==1)throw new Error('Case-insensitive search failed');q.value='';q.dispatchEvent(new Event('input'));const toggle=document.querySelector('#toggle-files');toggle.click();if(toggle.getAttribute('aria-expanded')!=='false')throw new Error('Sidebar did not hide');toggle.click();if(toggle.getAttribute('aria-expanded')!=='true')throw new Error('Sidebar did not reopen');const theme=document.querySelector('#toggle-theme');theme.click();if(document.documentElement.dataset.theme!=='light')throw new Error('Light theme failed');theme.click();if(document.documentElement.dataset.theme!=='dark')throw new Error('Dark theme failed');theme.click();if(document.documentElement.hasAttribute('data-theme'))throw new Error('System theme failed');if(!document.querySelector('#go-back').disabled || document.querySelector('#go-home').getAttribute('aria-disabled')!=='true')throw new Error('Root navigation state failed');return true;})()";
                break;
            case -4:
                script = @"(()=>{const a=document.querySelector('#files a[href=\"/files/chapters/\"]');if(!a)return false;a.click();return true;})()";
                break;
            case -3:
                script = @"(()=>{if(!document.querySelector('#files a[href=\"/files/chapters/native%20window.md\"]'))return false;const back=document.querySelector('#go-back');if(back.disabled)throw new Error('Back button disabled after navigation');back.click();return true;})()";
                break;
            case -2:
                script = @"(()=>{if(!document.querySelector('#directory-list a[href=\"/files/chapters/\"]'))return false;const forward=document.querySelector('#go-forward');if(forward.disabled)throw new Error('Forward button disabled after back');forward.click();return true;})()";
                break;
            case -1:
                script = @"(()=>{const a=document.querySelector('#files a[href=\"/files/chapters/native%20window.md\"]');if(!a)return false;a.click();return true;})()";
                break;
            case 0:
                script = @"(()=>{const main=document.querySelector('main');const details=main?.querySelector('#html-details');const styled=main?.querySelector('#html-styled');const img=main?.querySelector('#html-image');if(!main?.textContent.includes('Native window test') || !document.querySelector('#status')?.textContent.startsWith('Автообновление') || !details || !styled || !img?.naturalWidth)return false;if(window.htmlExecuted || main.querySelector('script,iframe,style,[onerror]') || main.querySelector('#unsafe-link')?.hasAttribute('href'))return false;if(getComputedStyle(styled).color!=='rgb(12, 34, 56)' || !main.querySelector('h2#custom-anchor') || main.querySelector('pre code')?.textContent.trim()!=='<em>HTML code example</em>')return false;details.querySelector('summary').click();if(!details.open)return false;scrollTo(0,300);return scrollY>200;})()";
                break;
            case 1:
                script = @"(()=>{if(!window.manualRefreshTested){const b=document.querySelector('#refresh');if(b.disabled)return false;b.click();if(!b.disabled || b.getAttribute('aria-busy')!=='true')throw new Error('Manual refresh did not start');window.manualRefreshTested=true;return false;}return document.querySelector('main')?.textContent.includes('Updated native document') && document.querySelector('main #updated-html')?.textContent==='Updated HTML' && !window.htmlExecuted && !document.querySelector('main script, main [onerror]') && scrollY>200;})()";
                break;
            case 2:
                script = @"(()=>{const a=document.querySelector('main a[href=\"second.md\"]');if(!a)return false;a.click();return true;})()";
                break;
            case 3:
                script = @"(()=>{if(!document.querySelector('main')?.textContent.includes('Second document'))return false;document.querySelector('#go-up').click();return true;})()";
                break;
            case 4:
                script = @"(()=>{if(!document.querySelector('#directory-list a[href=\"/files/chapters/second.md\"]'))return false;document.querySelector('#go-home').click();return true;})()";
                break;
            case 6:
                script = @"(()=>{if(innerWidth>760)return false;if(document.documentElement.scrollWidth>innerWidth+1)throw new Error('Narrow layout overflows');const toggle=document.querySelector('#toggle-files');if(toggle.getAttribute('aria-expanded')!=='false')return false;toggle.click();const backdrop=document.querySelector('#sidebar-backdrop');if(backdrop.hidden || toggle.getAttribute('aria-expanded')!=='true')throw new Error('Narrow sidebar did not open');backdrop.click();if(toggle.getAttribute('aria-expanded')!=='false')throw new Error('Backdrop did not close sidebar');document.dispatchEvent(new KeyboardEvent('keydown',{key:'k',metaKey:true,bubbles:true}));if(document.activeElement!==document.querySelector('#file-search') || toggle.getAttribute('aria-expanded')!=='true')throw new Error('Search shortcut failed');document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}));if(toggle.getAttribute('aria-expanded')!=='false')throw new Error('Escape did not close sidebar');return true;})()";
                break;
            default:
                script = @"document.querySelector('#directory-list')?.textContent.includes('chapters')";
            }
            if (state->stage == -5)
                script = [[NSString stringWithUTF8String:resize_checks] stringByAppendingString:script];
            if (state->stage == -5 || state->stage == 6)
                script = [[NSString stringWithUTF8String:typography_checks] stringByAppendingString:script];
            if (state->stage == -1)
                script = [[NSString stringWithUTF8String:persisted_width_check] stringByAppendingString:script];
            if (state->stage == 0)
                script = [[[NSString stringWithUTF8String:math_checks]
                    stringByAppendingString:@"if (document.fonts.status !== 'loaded') { false; } else "]
                    stringByAppendingString:script];
            if (state->stage == 1)
                script = [[NSString stringWithUTF8String:updated_math_check] stringByAppendingString:script];
            [view evaluateJavaScript:script completionHandler:^(id result, NSError* error) {
                state->pending = false;
                // A click may replace the JavaScript context while its result is in flight.
                if (error) {
                    if ([error.domain isEqualToString:WKErrorDomain] && error.code == WKErrorJavaScriptExceptionOccurred)
                        state->failure = error.localizedDescription;
                    return;
                }
                if (![result respondsToSelector:@selector(boolValue)] || ![result boolValue]) return;
                if (state->stage == -5) {
                    state->pending = true;
                    screenshot(view, @"directory.png", ^(NSString* error) {
                        state->pending = false;
                        if (error) state->failure = error;
                        else ++state->stage;
                    });
                } else if (state->stage < 0) { ++state->stage; }
                else if (state->stage == 0) {
                    if (![window.title isEqualToString:@"native window.md — mdread"]) {
                        state->failure = @"Incorrect document window title";
                        return;
                    }
                    std::ofstream output(file);
                    output << "# Updated native document\n\n";
                    output << "Updated formula: $\\sqrt{x^2 + y^2}$.\n\n";
                    output << "<div id=\"updated-html\" style=\"color: rgb(12, 34, 56)\">Updated HTML</div>\n\n"
                              "<img src=\"missing.png\" onerror=\"window.htmlExecuted=true\">\n\n"
                              "<script>window.htmlExecuted=true</script>\n\n";
                    if (directory) output << "<a href=\"second.md\">Next document</a>\n\n";
                    for (int i = 0; i < 100; ++i) output << "Paragraph " << i << " preserved for scrolling.\n\n";
                    if (!output) { state->failure = @"Unable to update Markdown fixture"; return; }
                    state->stage = 1;
                    state->pending = true;
                    screenshot(view, directory ? @"directory-document.png" : @"document.png", ^(NSString* error) {
                        state->pending = false;
                        if (error) state->failure = error;
                    });
                } else if (directory && state->stage == 5) {
                    [window setContentSize:NSMakeSize(560, 640)];
                    ++state->stage;
                } else if (directory && state->stage == 6) {
                    state->pending = true;
                    screenshot(view, @"narrow-window.png", ^(NSString* error) {
                        state->pending = false;
                        if (error) { state->failure = error; return; }
                        state->stage = 7;
                        [window performClose:nil];
                        state->closed = !window.visible;
                    });
                } else if (!directory) {
                    state->stage = 2;
                    [window performClose:nil];
                    state->closed = !window.visible;
                } else { ++state->stage; }
            }];
            return false;
        });
        if (state->failure || state->stage != (directory ? 7 : 2) || !state->closed) {
            std::cerr << (state->failure ? state->failure.UTF8String : "Window did not close successfully") << '\n';
            return 1;
        }
        std::cout << "Native window: rendered Markdown, live update, preserved scroll, close passed\n";
        std::cout << "HTML: details, inline styles, anchors, images, escaped code and script filtering passed\n";
        if (directory) std::cout << "Directory browser: folder click, file click, Markdown link, return to root passed\n";
        if (directory) std::cout << "Controls: search, sidebar, theme, back/forward, up/home, refresh and narrow layout passed\n";
    }
    return 0;
}
