# byte_stream

[简体中文](README.md) | **English**

`byte_stream` is a header-only C++17 library for encoding and decoding binary protocols. It reads and writes integers, enums, floating-point values, containers, and custom structures in field order. It supports both byte orders and nonstandard integer widths such as 3, 5, 6, and 7 bytes.

## Features

- Header-only: include `include/byte_stream/byte_stream.hpp`
- C++17 with no third-party runtime dependencies
- Zero-copy `byte_reader` and allocation-free, fixed-capacity `byte_writer`, with support for `-fno-exceptions`
- Little-endian and big-endian support
- Support for integers, enums, `std::byte`, `float`, `double`, and `bool`
- Nonstandard integer widths, such as a 24-bit device identifier stored in 3 bytes
- Bulk reads and writes for sequential containers, `std::array`, and C arrays
- Support for `std::optional`, smart pointers, `std::pair`, `std::tuple`, and `std::variant`
- Custom protocol structures through ADL, macros, or `byte_stream_codec<T>`
- Compact varint/zigzag integer encoding and containers with compact length prefixes
- Separate byte escaping and unescaping utilities with configurable rules
- CMake interface target, installation exports, examples, and GTest tests
- Continuous integration on Linux, macOS, and Windows

## Stream state and object assignment

```cpp
byte_stream::stream bs(byte_stream::endian::big); // empty, position 0
bs = uint16_t{0x1234};                         // replace: 12 34
bs.set(uint16_t{0x5678});                      // append: 12 34 56 78
const auto first = bs.get<uint16_t>();         // 0x1234
uint16_t second{};
bs.get_to(second);                            // 0x5678
bs.set_endian(byte_stream::endian::little);    // affects subsequent reads/writes
```

Default construction uses little endian. Object assignment preserves the current endian,
replaces all bytes, resets the read position, and reuses capacity. It uses the same codec
and default width as `set(obj)`. On failure, old data is discarded and a partial encoded
prefix may remain. The input must not refer to the destination's buffer or its elements;
custom serializers must only append bytes and must not change endian or read position.

Copying or moving a stream transfers its complete state, including bytes, read position,
and endian. It does not serialize an object. `set_endian()` changes neither existing bytes
nor read position; reading mixed-endian fields requires explicit switching.
See [DESIGN.md](DESIGN.md) for the full contract.

## Quick start

```cpp
#include "byte_stream/byte_stream.hpp"

#include <stdint.h>

int main() {
    byte_stream::stream stream;
    // The default wire byte order is little-endian; big-endian can be set explicitly.

    stream.set<uint8_t>(0x01);
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3); // Write only 3 bytes.

    const auto id = stream.get<uint8_t>();
    const auto seq = stream.get<uint16_t>();
    const auto area = stream.get<uint32_t>(3);

    (void)id;
    (void)seq;
    (void)area;
}
```

The default wire byte order is `byte_stream::endian::little`, so the default encoding is independent of the host byte order.
The public API offers `little` and `big`. Set big-endian explicitly when required by the protocol:

```cpp
byte_stream::stream stream;
stream.set_endian(byte_stream::endian::big);
```

See [Wire Format](WIRE_FORMAT.md) (Chinese) for scalar, container, and compact encoding rules and compatibility guarantees.
See [Design and API contracts](DESIGN.md) (Chinese) for buffer ownership, allocation behavior, atomic APIs, custom codecs, and threading constraints.

Inspect the encoded bytes:

```cpp
const auto& bytes = stream.buffer();
const auto hex = stream.to_hex(true); // "01 34 12 c3 b2 a1"

// Read-only indexing and iteration over the entire buffer leave the read position unchanged.
const auto first = stream[0];
for (uint8_t byte : stream) {
    (void)byte;
}
```

For embedded systems, received network packets, and real-time hot paths, read and write directly in caller-provided storage without copying the input, growing the buffer, or throwing exceptions:

```cpp
#include "byte_stream/byte_io.hpp"

#include <array>

std::array<uint8_t, 32> storage{};
byte_stream::byte_writer writer(storage.data(), storage.size());
if (writer.write<uint16_t>(0x1234) != byte_stream::byte_stream_errc::ok ||
    writer.write<uint32_t>(0x00A1B2C3, 3) != byte_stream::byte_stream_errc::ok) {
    // Insufficient fixed-buffer space or an invalid argument.
}

byte_stream::byte_reader reader(writer.view());
uint16_t sequence = 0;
uint32_t device_id = 0;
if (reader.read(sequence) != byte_stream::byte_stream_errc::ok ||
    reader.read(device_id, 3) != byte_stream::byte_stream_errc::ok) {
    // Truncated input, an invalid value, or an invalid argument.
}
```

