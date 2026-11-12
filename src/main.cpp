#include "web.hpp"
#include "window.hpp"
#include "purify.hpp"
#include <md4c-html.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace {
std::atomic<bool> running{true};
static_assert(std::atomic<bool>::is_always_lock_free, "Signal handler needs a lock-free atomic");
void stop(int) { running.store(false, std::memory_order_relaxed); }

struct Socket {
    int fd;
    explicit Socket(int value) : fd(value) {}
    ~Socket() { if (fd >= 0) ::close(fd); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};

std::string escape_html(std::string_view text) {
    std::string out;
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string json_string(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 0x20) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
    return out + '"';
}

constexpr std::size_t max_file_size = 16 * 1024 * 1024;
std::string read_file(const fs::path& file) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) throw std::runtime_error("Файл недоступен: " + file.string());
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Не удалось прочитать: " + file.string());
    std::string result;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        result.append(buffer, static_cast<std::size_t>(input.gcount()));
        if (result.size() > max_file_size) throw std::runtime_error("Максимальный размер файла — 16 MiB");
    }
    if (input.bad()) throw std::runtime_error("Ошибка чтения: " + file.string());
    return result;
}

std::string render(std::string_view markdown) {
    std::string html;
    auto append = [](const MD_CHAR* data, MD_SIZE size, void* target) {
        static_cast<std::string*>(target)->append(data, size);
    };
    if (md_html(markdown.data(), static_cast<MD_SIZE>(markdown.size()), append, &html,
                MD_DIALECT_GITHUB, MD_HTML_FLAG_SKIP_UTF8_BOM) != 0)
        throw std::runtime_error("Не удалось разобрать Markdown");
    return html;
}

void respond(int fd, int code, std::string_view type, std::string_view body,
             std::string_view extra = {}) {
    const char* reason = "Error";
    switch (code) {
    case 200: reason = "OK"; break;
    case 304: reason = "Not Modified"; break;
    case 400: reason = "Bad Request"; break;
    case 403: reason = "Forbidden"; break;
    case 404: reason = "Not Found"; break;
    case 405: reason = "Method Not Allowed"; break;
    case 503: reason = "Service Unavailable"; break;
    }
    std::string header = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n"
        "Connection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
        "img-src 'self' https: http: data:; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'\r\n"
        "Referrer-Policy: no-referrer\r\nContent-Type: " + std::string(type) + "\r\n";
    if (code != 304) header += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    header += extra;
    header += "\r\n";
    for (std::string_view part : {std::string_view(header), body}) {
        while (!part.empty()) {
            const auto n = ::send(fd, part.data(), part.size(), 0);
            if (n < 0 && errno == EINTR && running) continue;
            if (n <= 0) return;
            part.remove_prefix(static_cast<std::size_t>(n));
        }
    }
}

std::string lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

std::string decode_path(std::string_view value) {
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '%') {
            if (i + 2 >= value.size() || digit(value[i+1]) < 0 || digit(value[i+2]) < 0)
                throw std::runtime_error("Invalid URL");
            c = static_cast<char>(digit(value[i+1]) * 16 + digit(value[i+2]));
            i += 2;
        }
        if (static_cast<unsigned char>(c) < 32 || c == '\\') throw std::runtime_error("Invalid URL");
        result += c;
    }
    return result;
}

