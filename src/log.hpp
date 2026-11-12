#pragma once

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <syncstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define FWRITE_UNLOCKED _fwrite_nolock
#else
#define FWRITE_UNLOCKED fwrite_unlocked
#endif

namespace detail {
struct LogBlock {
    inline constexpr static size_t kCap = 64 * 1024;

    bool append(std::string_view s) {
        if (len + s.size() > kCap) {
            return false;
        }
        memcpy(data + len, s.data(), s.size());
        len += s.size();
        return true;
    }

    void clear() noexcept {
        len = 0;
    }

    bool empty() const noexcept {
        return len == 0;
    }

    size_t len{0};
    char data[kCap]{0};
};

// SPSC
template <typename T>
class SpscQueue {
  public:
    struct Node {
        Node(T &&v, Node *n) : value(std::move(v)), next(n) {
        }

        T value;
        Node *next;
    };

    // ---- Producer(single thread) ----
    void push(T v) {
        auto *n = new Node(std::move(v), pending_);
        if (!pending_tail_) {
            pending_tail_ = n;
        }
        pending_ = n;
    }

    void flush() {
        if (!pending_) {
            return;
        }
        pending_tail_->next = head_.load(std::memory_order_acquire);
        head_.store(pending_, std::memory_order_release);
        pending_ = nullptr;
        pending_tail_ = nullptr;
    }

    // ---- Consumer(single thread) ----
    Node *drain() {
        return head_.exchange(nullptr, std::memory_order_acquire);
    }

  private:
    std::atomic<Node *> head_{nullptr};
    Node *pending_{nullptr};
    Node *pending_tail_{nullptr};
};

// reverse
template <typename T>
typename SpscQueue<T>::Node *to_fifo(typename SpscQueue<T>::Node *list) {
    typename SpscQueue<T>::Node *fifo = nullptr;
    while (list) {
        auto *n = list;
        list = list->next;
        n->next = fifo;
        fifo = n;
    }
    return fifo;
}

inline void throw_system_error(const std::string &operation) {
    throw std::system_error(errno, std::system_category(), operation);
}

inline void throw_runtime_error(const std::string &message) {
    throw std::runtime_error(message);
}

inline std::filesystem::path to_platform_path(const std::string &utf8_path) {
#ifdef _WIN32
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t *>(utf8_path.data()), utf8_path.size()));
#else
    return std::filesystem::path(utf8_path);
#endif
}

inline FILE *open_append_binary(const std::string &utf8_path) {
#ifdef _WIN32
    return _wfopen(to_platform_path(utf8_path).c_str(), L"ab");
#else
    return std::fopen(to_platform_path(utf8_path).c_str(), "ab");
#endif
}

class FileAppender {
  public:
    explicit FileAppender(std::string path) : path_(std::move(path)) {
        file_ = open_append_binary(path_);
        if (!file_) {
            throw_system_error("Failed to open log file");
        }
    }

    ~FileAppender() {
        if (file_) {

            fclose(file_);
            file_ = nullptr;
        }
    }

    FileAppender(const FileAppender &) = delete;

    size_t written_bytes() const noexcept {
        return written_bytes_;
    }

    void reset_written_bytes() noexcept {
        written_bytes_ = 0;
    }

    void append(std::string_view data) {
        append(data.data(), data.size());
    }

    void append(const char *data, size_t len);

    void flush();

  private:
    std::string path_;
    FILE *file_{nullptr};
    size_t written_bytes_{0};
};

inline void FileAppender::append(const char *data, size_t len) {
    if (!data || !file_ || len == 0) {
        return;
    }

    const size_t written = FWRITE_UNLOCKED(data, 1, len, file_);

    if (written != len) {
        if (ferror(file_)) {
            throw_system_error("Failed to write log file");
        }
        throw_runtime_error("Partial write to log file");
    }

    written_bytes_ += len;
}

inline void FileAppender::flush() {
    if (!file_) {
        return;
    }

    if (fflush(file_) != 0) {
        throw_system_error("Failed to flush log file");
    }
}

