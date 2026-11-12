#include "window.hpp"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdexcept>

namespace {
struct State {
    const std::string& url;
    const std::function<bool()>& should_stop;
};

gboolean navigation(WebKitWebView*, WebKitPolicyDecision* decision,
                    WebKitPolicyDecisionType type, gpointer data) {
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION
        && type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) return FALSE;
    const auto* state = static_cast<State*>(data);
    auto* action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    const char* uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
    const std::string target = uri ? uri : "";
    const bool local = target.rfind(state->url, 0) == 0;
    if (target == state->url || target.rfind(state->url + "#", 0) == 0
        || target.rfind(state->url + "files/", 0) == 0) {
        webkit_policy_decision_use(decision);
    } else {
        if (!local && webkit_navigation_action_get_navigation_type(action) == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED
            && (target.rfind("https://", 0) == 0 || target.rfind("http://", 0) == 0 || target.rfind("mailto:", 0) == 0)) {
            GError* error = nullptr;
            g_app_info_launch_default_for_uri(target.c_str(), nullptr, &error);
            if (error) g_error_free(error);
        }
        webkit_policy_decision_ignore(decision);
    }
    return TRUE;
}
} // namespace

void run_native_window(const std::string& url, const std::string& title,
                       const std::function<bool()>& should_stop) {
    if (!gtk_init_check(nullptr, nullptr)) throw std::runtime_error("Нет доступа к графическому сеансу; используйте --no-window");
    State state{url, should_stop};
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), 960, 760);
    auto* context = webkit_web_context_new_ephemeral();
    GtkWidget* view = webkit_web_view_new_with_context(context);
    g_object_unref(context);
    gtk_container_add(GTK_CONTAINER(window), view);
    g_signal_connect(view, "decide-policy", G_CALLBACK(navigation), &state);
    g_signal_connect(view, "notify::title", G_CALLBACK(+[](WebKitWebView* view, GParamSpec*, gpointer data) {
        const char* title = webkit_web_view_get_title(view);
        if (title) gtk_window_set_title(GTK_WINDOW(data), title);
    }), window);
    const auto close_handler = g_signal_connect(window, "delete-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer) -> gboolean {
        gtk_main_quit();
        return TRUE;
    }), nullptr);
    const auto timer = g_timeout_add(100, +[](gpointer data) -> gboolean {
        if (static_cast<State*>(data)->should_stop()) gtk_main_quit();
        return G_SOURCE_CONTINUE;
    }, &state);
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(view), url.c_str());
    gtk_widget_show_all(window);
    if (!should_stop()) gtk_main();
    g_source_remove(timer);
    g_signal_handler_disconnect(window, close_handler);
    gtk_widget_destroy(window);
}
