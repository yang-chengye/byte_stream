#include "byte_stream/byte_stream.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <stdint.h>
#include <iomanip>
#include <iostream>
#include <list>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bench {

volatile uint64_t sink = 0;

struct packet {
    uint16_t sequence{};
    uint32_t device_id{};
    uint8_t type{};
    std::array<uint16_t, 8> values{};
    std::optional<uint16_t> quality;
    std::vector<uint8_t> payload;
};

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.sequence);
    stream.set(value.device_id, 3);
    stream.set(value.type);
    stream.set(value.values);
    stream.set(value.quality);
    stream.set(static_cast<uint16_t>(value.payload.size()));
    stream.set(value.payload);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    value.sequence = stream.get<uint16_t>();
    value.device_id = stream.get<uint32_t>(3);
    value.type = stream.get<uint8_t>();
    stream.get_to(value.values);
    stream.get_to(value.quality);

    const auto payload_size = stream.get<uint16_t>();
    stream.get_to(value.payload, payload_size);
}

struct nested_packet {
    uint32_t timestamp{};
    packet primary;
    packet backup;
    std::array<uint32_t, 4> counters{};
};

void to_byte_stream(byte_stream::stream& stream, const nested_packet& value) {
    stream.set(value.timestamp);
    stream.set(value.primary);
    stream.set(value.backup);
    stream.set(value.counters);
}

void from_byte_stream(const byte_stream::stream& stream, nested_packet& value) {
    value.timestamp = stream.get<uint32_t>();
    stream.get_to(value.primary);
    stream.get_to(value.backup);
    stream.get_to(value.counters);
}

template <typename Fn>
double measure_seconds(Fn&& fn) {
    const auto start = std::chrono::steady_clock::now();
    fn();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(stop - start).count();
}

void print_throughput(const std::string& name, std::size_t bytes, double seconds) {
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::cout << std::left << std::setw(36) << name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << seconds * 1000.0 << " ms  "
              << std::setw(10) << std::setprecision(2) << (mib / seconds)
              << " MiB/s\n";
}

void print_rate(const std::string& name, std::size_t count, double seconds) {
    const double ns_per_op = seconds * 1'000'000'000.0 / static_cast<double>(count);
    const double ops_per_sec = static_cast<double>(count) / seconds;
    std::cout << std::left << std::setw(36) << name
              << std::right << std::setw(10) << std::fixed << std::setprecision(2)
              << ns_per_op << " ns/op  "
              << std::setw(10) << std::setprecision(2) << (ops_per_sec / 1'000'000.0)
              << " Mops/s\n";
}

std::vector<uint8_t> make_payload(std::size_t size) {
    std::vector<uint8_t> payload(size);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 131u + 17u) & 0xFFu);
    }
    return payload;
}

std::vector<uint32_t> make_u32_values(std::size_t count) {
    std::vector<uint32_t> values(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<uint32_t>(0x10000000u + i * 17u);
    }
    return values;
}

std::vector<float> make_float_values(std::size_t count) {
    std::vector<float> values(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<float>(i % 1024u) * 0.25f - 128.0f;
    }
    return values;
}

std::vector<double> make_double_values(std::size_t count) {
    std::vector<double> values(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<double>(i % 4096u) * 0.125 - 256.0;
    }
    return values;
}

std::vector<packet> make_packets(std::size_t count) {
    std::vector<packet> packets;
    packets.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        packet value;
        value.sequence = static_cast<uint16_t>(i);
        value.device_id = 0x00110000u | static_cast<uint32_t>(i & 0xFFFFu);
        value.type = static_cast<uint8_t>(i & 0x7Fu);
        for (std::size_t j = 0; j < value.values.size(); ++j) {
            value.values[j] = static_cast<uint16_t>(i + j * 3u);
        }
        if ((i & 1u) == 0) {
            value.quality = static_cast<uint16_t>(i & 0xFFFFu);
        }
        value.payload = make_payload(16);
        packets.push_back(std::move(value));
    }

    return packets;
}

std::vector<nested_packet> make_nested_packets(std::size_t count) {
    std::vector<nested_packet> packets;
    packets.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        nested_packet value;
        value.timestamp = static_cast<uint32_t>(0x60000000u + i);
        value.primary.sequence = static_cast<uint16_t>(i);
        value.primary.device_id = 0x00110000u | static_cast<uint32_t>(i & 0xFFFFu);
        value.primary.type = static_cast<uint8_t>(i & 0x7Fu);
        value.primary.payload = make_payload(16);
        if ((i & 1u) == 0) {
            value.primary.quality = static_cast<uint16_t>(i & 0xFFFFu);
        }
        for (std::size_t j = 0; j < value.primary.values.size(); ++j) {
            value.primary.values[j] = static_cast<uint16_t>(i + j * 3u);
        }

        value.backup = value.primary;
        value.backup.sequence = static_cast<uint16_t>(value.primary.sequence + 1u);
        for (std::size_t j = 0; j < value.counters.size(); ++j) {
            value.counters[j] = static_cast<uint32_t>(i * 17u + j);
        }
        packets.push_back(std::move(value));
    }

    return packets;
}