class FileManager {
  public:
    explicit FileManager(
        std::string dir,
        std::string prefix,
        size_t roll_size = 100 * 1024 * 1024,
        std::chrono::seconds flush_interval = std::chrono::seconds(3),
        size_t check_per_count = 1024)
        : dir_(std::move(dir)), prefix_(std::move(prefix)),
          roll_size_(roll_size), flush_interval_(flush_interval),
          check_per_count_(check_per_count) {
        roll_file();
    }

    void append(std::string_view data) {
        append(data.data(), data.size());
    }

    void append(const char *data, size_t len);

    void flush() {
        file_->flush();
    }

  private:
    void roll_file(std::chrono::system_clock::time_point now =
                       std::chrono::system_clock::now());
    const std::string dir_;
    const std::string prefix_;
    const size_t roll_size_;
    const std::chrono::seconds flush_interval_;
    const size_t check_per_count_;

    size_t count_{0};

    std::unique_ptr<FileAppender> file_;
    std::string current_path_;
    int same_second_count_{0};

    std::chrono::time_point<std::chrono::system_clock, std::chrono::days>
        last_day_;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
        last_roll_second_;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
        last_flush_second_;
};

inline void FileManager::append(const char *data, size_t len) {
    file_->append(data, len);
    if (file_->written_bytes() > roll_size_) {
        roll_file();
        file_->reset_written_bytes();
    } else {
        count_++;
        if (count_ >= check_per_count_) {
            count_ = 0;
            auto now = std::chrono::system_clock::now();
            auto now_day = std::chrono::floor<std::chrono::days>(now);
            if (now_day != last_day_) {
                roll_file(now);
            } else if (now - last_flush_second_ >= flush_interval_) {
                last_flush_second_ =
                    std::chrono::floor<std::chrono::seconds>(now);
                file_->flush();
            }
        }
    }
}

inline void FileManager::roll_file(std::chrono::system_clock::time_point now) {
    last_roll_second_ = last_flush_second_ =
        std::chrono::floor<std::chrono::seconds>(now);
    last_day_ = std::chrono::floor<std::chrono::days>(now);

    std::chrono::zoned_time zt{std::chrono::current_zone(), last_roll_second_};

    std::string path =
        std::format("{}/{}{:%Y-%m-%dT%H-%M-%S}.log", dir_, prefix_, zt);
    if (path == current_path_) {
        path = std::format("{}/{}{:%Y-%m-%dT%H-%M-%S}-{}.log",
                           dir_,
                           prefix_,
                           zt,
                           ++same_second_count_);
    } else {
        same_second_count_ = 0;
        current_path_ = path;
    }

    file_ = std::make_unique<FileAppender>(path);
}

class AsyncLogger;

struct ProducerCtx {
    LogBlock *take_free() {
        using Node = SpscQueue<LogBlock *>::Node;
        Node *stash = free_stash;
        if (!stash) {
            stash = free.drain();
        }
        if (!stash) {
            return nullptr;
        }
        LogBlock *blk = stash->value;
        free_stash = stash->next;
        delete stash;
        blk->clear();
        return blk;
    }

    std::uint64_t owner_id{0};

    std::mutex cur_mtx;
    LogBlock *cur{new LogBlock()};

    SpscQueue<LogBlock *> full{};
    SpscQueue<LogBlock *> free{};

    SpscQueue<LogBlock *>::Node *free_stash{nullptr};
};

class AsyncLogger {
  public:
    explicit AsyncLogger(
        std::string dir,
        std::string prefix,
        size_t roll_size = 100 * 1024 * 1024,
        std::chrono::seconds flush_interval = std::chrono::seconds(3),
        size_t check_per_count = 1024) {
        thread_ = std::jthread([this,
                                dir = std::move(dir),
                                prefix = std::move(prefix),
                                roll_size,
                                flush_interval,
                                check_per_count] {
            run(dir, prefix, roll_size, flush_interval, check_per_count);
        });
    }

    AsyncLogger(AsyncLogger &&) = delete;

    ~AsyncLogger() {
        if (!done_.load(std::memory_order_acquire)) {
            do_done();
        }
    }

    void append_message(std::string_view msg);

    void wait_for_done() {
        if (!done_.load(std::memory_order_acquire)) {
            do_done();
        }
    }

  private:
    ProducerCtx *register_producer();