std::string encode_path(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') result += static_cast<char>(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}

bool is_markdown(const fs::path& path) {
    const auto extension = lower(path.extension().string());
    return extension == ".md" || extension == ".markdown";
}

struct HttpError : std::runtime_error {
    int code;
    HttpError(int status, const std::string& message) : std::runtime_error(message), code(status) {}
};

struct Document {
    fs::path file;
    fs::path root;
    std::string source;
    std::string html;
    std::uint64_t revision = 0;
    bool directory_mode = false;
    fs::path cached_file;

    fs::path resolve(const std::string& relative) const {
        const fs::path path(relative);
        if (path.is_absolute()) throw HttpError(403, "Путь вне открытой директории");
        const auto candidate = fs::weakly_canonical(root / path);
        const auto within = candidate.lexically_relative(root);
        if (within.empty() || within.is_absolute() || *within.begin() == "..")
            throw HttpError(403, "Путь вне открытой директории");
        return candidate;
    }

    void reload(const fs::path& selected) {
        auto next = read_file(selected);
        if (revision && selected == cached_file && next == source) return;
        auto next_html = render(next);
        cached_file = selected;
        source = std::move(next);
        html = std::move(next_html);
        ++revision;
    }

    std::string listing(const std::string& relative) const {
        const auto folder = resolve(relative);
        if (!fs::is_directory(folder)) throw HttpError(404, "Директория недоступна");
        struct Entry { std::string name, path; bool directory; };
        std::vector<Entry> entries;
        for (const auto& entry : fs::directory_iterator(folder)) {
            const auto name = entry.path().filename().string();
            if (name.empty() || name.front() == '.') continue;
            try {
                const auto entry_path = fs::path(relative) / name;
                const auto safe = resolve(entry_path.generic_string());
                const bool directory = fs::is_directory(safe);
                if (!directory && (!is_markdown(entry.path()) || !fs::is_regular_file(safe))) continue;
                entries.push_back({name, entry_path.generic_string(), directory});
            } catch (const std::exception&) { continue; }
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.directory != b.directory) return a.directory > b.directory;
            const auto left = lower(a.name), right = lower(b.name);
            return left == right ? a.name < b.name : left < right;
        });
        std::string result = "{\"entries\":[";
        bool first = true;
        for (const auto& entry : entries) {
            if (!first) result += ',';
            first = false;
            result += "{\"name\":" + json_string(entry.name) + ",\"path\":" + json_string(entry.path)
                + ",\"directory\":" + (entry.directory ? "true" : "false")
                + ",\"url\":" + json_string("/files/" + encode_path(entry.path) + (entry.directory ? "/" : "")) + "}";
        }
        return result + "]}";
    }

    void serve_page(int fd, const std::string& selected, const std::string& folder) {
        const bool reading = !directory_mode || !selected.empty();
        if (reading) reload(directory_mode ? resolve(selected) : file);
        const auto root_name = root.filename().empty() ? root.string() : root.filename().string();
        const auto name = directory_mode
            ? (reading ? fs::path(selected).filename().string()
                       : (folder.empty() ? root_name : fs::path(folder).filename().string()))
            : file.filename().string();
        const auto title = escape_html(name);
        const std::string explorer = directory_mode
            ? "<aside id=\"explorer\" aria-label=\"Файлы\"><a class=\"root-link\" href=\"/\" title=\"В корень открытой папки\">"
              "<span class=\"brand-mark\" aria-hidden=\"true\">m↓</span><span>mdread</span></a>"
              "<div class=\"explorer-heading\"><span class=\"eyebrow\">РАБОЧАЯ ПАПКА</span><strong class=\"folder-name\">"
              + escape_html(root_name) + "</strong></div><nav id=\"breadcrumbs\" aria-label=\"Путь\"></nav>"
              "<label class=\"search-field\"><svg viewBox=\"0 0 24 24\" aria-hidden=\"true\"><circle cx=\"10\" cy=\"10\" r=\"6\"/><path d=\"m15 15 5 5\"/></svg><input id=\"file-search\" type=\"search\" "
              "placeholder=\"Поиск в папке\" aria-label=\"Найти файл в текущей папке\" autocomplete=\"off\"></label>"
              "<div id=\"file-count\" class=\"list-caption\"></div><div id=\"files\">Загрузка файлов…</div>"
              "<div class=\"explorer-hint\">" + (reading ? "Изменения появляются после сохранения" : "Выберите файл, чтобы начать чтение")
              + "</div></aside><div id=\"sidebar-resizer\" role=\"separator\" aria-label=\"Ширина списка файлов\" "
                "aria-orientation=\"vertical\" aria-controls=\"explorer\" tabindex=\"0\" "
                "title=\"Потяните для изменения ширины. Двойной щелчок — сброс\"></div>" : "";
        const std::string page = "<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>" + title +
            " — mdread</title><link rel=\"stylesheet\" href=\"/__mdread/style.css\">"
            "<script defer src=\"/__mdread/purify.js\"></script>"
            "<script defer src=\"/__mdread/app.js\"></script></head><body class=\"" + (directory_mode ? "with-explorer" : "")
            + "\" data-directory-mode=\"" + (directory_mode ? "true" : "false")
            + "\" data-folder=\"" + escape_html(folder) + "\" data-selected=\"" + escape_html(selected)
            + "\" data-reading=\"" + (reading ? "true" : "false") + "\">" + explorer
            + "<a class=\"skip-link\" href=\"#reader-main\">К документу</a>"
            + std::string(toolbar_start) + "<div class=\"document-heading\"><span class=\"eyebrow\">"
            + (reading ? "ДОКУМЕНТ" : "ОБЗОР ПАПКИ") + "</span><span id=\"filename\" title=\"" + title + "\">" + title
            + "</span></div>" + std::string(toolbar_end)
            + "<div id=\"notice\" role=\"status\" hidden></div><main id=\"reader-main\" tabindex=\"-1\">"
            // Raw document HTML is inserted only after sanitization in the web view.
            + (reading ? "<p class=\"muted\">Загрузка документа…</p>" : "<h1>Документы</h1><p class=\"muted\">Выберите файл для чтения или откройте папку.</p><div id=\"directory-list\"></div>")
            + "</main><button id=\"sidebar-backdrop\" aria-label=\"Закрыть список файлов\" tabindex=\"-1\" hidden></button></body></html>";
        respond(fd, 200, "text/html; charset=utf-8", page);
    }

    void serve(int fd, const std::string& host) {
        std::string request;
        char buffer[2048];
        while (request.find("\r\n\r\n") == std::string::npos) {
            const auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n < 0 && errno == EINTR && running) continue;
            if (n <= 0) return;
            request.append(buffer, static_cast<std::size_t>(n));
            if (request.size() > 16384) {
                respond(fd, 400, "text/plain", "Request too large");
                return;
            }
        }
        std::istringstream lines(request);
        std::string method, target, version, line;
        std::getline(lines, line);
        std::istringstream first(line);
        first >> method >> target >> version;
        std::map<std::string, std::string> headers;
        while (std::getline(lines, line) && line != "\r") {
            const auto colon = line.find(':');
            if (colon == std::string::npos) { respond(fd, 400, "text/plain", "Invalid header"); return; }
            auto value = line.substr(colon + 1);
            const auto start = value.find_first_not_of(" \t");
            value = start == std::string::npos ? "" : value.substr(start);
            const auto end = value.find_last_not_of(" \t\r");
            value = end == std::string::npos ? "" : value.substr(0, end + 1);
            const auto name = lower(line.substr(0, colon));
            if (!headers.emplace(name, value).second) { respond(fd, 400, "text/plain", "Duplicate header"); return; }
        }
        if (headers["host"] != host) { respond(fd, 403, "text/plain", "Invalid Host"); return; }
        if (method != "GET") { respond(fd, 405, "text/plain", "GET only", "Allow: GET\r\n"); return; }
        if (version != "HTTP/1.1" || target.empty() || target.front() != '/') {
            respond(fd, 400, "text/plain", "Invalid request"); return;
        }
        const auto query_start = target.find('?');
        const auto query = query_start == std::string::npos ? "" : target.substr(query_start + 1);
        target = target.substr(0, query_start);
        auto requested_path = [&]() -> std::string {
            if (query.empty()) return "";
            if (query.rfind("path=", 0) != 0) throw HttpError(400, "Некорректный запрос");
            return decode_path(query.substr(5));
        };
        try {
            if (target == "/") {
                serve_page(fd, "", "");
            } else if (target == "/__mdread/style.css") {
                respond(fd, 200, "text/css; charset=utf-8", stylesheet);
            } else if (target == "/__mdread/app.js") {
                respond(fd, 200, "text/javascript; charset=utf-8", browser_script);
            } else if (target == "/__mdread/purify.js") {
                respond(fd, 200, "text/javascript; charset=utf-8", sanitizer_script);
            } else if (target == "/__mdread/content") {
                try {
                    const auto relative = directory_mode ? requested_path() : "";
                    if (directory_mode && !is_markdown(relative)) throw HttpError(404, "Укажите Markdown-файл");
                    reload(directory_mode ? resolve(relative) : file);
                    const auto etag = "\"" + std::to_string(revision) + "\"";
                    if (headers["if-none-match"] == etag) respond(fd, 304, "application/json", "", "ETag: " + etag + "\r\n");
                    else respond(fd, 200, "application/json; charset=utf-8", "{\"html\":" + json_string(html) + "}", "ETag: " + etag + "\r\n");
                } catch (const HttpError&) { throw; }
                catch (const std::exception& error) {
                    respond(fd, 503, "application/json; charset=utf-8", "{\"error\":" + json_string(error.what()) + "}");
                }
            } else if (directory_mode && target == "/__mdread/list") {
                respond(fd, 200, "application/json; charset=utf-8", listing(requested_path()));
            } else if (directory_mode && target.rfind("/files/", 0) == 0) {
                auto relative = decode_path(target.substr(7));
                const auto candidate = resolve(relative);
                if (fs::is_directory(candidate)) {
                    relative = fs::path(relative).lexically_normal().generic_string();
                    if (relative == ".") relative.clear();
                    while (!relative.empty() && relative.back() == '/') relative.pop_back();
                    serve_page(fd, "", relative);
                } else if (is_markdown(relative) && fs::is_regular_file(candidate)) {
                    serve_page(fd, relative, fs::path(relative).parent_path().generic_string());
                } else serve_image(fd, "/" + relative);
            } else {
                if (directory_mode) respond(fd, 404, "text/plain", "Not found");
                else serve_image(fd, decode_path(target));
            }
        } catch (const HttpError& error) {
            respond(fd, error.code, "application/json; charset=utf-8", "{\"error\":" + json_string(error.what()) + "}");
        } catch (const std::exception& error) {
            respond(fd, 404, "application/json; charset=utf-8", "{\"error\":" + json_string(error.what()) + "}");
        }
    }

    void serve_image(int fd, const std::string& target) const {
        try {
            const auto candidate = fs::canonical(root / target.substr(1));
            const auto relative = candidate.lexically_relative(root);
            if (relative.empty() || *relative.begin() == ".." || relative.is_absolute()) {
                respond(fd, 403, "text/plain", "Outside document directory"); return;
            }
            const std::map<std::string, std::string> types = {
                {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
                {".gif", "image/gif"}, {".webp", "image/webp"}, {".svg", "image/svg+xml"},
                {".avif", "image/avif"}, {".ico", "image/x-icon"}
            };
            const auto type = types.find(lower(candidate.extension().string()));
            if (type == types.end()) { respond(fd, 404, "text/plain", "Not found"); return; }
            respond(fd, 200, type->second, read_file(candidate));
        } catch (const std::exception&) { respond(fd, 404, "text/plain", "Not found"); }
    }
};