`byte_view`, `byte_reader`, and `byte_writer` do not own memory. The caller must keep the underlying storage valid while using these objects or any views they return. Failed basic reads and writes leave the cursor unchanged and do not partially modify the output.

## Using CMake

Add the library as a subdirectory:

```cmake
add_subdirectory(byte_stream)
target_link_libraries(your_target PRIVATE byte_stream::byte_stream)
```

## Building, testing, and examples

```bash
cmake -S . -B build/dev -DBYTE_STREAM_BUILD_TESTS=ON -DBYTE_STREAM_BUILD_EXAMPLES=ON
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

Run the examples:

```bash
./build/dev/examples/byte_stream_basic
./build/dev/examples/byte_stream_custom_protocol
./build/dev/examples/byte_stream_macro_generated
./build/dev/examples/byte_stream_fixed_buffer
```

Tests use the pinned GTest sources in `3rdparty/googletest`. No system installation or network access during configuration is needed. GTest is used only for project tests and is not a runtime dependency of `byte_stream`.

## Cross-platform support

`byte_stream` uses standard C++17 and a CMake interface target, with no dependency on platform APIs. CI builds and tests on the following platforms:

| Platform | Compiler/toolchain |
| --- | --- |
| Linux | Default GCC, a GCC 9 compatibility job, Clang ASan/UBSan |
| macOS | AppleClang |
| Windows | MSVC / Visual Studio generator |
| Embedded Linux | ARMv7 hard-float and AArch64 cross-compilation, `-fno-exceptions -fno-rtti` |

Manual CMake commands require CMake 3.16 or later. The repository's CMake Presets use schema version 3 and require CMake 3.21 or later.

CMake Presets are recommended for local development:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset strict
cmake --build --preset strict
ctest --preset strict

cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers

cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

For Visual Studio and other multi-configuration generators on Windows, the presets already select `Debug` or `Release`. When running commands manually, select the build/test configuration explicitly (`cmake --build ... --config Release` and `ctest ... -C Release`, or the corresponding `Debug` commands).

The `strict` preset enables `-Wall -Wextra -Wpedantic -Werror` for project targets (`/W4 /WX` with MSVC).
The `sanitizers` preset enables ASan and UBSan with GCC/Clang. The main CMake options are:

| Option | Default | Purpose |
| --- | --- | --- |
| `BYTE_STREAM_BUILD_TESTS` | ON for a top-level build | Build unit, property, and header tests |
| `BYTE_STREAM_BUILD_EXAMPLES` | ON for a top-level build | Build examples |
| `BYTE_STREAM_BUILD_BENCHMARKS` | ON for a top-level build | Build Release benchmarks |
| `BYTE_STREAM_ENABLE_WARNINGS` | ON for a top-level build | Enable strict warnings for project development targets |
| `BYTE_STREAM_WARNINGS_AS_ERRORS` | OFF | Treat project warnings as errors without affecting consumer targets |
| `BYTE_STREAM_ENABLE_SANITIZERS` | OFF | Enable ASan/UBSan |
| `BYTE_STREAM_ENABLE_COVERAGE` | OFF | Enable GCC/Clang source coverage instrumentation |

## Benchmarks

The project provides two benchmark executables with no third-party dependencies:

- `byte_stream_performance`: the core baseline benchmark, with stable metric names for comparing large payloads, integer records, compact encoding, and custom protocol objects before and after changes.
- `byte_stream_coverage_benchmark`: additional coverage for small/medium payloads, floating-point values, common standard containers, big-endian paths, nested structures, repeated parsing, empty containers, bounded reads, and rejection of truncated input.

```bash
cmake --preset release
cmake --build --preset release
./build/release/benchmarks/byte_stream_performance
./build/release/benchmarks/byte_stream_coverage_benchmark
```

`byte_stream_performance` performs a warm-up, collects seven samples, and reports the median and range. Allocation of reusable buffers is excluded from core encoding/decoding time. Use CSV output and the comparison script to compare matching metrics before and after hot-path changes:

```bash
./build/release/benchmarks/byte_stream_performance --csv > baseline.csv
# Make changes and rebuild.
./build/release/benchmarks/byte_stream_performance --csv > current.csv
python3 benchmarks/compare.py baseline.csv current.csv --max-regression-percent 5
```

Collect baseline and current results on the same idle machine with the same compiler and build options. Shared CI runners are suitable for benchmark smoke tests, but not for strict performance gates. See [benchmarks/README.md](benchmarks/README.md) (Chinese) for the full methodology.

With Windows/MSVC multi-configuration generators, the benchmark executables are typically located at:

```powershell
.\build\release\benchmarks\Release\byte_stream_performance.exe
.\build\release\benchmarks\Release\byte_stream_coverage_benchmark.exe
```

The current implementation includes optimizations for common hot paths:

- Contiguous one-byte data, such as `std::vector<uint8_t>`, `std::string`, `std::array<uint8_t, N>`, `uint8_t[N]`, and sequences of `std::byte`, use bulk `memcpy`.
- Contiguous numeric containers, such as `std::vector<uint16_t>`, `std::vector<uint32_t>`, and `std::array<float, N>`, use bulk encoding/decoding with a single output-buffer resize per write.
- Byte-order conversion for 16/32/64-bit integers and contiguous scalar containers prefers compiler byteswap builtins.
- Integer writes resize the buffer once before filling in bytes, reducing the capacity checks associated with per-byte `push_back`.
- Reads of contiguous byte containers check the length once and copy in bulk.
- Use `set_compact` / `get_compact` to reduce the space used by small integers or length fields.
- For complex protocol structures, estimate the required capacity and call `reserve()` in advance to avoid repeated growth.

`byte_stream::stream` is an in-memory buffering tool suited to protocol frames, file blocks, and network packets already in memory. For very large files, read chunks along protocol boundaries and process each chunk with `byte_stream::stream`, rather than keeping the entire file and decoded output in memory at once.

## Byte escaping utilities

`byte_stream_utils.hpp` provides separate escaping and unescaping utilities outside the core `byte_stream.hpp`. Supply a rule table to adapt them to a protocol. For example, the following rules encode `0xC0` as `0xDB 0xDC` and `0xDB` as `0xDB 0xDD`:

```cpp
#include "byte_stream/byte_stream_utils.hpp"