    void run(const std::string &dir,
             const std::string &prefix,
             size_t roll_size,
             std::chrono::seconds flush_interval,
             size_t check_per_count);

    void do_done() {
        {
            std::lock_guard lock(cv_mtx_);
            done_.store(true, std::memory_order_release);
        }
        cv_.notify_one();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void stop_accepting() noexcept {
        accepting_.store(false, std::memory_order_release);
    }

    inline static std::uint64_t next_instance_id() noexcept {
        static std::atomic<std::uint64_t> seq{0};
        return seq.fetch_add(1, std::memory_order_relaxed);
    }

    const std::uint64_t id_{next_instance_id()};

    std::atomic<bool> done_{false};
    std::atomic<bool> accepting_{true};
    std::mutex cv_mtx_;
    std::condition_variable cv_;

    std::mutex reg_mtx_;
    std::vector<ProducerCtx *> producers_;

    std::jthread thread_;
};

inline ProducerCtx *AsyncLogger::register_producer() {
    auto *ctx = new ProducerCtx();
    ctx->owner_id = id_;
    {
        std::lock_guard lock(reg_mtx_);
        producers_.push_back(ctx);
    }
    return ctx;
}

inline void AsyncLogger::append_message(std::string_view msg) {
    if (!accepting_.load(std::memory_order_acquire)) {
        return;
    }

    thread_local ProducerCtx *ctx = nullptr;
    if (ctx == nullptr || ctx->owner_id != id_) {
        ctx = register_producer();
    }

    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(ctx->cur_mtx);

        if (ctx->cur->append(msg)) {
            return;
        }

        std::string_view rest = msg;
        while (!rest.empty()) {
            const size_t space = LogBlock::kCap - ctx->cur->len;
            if (space == 0) {
                ctx->full.push(ctx->cur);
                wake = true;

                LogBlock *fresh = ctx->take_free();
                if (!fresh) {
                    fresh = new LogBlock();
                }
                ctx->cur = fresh;
                continue;
            }

            const std::string_view chunk = rest.substr(0, space);
            ctx->cur->append(chunk);
            rest.remove_prefix(chunk.size());
        }
    }

    if (wake) {
        ctx->full.flush();
        cv_.notify_one();
    }
}

inline void AsyncLogger::run(const std::string &dir,
                             const std::string &prefix,
                             size_t roll_size,
                             std::chrono::seconds flush_interval,
                             size_t check_per_count) {
    std::unique_ptr<FileManager> out_file;
    try {
        out_file = std::make_unique<FileManager>(
            dir, prefix, roll_size, flush_interval, check_per_count);
    } catch (const std::exception &e) {
        std::cerr << std::format("file logging disabled: {}", e.what())
                  << std::endl;
    }

    if (!out_file) {
        stop_accepting();
        return;
    }

    bool file_ok = true;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(cv_mtx_);
            cv_.wait_for(lock, flush_interval, [this] {
                return done_.load(std::memory_order_acquire);
            });
        }

        if (done_.load(std::memory_order_acquire)) {
            break;
        }

        try {
            std::vector<ProducerCtx *> snapshot;
            {
                std::lock_guard lock(reg_mtx_);
                snapshot = producers_;
            }

            bool any = false;
            for (auto *ctx : snapshot) {
                for (auto *n = to_fifo<LogBlock *>(ctx->full.drain()); n;) {
                    auto *node = n;
                    n = n->next;
                    out_file->append(node->value->data, node->value->len);
                    ctx->free.push(node->value);
                    delete node;
                    any = true;
                }
                ctx->free.flush();

                LogBlock *taken = nullptr;
                {
                    std::lock_guard<std::mutex> lock(ctx->cur_mtx);
                    if (!ctx->cur->empty()) {
                        taken = ctx->cur;
                        ctx->cur = new LogBlock();
                    }
                }
                if (taken) {
                    out_file->append(taken->data, taken->len);
                    ctx->free.push(taken);
                    ctx->free.flush();
                    any = true;
                }
            }

            if (any) {
                out_file->flush();
            }
        } catch (const std::exception &e) {
            std::cerr << std::format("file logging disabled: {}", e.what())
                      << std::endl;
            file_ok = false;
            break;
        }
    }

    stop_accepting();

    if (file_ok) {
        try {
            std::vector<ProducerCtx *> snapshot;
            {
                std::lock_guard lock(reg_mtx_);
                snapshot = producers_;
            }
            for (auto *ctx : snapshot) {
                for (auto *n = to_fifo<LogBlock *>(ctx->full.drain()); n;) {
                    auto *node = n;
                    n = n->next;
                    out_file->append(node->value->data, node->value->len);
                    delete node;
                }

                std::lock_guard<std::mutex> lock(ctx->cur_mtx);
                if (!ctx->cur->empty()) {
                    out_file->append(ctx->cur->data, ctx->cur->len);
                }
            }
            out_file->flush();
        } catch (...) {
        }
    }
}