void usage() {
    std::cout << "Использование: mdread [--no-window] [--port N] <файл.md | директория>\n"
                 "  По умолчанию документ открывается в отдельном окне приложения\n"
                 "  --no-window   Запустить только сервер без окна\n"
                 "  --no-browser Старое имя опции --no-window\n"
                 "  --port N      Порт 0–65535 (0: выбрать свободный, по умолчанию)\n"
                 "  --            Конец опций, далее путь\n"
                 "  -h, --help    Справка\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        bool window = true, options = true;
        unsigned port = 0;
        fs::path file;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (options && (arg == "--help" || arg == "-h")) { usage(); return 0; }
            if (options && arg == "--") options = false;
            else if (options && (arg == "--no-window" || arg == "--no-browser")) window = false;
            else if (options && arg == "--port") {
                if (++i == argc) throw std::runtime_error("После --port нужен номер порта");
                const std::string number = argv[i];
                if (number.empty() || number.size() > 5 || number.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("Некорректный порт");
                port = static_cast<unsigned>(std::stoul(number));
                if (port > 65535) throw std::runtime_error("Порт должен быть от 0 до 65535");
            } else if (options && !arg.empty() && arg.front() == '-') {
                throw std::runtime_error("Неизвестная опция: " + arg);
            } else {
                if (!file.empty()) throw std::runtime_error("Укажите один файл или директорию");
                file = arg;
            }
        }
        if (file.empty()) { usage(); return 1; }
        // Keep the requested filename: editors often save by replacing its inode.
        file = fs::absolute(file).lexically_normal();
        const bool directory = fs::is_directory(file);
        Document doc{file, fs::canonical(directory ? file : file.parent_path()), {}, {}, 0, directory, {}};
        if (!directory) doc.reload(file);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        std::signal(SIGPIPE, SIG_IGN);
        Socket server(::socket(AF_INET, SOCK_STREAM, 0));
        if (server.fd < 0) throw std::runtime_error("Не удалось создать сокет");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<std::uint16_t>(port));
        if (::bind(server.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            throw std::runtime_error("Не удалось занять порт " + std::to_string(port) + ": " + std::strerror(errno));
        if (::listen(server.fd, 16) < 0) throw std::runtime_error("Не удалось запустить сервер");
        socklen_t length = sizeof(address);
        if (::getsockname(server.fd, reinterpret_cast<sockaddr*>(&address), &length) < 0)
            throw std::runtime_error("Не удалось определить порт");
        const std::string host = "127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        const auto url = "http://" + host + "/";
        std::cout << url << '\n' << (directory ? "Директория: " : "Файл: ") << file.string() << "\nОстановка: Ctrl+C\n" << std::flush;
        auto serve = [&] {
            while (running) {
                pollfd ready{server.fd, POLLIN, 0};
                const int result = ::poll(&ready, 1, 200);
                if (result < 0 && errno == EINTR) continue;
                if (result < 0) throw std::runtime_error("Ошибка ожидания соединения");
                if (!result || !(ready.revents & POLLIN)) continue;
                Socket client(::accept(server.fd, nullptr, nullptr));
                if (client.fd < 0) continue;
                timeval timeout{1, 0};
                ::setsockopt(client.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                ::setsockopt(client.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
                doc.serve(client.fd, host);
            }
        };
        if (!window) {
            serve();
        } else {
            std::exception_ptr server_error;
            std::thread worker([&] {
                try { serve(); }
                catch (...) { server_error = std::current_exception(); running = false; }
            });
            try {
                run_native_window(url, file.filename().string() + " — mdread", [] { return !running.load(); });
            } catch (...) {
                running = false;
                worker.join();
                throw;
            }
            running = false;
            worker.join();
            if (server_error) std::rethrow_exception(server_error);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mdread: " << error.what() << '\n';
        return 1;
    }
}