void benchmark_payload_size(const std::string& label, std::size_t payload_size, std::size_t rounds) {
    const auto payload = make_payload(payload_size);

    byte_stream::stream stream;
    stream.reserve(payload.size());
    const double write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.clear();
            stream.set(payload);
            sink += stream.size();
        }
    });
    print_throughput(label + " payload write", payload_size * rounds, write_seconds);

    std::vector<uint8_t> decoded;
    decoded.reserve(payload_size);
    const double read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            stream.reset_position();
            stream.get_to(decoded, static_cast<uint32_t>(payload_size));
            sink += decoded.size();
        }
    });
    print_throughput(label + " payload read", payload_size * rounds, read_seconds);
}

void benchmark_payload_sizes() {
    benchmark_payload_size("small", 64, 200'000);
    benchmark_payload_size("medium", 4u * 1024u, 20'000);
}

void benchmark_read_only_buffer_access() {
    constexpr std::size_t payload_size = 64u * 1024u * 1024u;
    const byte_stream::stream stream(make_payload(payload_size));

    uint64_t range_checksum = 0;
    const double range_seconds = measure_seconds([&] {
        for (uint8_t byte : stream) {
            range_checksum += byte;
        }
    });
    sink += range_checksum;
    print_throughput("read-only range traversal", payload_size, range_seconds);

    uint64_t indexed_checksum = 0;
    const double indexed_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < stream.size(); ++i) {
            indexed_checksum += stream[i];
        }
    });
    sink += indexed_checksum;
    print_throughput("read-only indexed traversal", payload_size, indexed_seconds);

    uint64_t checked_checksum = 0;
    const double checked_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < stream.size(); ++i) {
            checked_checksum += stream.at(i);
        }
    });
    sink += checked_checksum;
    print_throughput("checked indexed traversal", payload_size, checked_seconds);
}

void benchmark_floating_vectors() {
    constexpr std::size_t float_count = 1'000'000;
    constexpr std::size_t double_count = 500'000;
    constexpr std::size_t rounds = 4;
    const auto floats = make_float_values(float_count);
    const auto doubles = make_double_values(double_count);

    byte_stream::stream float_stream;
    float_stream.set_endian(byte_stream::endian::big);
    float_stream.reserve(floats.size() * sizeof(float));
    const double float_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            float_stream.clear();
            float_stream.set(floats);
            sink += float_stream.size();
        }
    });
    print_throughput("big endian vector<float> write",
        float_count * sizeof(float) * rounds,
        float_write_seconds);

    std::vector<float> decoded_floats;
    decoded_floats.reserve(float_count);
    const double float_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            float_stream.reset_position();
            float_stream.get_to(decoded_floats, static_cast<uint32_t>(float_count));
            sink += static_cast<uint64_t>(decoded_floats.front() + 512.0f);
            sink += decoded_floats.size();
        }
    });
    print_throughput("big endian vector<float> read",
        float_count * sizeof(float) * rounds,
        float_read_seconds);

    byte_stream::stream double_stream;
    double_stream.set_endian(byte_stream::endian::little);
    double_stream.reserve(doubles.size() * sizeof(double));
    const double double_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            double_stream.clear();
            double_stream.set(doubles);
            sink += double_stream.size();
        }
    });
    print_throughput("little endian vector<double> write",
        double_count * sizeof(double) * rounds,
        double_write_seconds);

    std::vector<double> decoded_doubles;
    decoded_doubles.reserve(double_count);
    const double double_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            double_stream.reset_position();
            double_stream.get_to(decoded_doubles, static_cast<uint32_t>(double_count));
            sink += static_cast<uint64_t>(decoded_doubles.front() + 512.0);
            sink += decoded_doubles.size();
        }
    });
    print_throughput("little endian vector<double> read",
        double_count * sizeof(double) * rounds,
        double_read_seconds);
}