#ifdef _WIN32
extern "C" {
__declspec(dllimport) void *__stdcall GetStdHandle(unsigned long n_std_handle);
__declspec(dllimport) int __stdcall GetConsoleMode(void *h_console_handle,
                                                   unsigned long *lp_mode);
__declspec(dllimport) int __stdcall SetConsoleMode(void *h_console_handle,
                                                   unsigned long dw_mode);
}
#endif

#if defined(ERROR)
#undef ERROR
#endif

#define _FOREACH_LOG_LEVEL(f)                                                  \
    f(TRACE) f(DEBUG) f(INFO) f(WARN) f(ERROR) f(FATAL)

enum class LogLevel : std::uint8_t {
#define _FUNCTION(name) name,
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION
};

inline constexpr const char
    level_ansi_colors[static_cast<std::uint8_t>(LogLevel::FATAL) + 1][6] = {
        "\033[90m", "\033[36m", "\033[32m", "\033[33m", "\033[31m", "\033[35m"};

inline constexpr std::string_view log_level_to_string(LogLevel lv) {
    switch (lv) {
#define _FUNCTION(name)                                                        \
    case LogLevel::name:                                                       \
        return #name;
        _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION
    }
    return "UNKNOWN";
}

inline constexpr LogLevel log_level_from_string(std::string_view lv) {
#define _FUNCTION(name)                                                        \
    if (lv == #name)                                                           \
        return LogLevel::name;
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION

    return LogLevel::INFO;
}

inline std::string env_utf8(const char *name) {
#ifdef _WIN32
    std::wstring wname;
    for (const char *p = name; *p != '\0'; ++p) {
        wname.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
    }
    const wchar_t *const value = ::_wgetenv(wname.c_str());
    if (value == nullptr) {
        return {};
    }
    const std::u8string utf8 = std::filesystem::path(value).u8string();
    return std::string(reinterpret_cast<const char *>(utf8.data()),
                       utf8.size());
#else
    const char *const value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string{};
#endif
}

inline bool detect_ansi_support() noexcept {
#ifdef _WIN32
    constexpr unsigned long std_output_handle =
        static_cast<unsigned long>(-11); // STD_OUTPUT_HANDLE
    constexpr unsigned long enable_virtual_terminal_processing = 0x0004;

    const void *const invalid_handle =
        reinterpret_cast<const void *>(static_cast<std::intptr_t>(-1));

    void *const handle = GetStdHandle(std_output_handle);
    if (handle == nullptr || handle == invalid_handle) {
        return false;
    }

    unsigned long mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) {
        return false;
    }
    if ((mode & enable_virtual_terminal_processing) != 0) {
        return true;
    }
    return SetConsoleMode(handle, mode | enable_virtual_terminal_processing) != 0;
#else
    return true;
#endif
}

inline bool ansi_colors_enabled() noexcept {
    static const bool enabled = detect_ansi_support();
    return enabled;
}

inline const char *level_color(LogLevel lv) noexcept {
    return ansi_colors_enabled()
               ? level_ansi_colors[static_cast<std::uint8_t>(lv)]
               : "";
}

template <typename T>
class WithSourceLocation {
  public:
    template <typename U>
        requires std::constructible_from<T, U>
    consteval WithSourceLocation(
        U &&inner, std::source_location loc = std::source_location::current())
        : inner_(std::forward<U>(inner)), loc_(std::move(loc)) {
    }

