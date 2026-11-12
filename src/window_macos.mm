#include "window.hpp"

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

@interface MDReadDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, WKNavigationDelegate>
@property(nonatomic, strong) NSURL* documentURL;
- (void)stopApplication;
@end

@implementation MDReadDelegate
- (void)stopApplication {
    [NSApp stop:nil];
    // Wake the event loop even when stop was called by a timer or window delegate.
    [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
        location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
        context:nil subtype:0 data1:0 data2:0] atStart:YES];
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    [self stopApplication];
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    [self stopApplication];
    // Let main join the server thread and release its resources.
    return NSTerminateCancel;
}
- (void)webView:(WKWebView*)webView decidePolicyForNavigationAction:(WKNavigationAction*)action
    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    (void)webView;
    NSURL* url = action.request.URL;
    const BOOL document = [url.scheme isEqualToString:self.documentURL.scheme]
        && [url.host isEqualToString:self.documentURL.host]
        && [url.port isEqual:self.documentURL.port]
        && [url.path isEqualToString:self.documentURL.path];
    if (document) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }
    // External links leave the reader available at the current document.
    if (action.navigationType == WKNavigationTypeLinkActivated
        && ([url.scheme isEqualToString:@"https"] || [url.scheme isEqualToString:@"http"]
            || [url.scheme isEqualToString:@"mailto"])) {
        [NSWorkspace.sharedWorkspace openURL:url];
    }
    decisionHandler(WKNavigationActionPolicyCancel);
}
@end

void run_native_window(const std::string& url, const std::string& title,
                       const std::function<bool()>& should_stop) {
    @autoreleasepool {
        NSApplication* app = NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        MDReadDelegate* delegate = [MDReadDelegate new];
        delegate.documentURL = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        app.delegate = delegate;

        NSMenu* menu = [NSMenu new];
        NSMenuItem* appItem = [NSMenuItem new];
        [menu addItem:appItem];
        NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"mdread"];
        [appMenu addItemWithTitle:@"Закрыть окно" action:@selector(performClose:) keyEquivalent:@"w"];
        [appMenu addItem:NSMenuItem.separatorItem];
        [appMenu addItemWithTitle:@"Завершить mdread" action:@selector(terminate:) keyEquivalent:@"q"];
        appItem.submenu = appMenu;
        NSMenuItem* editItem = [[NSMenuItem alloc] initWithTitle:@"Правка" action:nil keyEquivalent:@""];
        NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Правка"];
        [editMenu addItemWithTitle:@"Копировать" action:@selector(copy:) keyEquivalent:@"c"];
        [editMenu addItemWithTitle:@"Выделить всё" action:@selector(selectAll:) keyEquivalent:@"a"];
        editItem.submenu = editMenu;
        [menu addItem:editItem];
        app.mainMenu = menu;

        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 960, 760)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        window.title = [NSString stringWithUTF8String:title.c_str()];
        window.minSize = NSMakeSize(420, 300);
        window.delegate = delegate;
        [window center];

        WKWebViewConfiguration* configuration = [WKWebViewConfiguration new];
        configuration.websiteDataStore = WKWebsiteDataStore.nonPersistentDataStore;
        WKWebView* view = [[WKWebView alloc] initWithFrame:window.contentView.bounds configuration:configuration];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.navigationDelegate = delegate;
        window.contentView = view;
        [view loadRequest:[NSURLRequest requestWithURL:delegate.documentURL]];

        NSTimer* timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*) {
            if (should_stop()) [delegate stopApplication];
        }];
        [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
        [app finishLaunching];
        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        if (!should_stop()) [app run];
        [timer invalidate];
        view.navigationDelegate = nil;
        [view stopLoading];
        window.delegate = nil;
        [window close];
        app.delegate = nil;
    }
}