void benchmark_standard_containers() {
    constexpr std::size_t element_count = 4096;
    constexpr std::size_t rounds = 5000;
    const auto words = make_u32_values(element_count);
    const std::string text(element_count, 'x');
    std::list<uint16_t> list_values;
    for (std::size_t i = 0; i < element_count; ++i) {
        list_values.push_back(static_cast<uint16_t>(i));
    }

    byte_stream::stream vector_stream;
    vector_stream.reserve(words.size() * sizeof(uint32_t));
    const double vector_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            vector_stream.clear();
            vector_stream.set(words);
            sink += vector_stream.size();
        }
    });
    print_throughput("std::vector<uint32_t> write",
        element_count * sizeof(uint32_t) * rounds,
        vector_write_seconds);

    std::vector<uint32_t> decoded_words;
    decoded_words.reserve(element_count);
    const double vector_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            vector_stream.reset_position();
            vector_stream.get_to(decoded_words, static_cast<uint32_t>(element_count));
            sink += decoded_words.back();
        }
    });
    print_throughput("std::vector<uint32_t> read",
        element_count * sizeof(uint32_t) * rounds,
        vector_read_seconds);

    byte_stream::stream string_stream;
    string_stream.reserve(text.size());
    const double string_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            string_stream.clear();
            string_stream.set(text);
            sink += string_stream.size();
        }
    });
    print_throughput("std::string write", text.size() * rounds, string_write_seconds);

    std::string decoded_text;
    decoded_text.reserve(text.size());
    const double string_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            string_stream.reset_position();
            string_stream.get_to(decoded_text, static_cast<uint32_t>(text.size()));
            sink += decoded_text.size();
        }
    });
    print_throughput("std::string read", text.size() * rounds, string_read_seconds);

    byte_stream::stream list_stream;
    list_stream.reserve(list_values.size() * sizeof(uint16_t));
    const double list_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            list_stream.clear();
            list_stream.set(list_values);
            sink += list_stream.size();
        }
    });
    print_throughput("std::list<uint16_t> write",
        element_count * sizeof(uint16_t) * rounds,
        list_write_seconds);

    std::list<uint16_t> decoded_list;
    const double list_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            list_stream.reset_position();
            list_stream.get_to(decoded_list, static_cast<uint32_t>(element_count));
            sink += decoded_list.back();
        }
    });
    print_throughput("std::list<uint16_t> read",
        element_count * sizeof(uint16_t) * rounds,
        list_read_seconds);
}

void benchmark_big_endian_integral_records() {
    constexpr std::size_t record_count = 500'000;
    constexpr std::size_t bytes_per_record = 1 + 2 + 3 + 8;

    byte_stream::stream stream;
    stream.set_endian(byte_stream::endian::big);
    stream.reserve(record_count * bytes_per_record);
    const double write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < record_count; ++i) {
            stream.set(static_cast<uint8_t>(i));
            stream.set(static_cast<uint16_t>(i));
            stream.set(static_cast<uint32_t>(i), 3);
            stream.set(static_cast<uint64_t>(0x1020304050607080ull + i));
        }
    });
    print_rate("big endian integer write", record_count, write_seconds);

    stream.reset_position();
    const double read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < record_count; ++i) {
            sink += stream.get<uint8_t>();
            sink += stream.get<uint16_t>();
            sink += stream.get<uint32_t>(3);
            sink += stream.get<uint64_t>();
        }
    });
    print_rate("big endian integer read", record_count, read_seconds);
}

void benchmark_nested_protocol() {
    constexpr std::size_t packet_count = 100'000;
    const auto packets = make_nested_packets(packet_count);

    byte_stream::stream stream;
    stream.reserve(packet_count * 128);
    const double write_seconds = measure_seconds([&] {
        for (const auto& packet : packets) {
            stream.set(packet);
        }
    });
    print_rate("nested packet write", packet_count, write_seconds);
    print_throughput("nested packet write bytes", stream.size(), write_seconds);

    nested_packet decoded;
    stream.reset_position();
    const double read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < packet_count; ++i) {
            stream.get_to(decoded);
            sink += decoded.timestamp;
            sink += decoded.primary.payload.size();
            sink += decoded.backup.payload.size();
        }
    });
    print_rate("nested packet read", packet_count, read_seconds);
    print_throughput("nested packet read bytes", stream.size(), read_seconds);
}

void benchmark_repeated_parse() {
    const auto sample_packet = make_packets(1).front();
    byte_stream::stream stream;
    stream.set(sample_packet);
    const auto encoded = stream.buffer();
    constexpr std::size_t iterations = 200'000;

    const double seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < iterations; ++i) {
            const auto decoded = byte_stream::stream::parse<packet>(encoded);
            sink += decoded.sequence;
            sink += decoded.payload.size();
        }
    });
    print_rate("repeated parse<packet>", iterations, seconds);
}

