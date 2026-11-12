#include "web.hpp"
#include <md4c-html.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

extern char** environ;
namespace fs = std::filesystem;
namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }

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
                MD_DIALECT_GITHUB | MD_FLAG_NOHTML, MD_HTML_FLAG_SKIP_UTF8_BOM) != 0)
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
        "Content-Security-Policy: default-src 'none'; script-src 'self'; style-src 'self'; "
        "img-src 'self' https: http: data:; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'\r\n"
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

struct Document {
    fs::path file;
    fs::path root;
    std::string source;
    std::string html;
    std::uint64_t revision = 0;

    void reload() {
        auto next = read_file(file);
        if (revision && next == source) return;
        auto next_html = render(next);
        source = std::move(next);
        html = std::move(next_html);
        ++revision;
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
        target = target.substr(0, target.find('?'));
        if (target == "/") {
            const auto title = escape_html(file.filename().string());
            const std::string page = "<!doctype html><html lang=\"ru\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>" + title +
                " — mdread</title><link rel=\"stylesheet\" href=\"/__mdread/style.css\">"
                "<script defer src=\"/__mdread/app.js\"></script></head><body><header><span id=\"filename\">" +
                title + "</span><span id=\"status\" role=\"status\">Загрузка…</span></header>"
                "<main>" + html + "</main></body></html>";
            respond(fd, 200, "text/html; charset=utf-8", page);
        } else if (target == "/__mdread/style.css") {
            respond(fd, 200, "text/css; charset=utf-8", stylesheet);
        } else if (target == "/__mdread/app.js") {
            respond(fd, 200, "text/javascript; charset=utf-8", browser_script);
        } else if (target == "/__mdread/content") {
            try {
                reload();
                const auto etag = "\"" + std::to_string(revision) + "\"";
                if (headers["if-none-match"] == etag) respond(fd, 304, "application/json", "", "ETag: " + etag + "\r\n");
                else respond(fd, 200, "application/json; charset=utf-8", "{\"html\":" + json_string(html) + "}", "ETag: " + etag + "\r\n");
            } catch (const std::exception& error) {
                respond(fd, 503, "application/json; charset=utf-8", "{\"error\":" + json_string(error.what()) + "}");
            }
        } else {
            serve_image(fd, target);
        }
    }

    void serve_image(int fd, const std::string& target) const {
        try {
            const auto decoded = decode_path(target);
            const auto candidate = fs::canonical(root / decoded.substr(1));
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

pid_t open_browser(const std::string& url) {
#ifdef __APPLE__
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    char* args[] = {const_cast<char*>(opener), const_cast<char*>(url.c_str()), nullptr};
    pid_t child;
    if (posix_spawnp(&child, opener, nullptr, nullptr, args, environ) != 0) return -1;
    return child;
}

void usage() {
    std::cout << "Использование: mdread [--no-browser] [--port N] <файл.md>\n"
                 "  --no-browser  Только вывести URL\n"
                 "  --port N      Порт 0–65535 (0: выбрать свободный, по умолчанию)\n"
                 "  --            Конец опций, далее имя файла\n"
                 "  -h, --help    Справка\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        bool browser = true, options = true;
        unsigned port = 0;
        fs::path file;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (options && (arg == "--help" || arg == "-h")) { usage(); return 0; }
            if (options && arg == "--") options = false;
            else if (options && arg == "--no-browser") browser = false;
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
                if (!file.empty()) throw std::runtime_error("Укажите один Markdown-файл");
                file = arg;
            }
        }
        if (file.empty()) { usage(); return 1; }
        // Keep the requested filename: editors often save by replacing its inode.
        file = fs::absolute(file).lexically_normal();
        Document doc{file, fs::canonical(file.parent_path()), {}, {}, 0};
        doc.reload();
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
        std::cout << url << '\n' << "Файл: " << file.string() << "\nОстановка: Ctrl+C\n" << std::flush;
        pid_t opener = browser ? open_browser(url) : -1;
        if (browser && opener < 0) std::cerr << "Откройте URL выше в браузере.\n";
        while (running) {
            if (opener > 0) {
                int status = 0;
                if (waitpid(opener, &status, WNOHANG) == opener) {
                    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
                        std::cerr << "Откройте URL выше в браузере.\n";
                    opener = -1;
                }
            }
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
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mdread: " << error.what() << '\n';
        return 1;
    }
}