std::vector<uint8_t> raw{0x01, 0xC0, 0xDB, 0x02};
const std::array<byte_stream::byte_stream_utils::byte_escape_rule, 2> rules{{
    {0xC0, {0xDB, 0xDC}},
    {0xDB, {0xDB, 0xDD}},
}};

auto escaped = byte_stream::byte_stream_utils::escape_bytes(raw, rules);
auto decoded = byte_stream::byte_stream_utils::unescape_bytes(escaped, rules);
```

A rule's `escaped_value` is the complete replacement sequence and may contain one or more bytes. Unescaping throws `byte_stream::byte_stream_error` on an incomplete or unknown escape sequence.

If the frame header and trailer are not escaped, process only the payload before passing the result to `byte_stream::stream`:

```cpp
std::vector<uint8_t> wire_frame{/* Header, escaped payload, trailer. */};

auto logical_frame = byte_stream::byte_stream_utils::unescape_frame_payload(
    wire_frame,
    1, // Header length in bytes.
    1, // Trailer length in bytes.
    rules);

byte_stream::stream stream(logical_frame);

// Or construct byte_stream::stream in one step.
auto stream2 = byte_stream::byte_stream_utils::make_unescaped_frame_stream(wire_frame, 1, 1, rules);
```

## Containers

Container writes include all elements by default. Pass an element count to write only the first N elements. Container reads require an element count because the byte stream does not identify container boundaries.

```cpp
std::vector<uint16_t> values{0x1111, 0x2222, 0x3333};

byte_stream::stream stream;
stream.set(values, 2);

std::vector<uint16_t> decoded;
stream.get_to(decoded, 2);
```

Use a compact length prefix when a protocol field needs to carry its own length. Reads require a maximum element count to prevent malicious input from triggering unbounded allocation:

```cpp
std::vector<uint16_t> values{0x1111, 0x2222, 0x3333};

byte_stream::stream stream;
stream.set_with_size(values); // Compact length prefix followed by the payload.

std::vector<uint16_t> decoded;
stream.get_to_with_size(decoded, 1024);
```

Fixed-size arrays can be read and written directly. `std::array` and C arrays use their full length by default; pass an element count to read or write only the first N elements.

```cpp
std::array<uint16_t, 3> lanes{0x1111, 0x2222, 0x3333};
uint8_t raw[2]{0xAA, 0xBB};

