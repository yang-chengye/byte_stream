#include "byte_stream/byte_stream.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t slot_size = 64;
unsigned char* storage = nullptr;
static_assert(slot_size % alignof(std::max_align_t) == 0);
bool allocate_adjacent = false;
std::size_t next_slot = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void check_adjacent_source(std::size_t initial_size, bool replace) {
    next_slot = 0;
    allocate_adjacent = true;
    byte_stream::stream stream(byte_stream::endian::big);
    stream.reserve(slot_size);
    const std::vector<uint8_t> source(slot_size, 0xAB);
    allocate_adjacent = false;

    check(next_slot == 2 && stream.buffer().capacity() == slot_size,
        "test allocator did not supply the expected allocations");
    check(source.data() == stream.data() + stream.buffer().capacity(),
        "source must start exactly at the destination allocation's end");
    for (std::size_t i = 0; i < initial_size; ++i) {
        stream.set<uint8_t>(0x12);
    }
    check(stream.seek(initial_size), "failed to position input cursor");

    if (replace) {
        stream = source;
        check(stream.buffer() == source, "assignment did not preserve source bytes");
        check(stream.position() == 0, "assignment did not reset cursor");
    } else {
        stream.append(source);
        check(stream.size() == initial_size + source.size(), "wrong appended size");
        check(stream.position() == initial_size, "append changed cursor");
        check(std::all_of(stream.begin(), stream.begin() + initial_size,
                  [](uint8_t byte) { return byte == 0x12; }),
            "append changed the original prefix");
        check(std::equal(source.begin(), source.end(), stream.begin() + initial_size),
            "append did not preserve source bytes");
    }
    check(stream.get_endian() == byte_stream::endian::big, "endian changed");
}

} // namespace

// Isolate the allocation replacement in its own executable. Real allocators
// need not place independent buffers next to each other on every platform.
void* operator new(std::size_t size) {
    if (allocate_adjacent && size == slot_size && next_slot < 2) {
        return storage + slot_size * next_slot++;
    }
    if (void* result = std::malloc(size == 0 ? 1 : size)) {
        return result;
    }
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept {
    if (pointer != storage && pointer != storage + slot_size) {
        std::free(pointer);
    }
}

void operator delete(void* pointer, std::size_t) noexcept {
    ::operator delete(pointer);
}

int main() {
    storage = static_cast<unsigned char*>(std::malloc(2 * slot_size));
    if (storage == nullptr) {
        return 1;
    }
    try {
        for (const auto size : {std::size_t{0}, std::size_t{7}, slot_size}) {
            check_adjacent_source(size, false);
            check_adjacent_source(size, true);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        std::free(storage);
        return 1;
    }
    std::free(storage);
    return 0;
}
