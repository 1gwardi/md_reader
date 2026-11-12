#pragma once

// Inspect an arbitrary document without editing it. Used for real-world math
// regressions in addition to the synthetic native-window fixture.
int check_math_corpus(const char* url, const char* filename) {
    @autoreleasepool {
        struct State {
            bool busy = false;
            bool finished = false;
            NSString* failure = nil;
            NSString* report = nil;
        } storage;
        State* state = &storage;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        NSString* name = [[NSString stringWithUTF8String:filename] lastPathComponent];
        run_native_window(url, "mdread math corpus", [&] {
            if (std::chrono::steady_clock::now() > deadline) {
                state->failure = @"Timed out waiting for document math";
                return true;
            }
            if (state->finished) return true;
            if (state->busy) return false;
            WKWebView* view = (WKWebView*)NSApp.keyWindow.contentView;
            if (![view isKindOfClass:WKWebView.class]) return false;
            state->busy = true;
            [view evaluateJavaScript:@R"JS(
(() => {
  if (!document.querySelector('#status')?.textContent.startsWith('Автообновление') || document.fonts.status !== 'loaded') return null;
  const equations = [...document.querySelectorAll('main x-equation')];
  const errors = equations.filter(e => e.querySelector('.katex-error')).map(e => e.textContent.slice(0,160));
  const sample = equations.find(e => e.querySelector('msub') && e.textContent.includes('standoff')) ?? equations[0];
  sample?.scrollIntoView({block:'center'});
  return JSON.stringify({equations:equations.length, subscripts:document.querySelectorAll('main msub,main msubsup').length, errors});
})()
)JS" completionHandler:^(id result, NSError* error) {
                state->busy = false;
                if (error || ![result isKindOfClass:NSString.class]) return;
                state->report = result;
                NSDictionary* report = [NSJSONSerialization JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
                if ([report[@"equations"] integerValue] < 1 || [report[@"subscripts"] integerValue] < 1
                    || [report[@"errors"] count] > 0) {
                    state->failure = @"Missing subscripts or invalid formula in corpus document";
                    state->finished = true;
                    return;
                }
                state->busy = true;
                screenshot(view, [name stringByAppendingString:@".png"], ^(NSString* error) {
                    state->busy = false;
                    state->failure = error;
                    state->finished = true;
                });
            }];
            return false;
        });
        if (state->report) std::cout << name.UTF8String << ": " << state->report.UTF8String << '\n';
        if (state->failure) { std::cerr << state->failure.UTF8String << '\n'; return 1; }
        return 0;
    }
}