byte_stream::stream stream;
stream.set(lanes);
stream.set(raw);

std::array<uint16_t, 3> decoded_lanes{};
uint8_t decoded_raw[2]{};
stream.get_to(decoded_lanes);
stream.get_to(decoded_raw);
```

## Supported types

| Type | Encoding |
| --- | --- |
| `int8_t` / `uint8_t` / `int16_t` / `uint16_t` / `int32_t` / `uint32_t` / `int64_t` / `uint64_t` | Written in the current byte order; `n` selects a byte count from 1 to the type's width |
| `bool` | One byte; only `00` / `01` are accepted, other values fail decoding |
| enum / `std::byte` | Written as the underlying integer type |
| `float` / `double` | Written in IEEE byte representation using the current byte order |
| Sequential containers such as `std::vector`, `std::list`, and `std::string` | Elements in sequence; reads require an element count |
| `std::array<T, N>` / `T[N]` | Fixed-size arrays; read/write N elements by default |
| `std::optional<T>` | A one-byte presence flag, followed by `T` when present |
| `std::unique_ptr<T>` / `std::shared_ptr<T>` | Read/write the pointed-to `T`; writing a null pointer throws, with no automatic null marker |
| `std::pair<A, B>` / `std::tuple<Ts...>` | Elements in order |
| `std::variant<Ts...>` | A compact `uint32_t` alternative index followed by the active value using its ordinary encoding; dynamic containers must be explicitly wrapped in `length_prefixed<Container, MaxCount>`; alternatives must be default-constructible for deserialization |
| `length_prefixed<Container, MaxCount>` | A compact element count followed by container contents; both encoding and decoding enforce `MaxCount`, providing explicit boundaries within composite values such as variants and tuples |
| Custom structures | Extend through `to_byte_stream` / `from_byte_stream` in the type's namespace, optionally generated by macros; specialize `byte_stream::byte_stream_codec<T>` for advanced use cases |

## Compact integer encoding

`set_compact` / `get_compact` explicitly encode small integers compactly. Unsigned integers use a 7-bit varint; signed integers first use zigzag encoding, so small negative values such as `-1` and `-64` also fit in one byte. The decoder accepts only the shortest canonical encoding and rejects overlong representations. This does not change the fixed-width semantics of ordinary `set` / `get`. It is suitable for lengths, counts, enum values, and small status codes.

```cpp
byte_stream::stream stream;
stream.set_compact<uint32_t>(300); // ac 02
stream.set_compact<int32_t>(-1);   // 01

auto a = stream.get_compact<uint32_t>();
auto b = stream.get_compact<int32_t>();
```

## Custom protocol structures

Provide `to_byte_stream` and `from_byte_stream` in the same namespace as your type. `byte_stream::stream` calls them through ADL. The second parameter of `to_byte_stream` should be `const T&`; the first parameter of `from_byte_stream` should be `const byte_stream::stream&`.

```cpp
namespace protocol {

struct packet {
    uint8_t id;
    uint32_t device_id; // 24-bit device identifier, 3 bytes on the wire.
    std::vector<uint16_t> values;
};

void to_byte_stream(byte_stream::stream& stream, const packet& value) {
    stream.set(value.id);
    stream.set(value.device_id, 3);
    stream.set(static_cast<uint8_t>(value.values.size()));
    stream.set(value.values);
}

void from_byte_stream(const byte_stream::stream& stream, packet& value) {
    value.id = stream.get<uint8_t>();
    value.device_id = stream.get<uint32_t>(3);

    const auto count = stream.get<uint8_t>();
    stream.get_to(value.values, count);
}

} // namespace protocol
```

For structures whose fields are read and written sequentially using the standard field operations, macros can generate these two functions. Place the non-intrusive macro in the type's namespace to access public members:

```cpp
namespace protocol {

struct header {
    uint8_t version;
    uint16_t sequence;
    uint32_t device_id; // 24-bit device identifier, 3 bytes on the wire.
    std::array<uint8_t, 4> magic;
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(header,
    version,
    sequence,
    (device_id, 3),
    magic)

} // namespace protocol
```

Place the intrusive macro inside the type. It generates friend functions and can access private members:

```cpp
namespace protocol {

class header {
public:
    header() = default;
    header(uint8_t version, uint32_t device_id)
        : version_(version), device_id_(device_id) {}

private:
    uint8_t version_{};
    uint32_t device_id_{};

    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(header,
        version_,
        (device_id_, 3))
};

} // namespace protocol
```

Macro fields can be written as `member` or `(member, n)`. A bare field uses the default width; `n` is passed as the second argument to `set` / `get_to`, allowing fields such as 3-byte integers or the first N array elements. Macros encode in field-list order without adding field names or length metadata to the stream. For more complex protocol details, such as preceding length fields, checksums, or reads of variable-length containers, write `to_byte_stream` / `from_byte_stream` manually.

These field macros require standard preprocessor expansion rules. The `byte_stream::byte_stream` CMake target automatically enables `/Zc:preprocessor` with MSVC. Add this option yourself when invoking MSVC directly.

If a type needs conversion in only one direction, use a macro that generates just one function:

```cpp
BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(header,
    version,
    (device_id, 3))

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_DESERIALIZE(header,
    version,
    (device_id, 3))

BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(header,
    version_,
    (device_id_, 3))

BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_DESERIALIZE(header,
    version_,
    (device_id_, 3))
```

You can also specialize `byte_stream::byte_stream_codec<T>` to define support entirely outside the class. This suits library adapters, third-party types, or cases where you do not want to expose ADL functions in the type's namespace:

```cpp
namespace protocol {

struct frame_id {
    uint8_t channel;
    uint32_t value; // 3 bytes on the wire.
};

} // namespace protocol

namespace byte_stream {

template <>
struct byte_stream_codec<protocol::frame_id> {
    static void write(stream& output, const protocol::frame_id& value, uint32_t) {
        output.set(value.channel);
        output.set(value.value, 3);
    }

