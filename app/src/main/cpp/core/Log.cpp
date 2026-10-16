#include "Log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace osm {

namespace {
std::array<LogEntry, Log::kCapacity> g_ring;
uint64_t g_written = 0;
const auto g_start = std::chrono::steady_clock::now();
}  // namespace

std::mutex& Log::mutex() {
    static std::mutex m;
    return m;
}

double Log::now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
}

void Log::write(LogLevel level, const char* tag, const char* fmt, ...) {
    char text[sizeof(LogEntry::text)];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

#if defined(__ANDROID__)
    static constexpr int kPrio[] = {ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR};
    char fullTag[32];
    std::snprintf(fullTag, sizeof(fullTag), "OSM/%s", tag);
    __android_log_write(kPrio[static_cast<int>(level)], fullTag, text);
#else
    static constexpr const char* kName[] = {"D", "I", "W", "E"};
    std::fprintf(stderr, "[%s/%s] %s\n", kName[static_cast<int>(level)], tag, text);
#endif

    std::lock_guard<std::mutex> lock(mutex());
    LogEntry& e = g_ring[g_written % kCapacity];
    e.time = now();
    e.level = level;
    std::snprintf(e.tag, sizeof(e.tag), "%s", tag);
    std::memcpy(e.text, text, sizeof(e.text));
    ++g_written;
}

size_t Log::snapshot(LogEntry* out, size_t max) {
    std::lock_guard<std::mutex> lock(mutex());
    const size_t available = g_written < kCapacity ? static_cast<size_t>(g_written) : kCapacity;
    const size_t count = available < max ? available : max;
    const uint64_t first = g_written - count;
    for (size_t i = 0; i < count; ++i) out[i] = g_ring[(first + i) % kCapacity];
    return count;
}

uint64_t Log::totalWritten() {
    std::lock_guard<std::mutex> lock(mutex());
    return g_written;
}

void Log::clear() {
    std::lock_guard<std::mutex> lock(mutex());
    g_written = 0;
}

}  // namespace osm