void benchmark_empty_and_bounded_containers() {
    constexpr std::size_t rounds = 500'000;
    const std::vector<uint16_t> empty_words;
    const auto bounded_payload = make_payload(255);

    byte_stream::stream empty_stream;
    const double empty_write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            empty_stream.clear();
            empty_stream.set_with_size(empty_words);
            sink += empty_stream.size();
        }
    });
    print_rate("empty container write", rounds, empty_write_seconds);

    std::vector<uint16_t> decoded_words;
    const double empty_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            empty_stream.reset_position();
            empty_stream.get_to_with_size(decoded_words, 0);
            sink += decoded_words.size();
        }
    });
    print_rate("empty container read", rounds, empty_read_seconds);

    byte_stream::stream bounded_stream;
    bounded_stream.set_with_size(bounded_payload);
    std::vector<uint8_t> decoded_payload;
    decoded_payload.reserve(bounded_payload.size());
    const double bounded_read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < rounds; ++i) {
            bounded_stream.reset_position();
            bounded_stream.get_to_with_size(decoded_payload, bounded_payload.size());
            sink += decoded_payload.size();
        }
    });
    print_throughput("max-count payload read",
        bounded_payload.size() * rounds,
        bounded_read_seconds);
}

void benchmark_malformed_inputs() {
    constexpr std::size_t iterations = 10'000;
    std::vector<uint8_t> malformed;
    malformed.reserve(1 + 2 * sizeof(uint16_t));
    malformed.push_back(0x02);
    malformed.push_back(0x34);
    byte_stream::stream stream(std::move(malformed));
    stream.set_endian(byte_stream::endian::little);
    std::vector<uint16_t> decoded;

    const double seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < iterations; ++i) {
            stream.reset_position();
            try {
                stream.get_to_with_size(decoded, 2);
            }
            catch (const byte_stream::byte_stream_error&) {
                sink += stream.position();
            }
        }
    });
    print_rate("truncated container reject", iterations, seconds);
}

void benchmark_atomic_packet_reads() {
    constexpr std::size_t packet_count = 100'000;
    const auto packets = make_packets(packet_count);

    byte_stream::stream stream;
    stream.reserve(packet_count * 64);
    for (const auto& packet : packets) {
        stream.set(packet);
    }

    packet decoded;
    const double seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < packet_count; ++i) {
            stream.get_to_atomic(decoded);
            sink += decoded.sequence;
            sink += decoded.payload.size();
        }
    });
    print_rate("atomic packet read", packet_count, seconds);
}

void benchmark_variant_records() {
    using text_type = byte_stream::length_prefixed<std::string, 16>;
    using value_type = std::variant<std::monostate, uint32_t, text_type>;
    constexpr std::size_t record_count = 300'000;

    byte_stream::stream stream;
    stream.reserve(record_count * 4);
    const double write_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < record_count; ++i) {
            switch (i % 3) {
            case 0:
                stream.set(value_type{ std::monostate{} });
                break;
            case 1:
                stream.set(value_type{ static_cast<uint32_t>(i) });
                break;
            default:
                stream.set(value_type{ text_type{ std::string{ "wire" } } });
                break;
            }
        }
    });
    print_rate("variant record write", record_count, write_seconds);

    value_type decoded;
    const double read_seconds = measure_seconds([&] {
        for (std::size_t i = 0; i < record_count; ++i) {
            stream.get_to(decoded);
            sink += decoded.index();
        }
    });
    print_rate("variant record read", record_count, read_seconds);
}

void benchmark_object_assignment();
void benchmark_macro_fields();
void benchmark_buffer_resize();

} // namespace bench

int main() {
#ifdef BYTE_STREAM_BENCHMARK_DEBUG_BUILD
    std::cout << "warning: benchmark is running in a debug build\n";
#endif

    std::cout << "byte_stream coverage benchmark\n";
    bench::benchmark_payload_sizes();
    bench::benchmark_read_only_buffer_access();
    bench::benchmark_floating_vectors();
    bench::benchmark_standard_containers();
    bench::benchmark_big_endian_integral_records();
    bench::benchmark_nested_protocol();
    bench::benchmark_repeated_parse();
    bench::benchmark_empty_and_bounded_containers();
    bench::benchmark_malformed_inputs();
    bench::benchmark_atomic_packet_reads();
    bench::benchmark_variant_records();
    bench::benchmark_object_assignment();
    bench::benchmark_macro_fields();
    bench::benchmark_buffer_resize();
    std::cout << "sink=" << bench::sink << '\n';
}