    static void read(const stream& input, protocol::frame_id& value, uint32_t) {
        input.get_to(value.channel);
        input.get_to(value.value, 3);
    }
};

} // namespace byte_stream
```

Read and write custom types directly:

```cpp
protocol::packet p{/* ... */};

byte_stream::stream stream;
stream.set(p);

auto decoded = byte_stream::stream::parse<protocol::packet>(stream.buffer());
```

## Common APIs

| API | Description |
| --- | --- |
| `set(value, n)` | Write a value; `n` is a byte count for integers/enums or an element count for containers |
| `get<T>(n)` | Read and return a value |
| `get_atomic<T>(n)` | Read and return a value; restore the read position on failure |
| `get_to(value, n)` | Read into an existing object in place with minimal overhead |
| `get_to_atomic(value, n)` | Atomic read; leave both the read position and destination unchanged on failure |
| `parse<T>(bytes, endian)` | Parse exactly one object; supports vector/string/raw bytes and an explicit byte order |
| `set_compact(value)` / `get_compact<T>()` | Read/write compact integers or enums using varint/zigzag |
| `set_atomic(value, n)` | Atomic write; restore the original buffer length on failure |
| `set_with_size(value)` / `get_to_with_size(value, max_count)` | Read/write containers with compact length prefixes |
| `get_to_with_size_atomic(value, max_count)` | Atomically read a container with a compact length prefix |
| `buffer()` | Access the underlying `std::vector<uint8_t>` |
| `operator[]` / `at()` | Read-only byte access across the entire buffer; `at()` checks bounds |
| `begin()` / `end()` | Read-only iteration over the entire buffer, independent of the read position |
| `take_buffer()` | Move out the underlying buffer without copying; empty the stream and reset its position to zero |
| `operator+` / `operator+=` | Concatenate the complete buffers of two streams; preserve the left operand's position and byte order |
| `to_hex(with_spaces, uppercase)` | Format the complete buffer as hex; both options default to false |
| `stream::from_hex(text)` | Decode mixed-case hex, ignoring ASCII whitespace, into a byte vector |
| `seek(pos)` / `reset_position()` | Adjust the read position |
| `remaining()` / `eof()` | Inspect the remaining readable bytes |
| `set_endian(endian)` | Set the wire byte order to `little` or `big`; defaults to `little` |

The allocation-free basic APIs are in `byte_stream/byte_io.hpp`: `byte_writer::write` / `write_compact` / `write_bytes` write into a fixed caller-provided buffer; `byte_reader::read` / `read_compact` / `read_bytes` read from a caller-provided buffer. `read_view` returns a subview of the input directly. These operations return `byte_stream_errc`: `ok` indicates success and `insufficient_space` indicates insufficient remaining writer capacity.

## Hexadecimal conversion

```cpp
byte_stream::stream s(std::vector<uint8_t>{0x01, 0xab, 0xcd});
s.to_hex();             // "01abcd"
s.to_hex(true);         // "01 ab cd"
s.to_hex(false, true);  // "01ABCD"
s.to_hex(true, true);   // "01 AB CD"
auto text = byte_stream::stream::to_hex(s.buffer(), true, true);
auto bytes = byte_stream::stream::from_hex("01 aB CD");
byte_stream::stream restored(std::move(bytes));
```

`to_hex` returns an owning string; its static overload accepts a `const std::vector<uint8_t>&`.
It formats the complete buffer without changing the read position or byte order.
`from_hex(std::string_view)` returns an owning `std::vector<uint8_t>` and does not retain the input view.
It accepts both letter cases and ignores ASCII space, tab, LF, CR, form feed and vertical tab anywhere;
thus `"A B"` decodes to `0xAB`. Empty or whitespace-only input returns an empty vector.
Odd digit counts and invalid characters throw `byte_stream_error` with `invalid_value`;
`0x` prefixes, commas and colons are not accepted. Output size overflow reports `size_overflow`.
Both conversions may allocate, are independent of endian settings, and do not change the binary wire format.

## Exception handling

Errors detected by the owning `byte_stream::stream` API are reported through `byte_stream::byte_stream_error`. It derives from `std::runtime_error`; `what()` provides a diagnostic message and `code()` provides a machine-readable error category. The basic reader/writer APIs described above return error codes instead.

Ordinary `get` / `get_to` provide the lowest-overhead path. If a composite or custom codec fails, the read position and the destination of `get_to` may already have changed partially. Use `get_atomic<T>` to restore the position on failure. Use `get_to_atomic` / `get_to_with_size_atomic` to preserve both the position and destination; the destination type must support default construction and `noexcept` swapping. Use `set` for hot-path writes, and `set_atomic` when a composite codec needs rollback on failure. `set_with_size` restores the original buffer length on failure by default. Custom codecs used in atomic writes must only append bytes, without clearing or rewriting existing contents.

```cpp
try {
    auto value = stream.get<uint16_t>();
}
catch (const byte_stream::byte_stream_error& err) {
    if (err.code() == byte_stream::byte_stream_errc::insufficient_data) {
        // Not enough input data.
    }
}
```

The error categories are:

| Error code | Description |
| --- | --- |
| `ok` | Successful operation in the exception-free API |
| `invalid_size` | Invalid byte count, such as reading 3 bytes into a `uint16_t` |
| `insufficient_data` | Not enough readable bytes remaining |
| `insufficient_space` | Not enough space remaining in a fixed-capacity writer |
| `null_pointer` | A null raw or smart pointer was supplied |
| `count_out_of_range` | A fixed-array read/write count exceeds the array length |
| `missing_element_count` | A dynamic-container read did not specify an element count |
| `container_too_small` | The container has fewer elements than requested for writing |
| `trailing_data` | Unconsumed bytes remain after `parse<T>` has parsed the object |
| `invalid_value` | A wire value violates a type constraint, such as a boolean other than `00` / `01` |
| `non_canonical_encoding` | A compact integer does not use the shortest canonical encoding |
| `size_overflow` | A buffer or utility output size overflows or exceeds the container's maximum capacity |

## Project layout

```text
include/byte_stream/byte_stream.hpp       # Owning, high-level, header-only API
include/byte_stream/byte_io.hpp           # Fixed-buffer, exception-free reader/writer API
include/byte_stream/byte_stream_types.hpp # Shared byte-order and error definitions
WIRE_FORMAT.md                          # Binary format and compatibility contract
DESIGN.md                               # Ownership, allocation, atomicity, threading, and extensions
examples/                               # Runnable examples
tests/                                  # Unit, property, malformed-input, and header tests
3rdparty/googletest/                     # Bundled dependency used only for tests
benchmarks/                             # Performance workloads and regression comparison tools
cmake/                                  # Packaging and embedded smoke projects
.github/workflows/                      # CI, coverage, and release automation
```

## Design reference

The custom-type extension APIs were inspired by [nlohmann/json](https://github.com/nlohmann/json), including ADL lookup of conversion functions and intrusive/non-intrusive macros that generate those functions. In this project, these correspond to `to_byte_stream` / `from_byte_stream` and the `BYTE_STREAM_DEFINE_TYPE_*` macros. Encoding follows this project's own binary protocol rules. The core library does not depend on nlohmann/json.

## License

MIT License
