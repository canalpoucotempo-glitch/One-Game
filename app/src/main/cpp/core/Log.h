// Native logging: forwards to logcat (Android) or stderr (host) and keeps a
// fixed-size, allocation-free ring buffer that the Diagnostics page reads.
#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace osm {

enum class LogLevel : uint8_t { Debug = 0, Info, Warn, Error };

struct LogEntry {
    double   time = 0.0;          // seconds since process start
    LogLevel level = LogLevel::Info;
    char     tag[16] = {};
    char     text[160] = {};
};

class Log {
public:
    static constexpr size_t kCapacity = 256;

    static void write(LogLevel level, const char* tag, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 3, 4)))
#endif
        ;

    // Copies up to `max` most-recent entries (oldest first) into `out`.
    // Returns the number copied. Thread-safe.
    static size_t snapshot(LogEntry* out, size_t max);
    static uint64_t totalWritten();
    static void clear();
    static double now();

private:
    static std::mutex& mutex();
};

}  // namespace osm

#define OSM_LOGD(tag, ...) ::osm::Log::write(::osm::LogLevel::Debug, tag, __VA_ARGS__)
#define OSM_LOGI(tag, ...) ::osm::Log::write(::osm::LogLevel::Info, tag, __VA_ARGS__)
#define OSM_LOGW(tag, ...) ::osm::Log::write(::osm::LogLevel::Warn, tag, __VA_ARGS__)
#define OSM_LOGE(tag, ...) ::osm::Log::write(::osm::LogLevel::Error, tag, __VA_ARGS__)