    constexpr const T &format() const noexcept {
        return inner_;
    }

    constexpr const std::source_location &location() const noexcept {
        return loc_;
    }

  private:
    T inner_;
    std::source_location loc_;
};

inline auto g_log_file = []() -> std::unique_ptr<AsyncLogger> {
    const std::string path = env_utf8("LOG_PATH");
    if (!path.empty()) {
        return std::make_unique<AsyncLogger>(path, "");
    }
    return nullptr;
}();

inline std::atomic<LogLevel> g_log_threshold{[]() -> LogLevel {
    const std::string lv = env_utf8("LOG_LEVEL");
    if (!lv.empty()) {
        return log_level_from_string(lv);
    }
    return LogLevel::INFO;
}()};

// hook function, empty implement in default case
inline auto g_log_callback = std::function<void(LogLevel, std::string_view)>{};

// query zone only once
inline const std::chrono::time_zone *cached_zone() {
    static const std::chrono::time_zone *zone = std::chrono::current_zone();
    return zone;
}

inline void
output_log(LogLevel lv, std::string_view msg, const std::source_location &loc) {
    if (lv < g_log_threshold.load(std::memory_order_relaxed)) {
        return;
    }

    if (g_log_file == nullptr && g_log_callback) {
        g_log_callback(lv, msg);
        return;
    }

    // thread_local std::uint64_t tid =
    //	std::hash<std::thread::id>{}(std::this_thread::get_id());

    std::chrono::zoned_time now{cached_zone(),
                                std::chrono::floor<std::chrono::seconds>(
                                    std::chrono::system_clock::now())};
    // std::string fmsg = std::format("{}[{}]<{}> {}:{} {}()-> {}",
    //	now,
    //	tid,
    //	log_level_to_string(lv),
    //	loc.file_name(),
    //	loc.line(),
    //	loc.function_name(),
    //	msg);

    std::string fmsg =
        std::format("[{:%H:%M:%S}] [{}] {}", now, log_level_to_string(lv), msg);

    if (g_log_file) {
        fmsg += '\n';
        g_log_file->append_message(fmsg);
    }

    if (g_log_callback) {
        g_log_callback(lv, msg);
    } else {
        const char *color = level_color(lv);
        std::osyncstream sync_out(std::cout);
        if (color[0] != '\0') {
            sync_out << color << fmsg << "\033[0m" << std::endl;
        } else {
            sync_out << fmsg << std::endl;
        }
    }
}

template <typename... Args>
constexpr void log_fmt(LogLevel lv,
                       WithSourceLocation<std::format_string<Args...>> fmt,
                       Args &&...args) {
    if (lv < detail::g_log_threshold) {
        return;
    }

    auto msg = std::vformat(fmt.format().get(), std::make_format_args(args...));

    output_log(lv, msg, fmt.location());
}
} // namespace detail

namespace vkao {
inline void set_log_path(const std::string &dir,
                         const std::string &prefix,
                         size_t roll_size,
                         std::chrono::seconds flush_interval,
                         size_t check_per_count) {
    detail::g_log_file = std::make_unique<detail::AsyncLogger>(
        dir, prefix, roll_size, flush_interval, check_per_count);
}

inline void set_log_threshold(detail::LogLevel lv) {
    detail::g_log_threshold.store(lv, std::memory_order_relaxed);
}
} // namespace vkao

#define _FUNCTION(name)                                                        \
    template <typename... Args>                                                \
    constexpr void VKAO_##name(                                                \
        detail::WithSourceLocation<std::format_string<Args...>> fmt,           \
        Args &&...args) {                                                      \
        return detail::log_fmt(detail::LogLevel::name,                         \
                               std::move(fmt),                                 \
                               std::forward<Args>(args)...);                   \
    }
_FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION

#if defined(NDEBUG)
#define VKAO_ASSERT(x, ...) (x)
#else
#define VKAO_ASSERT(x, ...)                                                    \
    {                                                                          \
        if (!(x)) {                                                            \
            VKAO_ERROR("Assertion Failed: {}", __VA_ARGS__);                   \
            __debugbreak();                                                    \
        }                                                                      \
    }
#endif