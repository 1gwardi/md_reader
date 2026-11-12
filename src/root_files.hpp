#pragma once

#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <filesystem>
#include <memory>
#include <vector>
#include <utility>
#include <stdexcept>
#include <string>

struct RootFileError : std::runtime_error {
    int code;
    RootFileError(int value, const std::string& message) : std::runtime_error(message), code(value) {}
};

struct FileHandle {
    int fd = -1;
    explicit FileHandle(int value = -1) : fd(value) {}
    ~FileHandle() { if (fd >= 0) ::close(fd); }
    FileHandle(FileHandle&& other) noexcept : fd(other.fd) { other.fd = -1; }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    struct stat stat() const {
        struct stat result{};
        if (::fstat(fd, &result) != 0) throw std::runtime_error("Не удалось проверить файл");
        return result;
    }
};

// No pathname is reopened after validation. Every component is opened relative
// to the already pinned directory; symlinks (including internal ones) are denied.
class RootFiles {
    FileHandle root_;
#ifdef __APPLE__
    static constexpr int search_flags = O_SEARCH;
#elif defined(O_PATH)
    static constexpr int search_flags = O_PATH | O_DIRECTORY;
#else
    static constexpr int search_flags = O_RDONLY | O_DIRECTORY;
#endif
public:
    explicit RootFiles(const std::filesystem::path& root)
        : root_(::open(root.c_str(), search_flags | O_CLOEXEC | O_NOFOLLOW)) {
        if (root_.fd < 0) throw std::runtime_error("Не удалось открыть корневую папку");
    }
    static std::filesystem::path relative(const std::string& name) {
        auto path = std::filesystem::path(name).lexically_normal();
        if (path.is_absolute() || (!path.empty() && *path.begin() == ".."))
            throw std::runtime_error("Путь вне открытой директории");
        return path;
    }
    FileHandle open(const std::string& name) const {
        const auto path = relative(name);
        FileHandle directory(::fcntl(root_.fd, F_DUPFD_CLOEXEC, 0));
        std::vector<std::string> parts;
        for (const auto& part : path) if (part != "." && !part.empty()) parts.push_back(part.string());
        if (parts.empty()) return directory;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const bool parent = i + 1 < parts.size();
            FileHandle next(::openat(directory.fd, parts[i].c_str(), O_CLOEXEC | O_NOFOLLOW
                | (parent ? search_flags : (O_RDONLY | O_NONBLOCK))));
            if (next.fd < 0) {
                struct stat link{};
                const bool symlink = errno == ELOOP || (::fstatat(directory.fd, parts[i].c_str(), &link, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(link.st_mode));
                throw RootFileError(symlink ? 403 : 503, "Файл недоступен или содержит симлинк: " + name);
            }
            if (!parent) return next;
            std::swap(directory.fd, next.fd);
        }
        throw std::runtime_error("Некорректный путь");
    }
};

inline std::string file_signature(const struct stat& st) {
#ifdef __APPLE__
    const auto modified = st.st_mtimespec, changed = st.st_ctimespec;
#else
    const auto modified = st.st_mtim, changed = st.st_ctim;
#endif
    return std::to_string(st.st_dev) + ":" + std::to_string(st.st_ino) + ":" + std::to_string(st.st_size)
        + ":" + std::to_string(modified.tv_sec) + ":" + std::to_string(modified.tv_nsec)
        + ":" + std::to_string(changed.tv_sec) + ":" + std::to_string(changed.tv_nsec);
}
