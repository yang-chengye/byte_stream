#ifndef BYTE_STREAM_BENCHMARK_SUPPORT_HPP
#define BYTE_STREAM_BENCHMARK_SUPPORT_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace bench {

constexpr std::size_t measurement_count = 7;

extern volatile uint64_t sink;

struct measurements {
    double median_seconds{};
    double min_seconds{};
    double max_seconds{};
};

inline bool& csv_output() noexcept {
    static bool enabled = false;
    return enabled;
}

inline void set_csv_output(bool enabled) noexcept {
    csv_output() = enabled;
}

inline void consume(uint64_t value) noexcept {
    sink += value;
}

template <typename Fn>
measurements measure(Fn&& fn) {
    consume(fn());

    std::array<double, measurement_count> samples{};
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto start = std::chrono::steady_clock::now();
        const auto checksum = fn();
        const auto stop = std::chrono::steady_clock::now();
        consume(checksum);
        samples[i] = std::chrono::duration<double>(stop - start).count();
    }

    std::sort(samples.begin(), samples.end());
    return { samples[samples.size() / 2], samples.front(), samples.back() };
}

inline void print_header(const char* title) {
    if (csv_output()) {
        std::cout
            << "benchmark,median_ns,min_ns,max_ns,operations,bytes,ns_per_op,mib_per_second\n";
    }
    else {
        std::cout << title << " (1 warmup, " << measurement_count << " samples, median reported)\n";
    }
}

inline void print_csv(const std::string& name,
    const measurements& result,
    std::size_t operations,
    std::size_t bytes) {
    const double median_ns = result.median_seconds * 1'000'000'000.0;
    const double ns_per_op = operations == 0
        ? 0.0
        : median_ns / static_cast<double>(operations);
    const double mib_per_second = bytes == 0
        ? 0.0
        : (static_cast<double>(bytes) / (1024.0 * 1024.0)) / result.median_seconds;
    std::cout << name << ','
              << std::fixed << std::setprecision(3)
              << median_ns << ','
              << result.min_seconds * 1'000'000'000.0 << ','
              << result.max_seconds * 1'000'000'000.0 << ','
              << operations << ','
              << bytes << ','
              << ns_per_op << ','
              << mib_per_second << '\n';
}

inline void print_throughput(const std::string& name,
    std::size_t bytes,
    const measurements& result) {
    if (csv_output()) {
        print_csv(name, result, 0, bytes);
        return;
    }

    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::cout << std::left << std::setw(34) << name
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(10) << result.median_seconds * 1000.0 << " ms  "
              << std::setw(10) << mib / result.median_seconds << " MiB/s  ["
              << result.min_seconds * 1000.0 << ", "
              << result.max_seconds * 1000.0 << "] ms\n";
}

inline void print_rate(const std::string& name,
    std::size_t count,
    const measurements& result) {
    if (csv_output()) {
        print_csv(name, result, count, 0);
        return;
    }

    const double ns_per_op = result.median_seconds * 1'000'000'000.0 /
        static_cast<double>(count);
    const double ops_per_second = static_cast<double>(count) / result.median_seconds;
    std::cout << std::left << std::setw(34) << name
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(10) << ns_per_op << " ns/op  "
              << std::setw(10) << ops_per_second / 1'000'000.0 << " Mops/s  ["
              << result.min_seconds * 1000.0 << ", "
              << result.max_seconds * 1000.0 << "] ms\n";
}

} // namespace bench

#endif
