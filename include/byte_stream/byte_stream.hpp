/**
 *   ____        _       ____  _                            
 *  | __ ) _   _| |_ ___/ ___|| |_ _ __ ___  __ _ _ __ ___  
 *  |  _ \| | | | __/ _ \___ \| __| '__/ _ \/ _` | '_ ` _ \ 
 *  | |_) | |_| | ||  __/___) | |_| | |  __/ (_| | | | | | |
 *  |____/ \__, |\__\___|____/ \__|_|  \___|\__,_|_| |_| |_|
 *         |___/                                            
 * https://github.com/yang-chengye/byte_stream
 * Version: 0.1.2
 * License: MIT
 */

#ifndef BYTE_STREAM_BYTE_STREAM_HPP
#define BYTE_STREAM_BYTE_STREAM_HPP

#include "byte_stream/byte_stream_types.hpp"

#include <array>
#include <cstddef>
#include <stdint.h>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace byte_stream {

/**
 * @brief 表示拥有缓冲区接口检测到的字节流错误。
 *
 * code() 提供适合程序判断的错误类别，what() 提供面向诊断的英文消息。
 */
class byte_stream_error : public std::runtime_error {
public:
    /**
     * @brief 构造带错误类别和诊断消息的异常。
     * @param code 错误类别。
     * @param message 由 what() 返回的诊断消息。
     */
    byte_stream_error(byte_stream_errc code, const std::string& message)
        : std::runtime_error(message),
          code_(code) {
    }

    /**
     * @brief 获取可机器判断的错误类别。
     * @return 构造异常时保存的错误类别。
     */
    [[nodiscard]] byte_stream_errc code() const noexcept { return code_; }

private:
    byte_stream_errc code_;
};

class stream;

/**
 * @brief 为 stream::set 和 stream::get_to 提供自定义类型编解码扩展点。
 * @tparam T 要支持的用户类型。
 * @tparam Enable 用于基于类型萃取启用偏特化的参数。
 *
 * 用户可以特化该模板，也可以提供可由 ADL 找到的 to_byte_stream 和
 * from_byte_stream 函数。编解码顺序由实现决定，并直接决定线格式。
 */
template <typename T, typename Enable = void>
struct byte_stream_codec;

/**
 * @brief 为复合对象中的动态容器声明显式的紧凑长度前缀和解码上限。
 * @tparam Container 被包装的容器类型。
 * @tparam MaxCount 解码时允许的最大元素数量。
 *
 * 该包装会改变对应字段的线格式：先写入 7 位分组的紧凑元素数量，再写入容器元素。
 */
template <typename Container, size_t MaxCount>
struct length_prefixed {
    static_assert(MaxCount < (std::numeric_limits<uint32_t>::max)(),
        "length_prefixed MaxCount must be less than UINT32_MAX");

    /** @brief 被包装的容器类型。 */
    using container_type = Container;
    /** @brief 解码允许的最大元素数量。 */
    static constexpr size_t max_count = MaxCount;

    /** @brief 实际参与编解码的容器值。 */
    Container value{};

    /**
     * @brief 默认构造一个值初始化的容器包装。
     */
    length_prefixed() = default;

    /**
     * @brief 移入一个已有容器。
     * @param initial 要转移所有权的容器值。
     */
    explicit length_prefixed(Container initial) : value(std::move(initial)) {}

    /**
     * @brief 比较两个包装对象所保存的容器值。
     * @param lhs 左操作数。
     * @param rhs 右操作数。
     * @return 两个容器相等时返回 true。
     */
    friend bool operator==(const length_prefixed& lhs, const length_prefixed& rhs) {
        return lhs.value == rhs.value;
    }
};

namespace detail {

template <typename>
struct always_false : std::false_type {};

template <typename T, typename = void>
struct has_adl_serializer : std::false_type {};

template <typename T>
struct has_adl_serializer<T, std::void_t<decltype(
    to_byte_stream(std::declval<stream&>(), std::declval<const T&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_adl_deserializer : std::false_type {};

template <typename T>
struct has_adl_deserializer<T, std::void_t<decltype(
    from_byte_stream(std::declval<const stream&>(), std::declval<T&>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct is_container : std::false_type {};

template <typename T>
struct is_container<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end()),
    decltype(std::declval<const T&>().size()),
    decltype(std::declval<T&>().clear())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_key_type : std::false_type {};

template <typename T>
struct has_key_type<T, std::void_t<typename T::key_type>> : std::true_type {};

template <typename T, typename = void>
struct has_reserve : std::false_type {};

template <typename T>
struct has_reserve<T, std::void_t<decltype(std::declval<T&>().reserve(size_t{}))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_resize : std::false_type {};

template <typename T>
struct has_resize<T, std::void_t<decltype(std::declval<T&>().resize(size_t{}))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_push_back : std::false_type {};

template <typename T>
struct has_push_back<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<T&>().push_back(std::declval<typename T::value_type>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_max_size : std::false_type {};

template <typename T>
struct has_max_size<T, std::void_t<decltype(std::declval<const T&>().max_size())>>
    : std::true_type {};

template <typename T>
struct is_byte_like
    : std::integral_constant<bool,
          sizeof(T) == 1 &&
              !std::is_same_v<std::remove_cv_t<T>, bool> &&
              (std::is_integral_v<std::remove_cv_t<T>> ||
                  std::is_enum_v<std::remove_cv_t<T>> ||
                  std::is_same_v<std::remove_cv_t<T>, std::byte>)> {};

template <typename T, typename = void>
struct is_contiguous_byte_container : std::false_type {};

template <typename T>
struct is_contiguous_byte_container<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<const T&>().data())>>
    : is_byte_like<typename T::value_type> {};

template <typename T>
struct is_fixed_width_scalar
    : std::integral_constant<bool,
          ((std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>) ||
              std::is_floating_point_v<T>) &&
              (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8)> {};

template <typename T, typename = void>
struct is_contiguous_scalar_container : std::false_type {};

template <typename T>
struct is_contiguous_scalar_container<T, std::void_t<
    typename T::value_type,
    decltype(std::declval<const T&>().data())>>
    : is_fixed_width_scalar<std::remove_cv_t<typename T::value_type>> {};

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {
    static constexpr size_t size = N;
};

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

template <typename T>
struct is_unique_ptr : std::false_type {};

template <typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {
    using element_type = T;
};

template <typename T>
struct is_shared_ptr : std::false_type {};

template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {
    using element_type = T;
};

template <typename T>
struct is_pair : std::false_type {};

template <typename First, typename Second>
struct is_pair<std::pair<First, Second>> : std::true_type {};

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
struct is_variant : std::false_type {};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template <typename T>
struct is_dynamic_container
    : std::integral_constant<bool,
          is_container<T>::value && !is_std_array<T>::value> {};

template <typename T>
struct variant_has_unframed_dynamic_container : std::false_type {};

template <typename... Ts>
struct variant_has_unframed_dynamic_container<std::variant<Ts...>>
    : std::disjunction<is_dynamic_container<Ts>...> {};

template <typename T, bool = std::is_same_v<std::remove_cv_t<T>, bool>>
struct unsigned_integral {
    using type = std::make_unsigned_t<T>;
};

template <typename T>
struct unsigned_integral<T, true> {
    using type = uint8_t;
};

template <typename T>
using unsigned_integral_t = typename unsigned_integral<T>::type;

template <typename T>
uint64_t sign_extend(uint64_t value, size_t byte_count) {
    if constexpr (std::is_signed_v<T>) {
        const size_t bits = byte_count * 8;
        if (bits < sizeof(uint64_t) * 8) {
            const uint64_t sign_bit = uint64_t{ 1 } << (bits - 1);
            if ((value & sign_bit) != 0) {
                value |= (~uint64_t{ 0 }) << bits;
            }
        }
    }
    return value;
}

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
constexpr endian host_endian() noexcept {
    return endian::little;
}
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
constexpr endian host_endian() noexcept {
    return endian::big;
}
#elif defined(_WIN32)
constexpr endian host_endian() noexcept {
    return endian::little;
}
#else
inline endian host_endian() noexcept {
    const uint16_t value = 1;
    uint8_t first = 0;
    std::memcpy(&first, &value, sizeof(first));
    return first == 1 ? endian::little : endian::big;
}
#endif

inline uint16_t byteswap16(uint16_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ushort(value);
#elif defined(__has_builtin)
#if __has_builtin(__builtin_bswap16)
    return __builtin_bswap16(value);
#else
    return static_cast<uint16_t>((value << 8) | (value >> 8));
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(value);
#else
    return static_cast<uint16_t>((value << 8) | (value >> 8));
#endif
}

inline uint32_t byteswap32(uint32_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#elif defined(__has_builtin)
#if __has_builtin(__builtin_bswap32)
    return __builtin_bswap32(value);
#else
    return ((value & 0x000000FFu) << 24) |
        ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) |
        ((value & 0xFF000000u) >> 24);
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    return ((value & 0x000000FFu) << 24) |
        ((value & 0x0000FF00u) << 8) |
        ((value & 0x00FF0000u) >> 8) |
        ((value & 0xFF000000u) >> 24);
#endif
}

inline uint64_t byteswap64(uint64_t value) noexcept {
#if defined(_MSC_VER)
    return _byteswap_uint64(value);
#elif defined(__has_builtin)
#if __has_builtin(__builtin_bswap64)
    return __builtin_bswap64(value);
#else
    return ((value & 0x00000000000000FFull) << 56) |
        ((value & 0x000000000000FF00ull) << 40) |
        ((value & 0x0000000000FF0000ull) << 24) |
        ((value & 0x00000000FF000000ull) << 8) |
        ((value & 0x000000FF00000000ull) >> 8) |
        ((value & 0x0000FF0000000000ull) >> 24) |
        ((value & 0x00FF000000000000ull) >> 40) |
        ((value & 0xFF00000000000000ull) >> 56);
#endif
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(value);
#else
    return ((value & 0x00000000000000FFull) << 56) |
        ((value & 0x000000000000FF00ull) << 40) |
        ((value & 0x0000000000FF0000ull) << 24) |
        ((value & 0x00000000FF000000ull) << 8) |
        ((value & 0x000000FF00000000ull) >> 8) |
        ((value & 0x0000FF0000000000ull) >> 24) |
        ((value & 0x00FF000000000000ull) >> 40) |
        ((value & 0xFF00000000000000ull) >> 56);
#endif
}

template <typename T>
T byteswap_uint(T value) noexcept {
    static_assert(std::is_unsigned_v<T>, "byteswap_uint expects an unsigned integer");
    if constexpr (sizeof(T) == sizeof(uint16_t)) {
        return static_cast<T>(byteswap16(static_cast<uint16_t>(value)));
    }
    else if constexpr (sizeof(T) == sizeof(uint32_t)) {
        return static_cast<T>(byteswap32(static_cast<uint32_t>(value)));
    }
    else if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return static_cast<T>(byteswap64(static_cast<uint64_t>(value)));
    }
    else {
        return value;
    }
}

template <typename T>
struct can_raw_copy_scalar
    : std::integral_constant<bool,
          std::is_floating_point_v<T> ||
              (std::is_integral_v<T> &&
                  std::is_unsigned_v<T> &&
                  !std::is_same_v<std::remove_cv_t<T>, bool>)> {};

template <typename T>
uint64_t scalar_to_uint(T value) {
    if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) == sizeof(float)) {
            uint32_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            return raw;
        }
        else {
            uint64_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            return raw;
        }
    }
    else {
        using U = unsigned_integral_t<T>;
        return static_cast<uint64_t>(static_cast<U>(value));
    }
}

template <typename T>
T scalar_from_uint(uint64_t raw) {
    T value{};
    if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) == sizeof(float)) {
            const uint32_t bits = static_cast<uint32_t>(raw);
            std::memcpy(&value, &bits, sizeof(value));
        }
        else {
            std::memcpy(&value, &raw, sizeof(value));
        }
    }
    else if constexpr (std::is_signed_v<T>) {
        using U = unsigned_integral_t<T>;
        const U bits = static_cast<U>(raw);
        std::memcpy(&value, &bits, sizeof(value));
    }
    else {
        value = static_cast<T>(raw);
    }
    return value;
}

template <typename T, bool = std::is_enum_v<T>>
struct compact_underlying {
    using type = T;
};

template <typename T>
struct compact_underlying<T, true> {
    using type = std::underlying_type_t<T>;
};

template <typename T>
using compact_underlying_t = typename compact_underlying<T>::type;

template <typename T>
uint64_t compact_encode(T value) {
    using value_type = std::remove_cv_t<T>;
    using wire_type = compact_underlying_t<value_type>;
    using U = unsigned_integral_t<wire_type>;
    U raw = static_cast<U>(static_cast<wire_type>(value));

    if constexpr (std::is_signed_v<wire_type>) {
        constexpr size_t bits = sizeof(wire_type) * 8;
        raw = static_cast<U>((raw << 1) ^ (U{ 0 } - (raw >> (bits - 1))));
    }

    return static_cast<uint64_t>(raw);
}

template <typename T>
T compact_decode(uint64_t raw_value) {
    using value_type = std::remove_cv_t<T>;
    using wire_type = compact_underlying_t<value_type>;
    using U = unsigned_integral_t<wire_type>;
    U raw = static_cast<U>(raw_value);

    if constexpr (std::is_signed_v<wire_type>) {
        raw = static_cast<U>((raw >> 1) ^ (U{ 0 } - (raw & U{ 1 })));
        wire_type decoded{};
        std::memcpy(&decoded, &raw, sizeof(decoded));
        if constexpr (std::is_enum_v<value_type>) {
            return static_cast<value_type>(decoded);
        }
        else {
            return decoded;
        }
    }
    else {
        if constexpr (std::is_enum_v<value_type>) {
            return static_cast<value_type>(static_cast<wire_type>(raw));
        }
        else {
            return static_cast<value_type>(raw);
        }
    }
}

} // namespace detail

/**
 * @brief 按字段顺序编码和解码二进制协议的自持有字节流。
 *
 * stream 使用 std::vector<uint8_t> 持有全部字节，可按需扩容，并维护独立的读取位置。
 * set 系列函数追加数据，get 系列函数从当前位置读取。多字节标量默认使用小端线格式；
 * 动态容器默认需要调用者显式提供元素数量，或使用带长度前缀的接口。
 */
class stream {
public:
    /** @brief 完整字节缓冲区的只读迭代器类型。 */
    using const_iterator = std::vector<uint8_t>::const_iterator;

    /**
     * @brief 表示使用类型自然宽度或默认元素数量语义的参数哨兵值。
     */
    static constexpr uint32_t default_size = (std::numeric_limits<uint32_t>::max)();

    /**
     * @brief 构造空字节流，读取位置为 0，字节序为小端。
     */
    stream() = default;

    /**
     * @brief 构造指定字节序的空流，读取位置为 0。
     * @param byte_order 后续读写使用的字节序。
     */
    explicit stream(endian byte_order) noexcept : endian_(byte_order) {}

    /**
     * @brief 复制或移动字节流。
     *
     * 复制会保留缓冲区、读取位置和字节序；移动后的源对象保持有效但内容未指定。
     */
    stream(const stream&) = default;
    stream(stream&&) noexcept = default;
    stream& operator=(const stream&) = default;
    stream& operator=(stream&&) noexcept = default;

    /**
     * @brief 使用当前字节序编码对象，替换全部数据并将读取位置归零。
     * @return 当前流的引用。
     * @note 复用已有容量；失败时原数据已清除，可能留下部分新数据。
     * value 不得引用本流的 buffer 或其中元素。自定义 codec 应只追加数据，
     * 不改变字节序或读取位置。流之间的赋值复制或移动完整状态。
     */
    template <typename T, std::enable_if_t<!std::is_base_of_v<stream, std::decay_t<T>>, int> = 0>
    stream& operator=(const T& value) {
        clear();
        set(value);
        return *this;
    }

    /**
     * @brief 复制已有字节数组并构造字节流。
     * @param data 要复制的完整字节内容。
     */
    stream(const std::vector<uint8_t>& data) : buf_(data) {}

    /**
     * @brief 接管已有字节数组并构造字节流。
     * @param data 要移动到流内的字节数组。
     */
    stream(std::vector<uint8_t>&& data) noexcept : buf_(std::move(data)) {}

    /**
     * @brief 复制指定原始字节并构造字节流。
     * @param data 输入首地址；size 为 0 时可以为空指针。
     * @param size 要复制的字节数。
     * @throws byte_stream_error 非零长度使用空指针或长度计算溢出时抛出。
     */
    stream(const void* data, size_t size) {
        append(data, size);
    }

    /**
     * @brief 将另一个流的完整缓冲区追加到当前流。
     * @param other 要追加的流；允许传入当前对象自身。
     * @return 当前流的引用。
     * @throws byte_stream_error 结果长度溢出时抛出。
     */
    stream& operator+=(const stream& other) {
        append(other.buf_);
        return *this;
    }

    /**
     * @brief 连接两个流的完整缓冲区并返回新流。
     * @param other 右侧流。
     * @return 包含当前流字节后接 other 字节的新流。
     * @throws byte_stream_error 结果长度溢出时抛出。
     */
    [[nodiscard]] stream operator+(const stream& other) const {
        stream result(*this);
        result += other;
        return result;
    }

    /**
     * @brief 追加另一个流的完整缓冲区，不受其读取位置影响。
     * @param other 要追加的流；允许传入当前对象自身。
     * @throws byte_stream_error 结果长度溢出时抛出。
     */
    void append(const stream& other) {
        if (&other == this) {
            append_bytes(buf_.data(), buf_.size());
            return;
        }
        append(other.buf_);
    }

    /**
     * @brief 追加字节数组的全部内容。
     * @param data 要追加的字节数组；允许传入本流的 buffer() 引用。
     * @throws byte_stream_error 结果长度溢出时抛出。
     */
    void append(const std::vector<uint8_t>& data) {
        if (&data == &buf_) {
            append_bytes(buf_.data(), buf_.size());
            return;
        }
        append(data.data(), data.size());
    }

    /**
     * @brief 追加一段原始字节。
     * @param data 输入首地址；size 为 0 时可以为空指针。
     * @param size 要追加的字节数。
     * @throws byte_stream_error 非零长度使用空指针或结果长度溢出时抛出。
     */
    void append(const void* data, size_t size) {
        if (size == 0) {
            return;
        }
        if (data == nullptr) {
            throw byte_stream_error(byte_stream_errc::null_pointer,
                "byte_stream: null data pointer");
        }

        append_bytes(data, size);
    }

    /**
     * @brief 使用对应 codec 将一个值追加到缓冲区。
     * @tparam T 支持内置 codec、用户特化或 ADL 编解码函数的类型。
     * @param value 要编码的值。
     * @param n 整数和枚举的字节数，或容器的元素数量；默认值采用类型默认语义。
     * @throws byte_stream_error 输入参数、容量计算或编码数据无效时抛出。
     * @note 复合 codec 失败前可能已经追加部分字段；需要回滚时使用 set_atomic。
     */
    template <typename T>
    void set(const T& value, uint32_t n = default_size) {
        byte_stream_codec<std::remove_cv_t<T>>::write(*this, value, n);
    }

    /**
     * @brief 原子地编码一个值，失败时恢复调用前的缓冲区长度。
     * @tparam T 支持 stream 编码的类型。
     * @param value 要编码的值。
     * @param n 与 set 相同的宽度或元素数量参数。
     * @throws byte_stream_error 编码失败时在回滚后重新抛出。
     * @note 自定义 codec 应只向末尾追加数据，不应修改此前已有字节。
     */
    template <typename T>
    void set_atomic(const T& value, uint32_t n = default_size) {
        const size_t old_size = buf_.size();
        try {
            set(value, n);
        }
        catch (...) {
            buf_.resize(old_size);
            throw;
        }
    }

    /**
     * @brief 使用最低额外开销的原地路径读取到已有对象。
     * @tparam T 支持内置 codec、用户特化或 ADL 编解码函数的类型。
     * @param value 接收解码结果。
     * @param n 整数和枚举的字节数，或容器的元素数量；默认值采用类型默认语义。
     * @throws byte_stream_error 输入截断、参数无效或值不合法时抛出。
     * @note 复合对象失败时，读取位置和目标对象都可能已发生部分改变。
     */
    template <typename T>
    void get_to(T& value, uint32_t n = default_size) const {
        byte_stream_codec<std::remove_cv_t<T>>::read(*this, value, n);
    }

    /**
     * @brief 读取到已有对象，并为读取位置和目标值提供强失败原子性。
     * @tparam T 可默认构造且可无异常交换的可解码类型。
     * @param value 接收解码结果；失败时保持原值。
     * @param n 与 get_to 相同的宽度或元素数量参数。
     * @throws byte_stream_error 解码失败时在恢复读取位置后重新抛出。
     */
    template <typename T>
    void get_to_atomic(T& value, uint32_t n = default_size) const {
        read_into_atomic(value, [this, n](T& decoded) {
            byte_stream_codec<std::remove_cv_t<T>>::read(*this, decoded, n);
        });
    }

    /**
     * @brief 从当前位置解码并返回一个值。
     * @tparam T 可默认构造且支持 stream 解码的类型。
     * @param n 与 get_to 相同的宽度或元素数量参数。
     * @return 解码后的值。
     * @throws byte_stream_error 解码失败时抛出；复合类型可能已推进读取位置。
     */
    template <typename T>
    [[nodiscard]] T get(uint32_t n = default_size) const {
        static_assert(std::is_default_constructible_v<T>,
            "byte_stream::stream::get<T> requires a default-constructible T");
        T value{};
        byte_stream_codec<std::remove_cv_t<T>>::read(*this, value, n);
        return value;
    }

    /**
     * @brief 解码并返回一个值，失败时恢复读取位置。
     * @tparam T 可默认构造且支持 stream 解码的类型。
     * @param n 与 get_to 相同的宽度或元素数量参数。
     * @return 解码后的值。
     * @throws byte_stream_error 解码失败时在恢复读取位置后重新抛出。
     */
    template <typename T>
    [[nodiscard]] T get_atomic(uint32_t n = default_size) const {
        static_assert(std::is_default_constructible_v<T>,
            "byte_stream::stream::get_atomic<T> requires a default-constructible T");
        T value{};
        read_transaction([this, &value, n] {
            byte_stream_codec<std::remove_cv_t<T>>::read(*this, value, n);
        });
        return value;
    }

    /**
     * @brief 使用 7 位分组变长格式追加整数或枚举。
     *
     * 有符号整数先经过 zigzag 编码，使绝对值较小的负数也保持紧凑。
     * @tparam T 整数类型或枚举类型。
     * @param value 要编码的值。
     * @throws byte_stream_error 缓冲区长度计算溢出时抛出。
     */
    template <typename T>
    void set_compact(T value) {
        using value_type = std::remove_cv_t<T>;
        static_assert(std::is_integral_v<value_type> || std::is_enum_v<value_type>,
            "byte_stream::stream::set_compact supports only integral and enum types");
        write_compact_uint(detail::compact_encode(value));
    }

    /**
     * @brief 将 set_compact 写入的整数或枚举解码到已有对象。
     * @tparam T 整数类型或枚举类型。
     * @param value 接收结果；失败时保持原值。
     * @throws byte_stream_error 数据截断、超宽或编码不规范时抛出，且读取位置保持不变。
     */
    template <typename T>
    void get_compact_to(T& value) const {
        using value_type = std::remove_cv_t<T>;
        using wire_type = detail::compact_underlying_t<value_type>;
        static_assert(std::is_integral_v<value_type> || std::is_enum_v<value_type>,
            "byte_stream::stream::get_compact_to supports only integral and enum types");
        const auto decoded = detail::compact_decode<value_type>(
            read_compact_uint(sizeof(wire_type) * 8));
        value = decoded;
    }

    /**
     * @brief 解码并返回一个紧凑编码的整数或枚举。
     * @tparam T 整数类型或枚举类型。
     * @return 解码后的值。
     * @throws byte_stream_error 数据截断、超宽或编码不规范时抛出，且读取位置保持不变。
     */
    template <typename T>
    [[nodiscard]] T get_compact() const {
        using value_type = std::remove_cv_t<T>;
        using wire_type = detail::compact_underlying_t<value_type>;
        static_assert(std::is_integral_v<value_type> || std::is_enum_v<value_type>,
            "byte_stream::stream::get_compact supports only integral and enum types");
        return detail::compact_decode<value_type>(read_compact_uint(sizeof(wire_type) * 8));
    }

    /**
     * @brief 先写入紧凑元素数量，再写入容器内容。
     * @tparam Container 具有 size() 且支持 stream 编码的容器类型。
     * @param value 要编码的容器。
     * @throws byte_stream_error 元素数量无法表示、容量溢出或元素编码失败时抛出。
     * @note 整个操作具有缓冲区长度原子性，失败时不会保留已追加字节。
     */
    template <typename Container>
    void set_with_size(const Container& value) {
        const size_t old_size = buf_.size();
        try {
            if (value.size() >= default_size) {
                throw byte_stream_error(byte_stream_errc::count_out_of_range,
                    "byte_stream: container size cannot be represented by the element-count API");
            }
            set_compact(static_cast<uint32_t>(value.size()));
            set(value);
        }
        catch (...) {
            buf_.resize(old_size);
            throw;
        }
    }

    /**
     * @brief 读取带紧凑元素数量前缀的容器。
     * @tparam Container 支持 stream 解码的容器类型。
     * @param value 接收容器内容。
     * @param max_count 调用者允许的最大元素数量，用于限制不可信输入触发的分配。
     * @throws byte_stream_error 数量越界、输入截断或元素无效时抛出。
     * @note 失败时恢复读取位置，但 value 可能已经被部分修改。
     */
    template <typename Container>
    void get_to_with_size(Container& value, size_t max_count) const {
        read_transaction([this, &value, max_count] {
            const auto count = get_compact<uint32_t>();
            if (count > max_count || count >= default_size) {
                throw byte_stream_error(byte_stream_errc::count_out_of_range,
                    "byte_stream: compact container size exceeds allowed count");
            }
            get_to(value, count);
        });
    }

    /**
     * @brief 读取带紧凑长度前缀的容器，并为位置和目标值提供强失败原子性。
     * @tparam Container 可默认构造、可无异常交换且支持 stream 解码的容器类型。
     * @param value 接收结果；失败时保持原值。
     * @param max_count 调用者允许的最大元素数量。
     * @throws byte_stream_error 数量越界、输入截断或元素无效时抛出。
     */
    template <typename Container>
    void get_to_with_size_atomic(Container& value, size_t max_count) const {
        read_into_atomic(value, [this, max_count](Container& decoded) {
            const auto count = get_compact<uint32_t>();
            if (count > max_count || count >= default_size) {
                throw byte_stream_error(byte_stream_errc::count_out_of_range,
                    "byte_stream: compact container size exceeds allowed count");
            }
            get_to(decoded, count);
        });
    }

    /**
     * @brief 从字节数组解析恰好一个完整对象，并拒绝尾随数据。
     * @tparam T 可默认构造且支持 stream 解码的类型。
     * @param data 完整编码数据。
     * @param byte_order 解析多字节标量使用的线字节序。
     * @return 解析得到的对象。
     * @throws byte_stream_error 输入无效、数据截断或解析后仍有剩余字节时抛出。
     */
    template <typename T>
    [[nodiscard]] static T parse(
        const std::vector<uint8_t>& data,
        endian byte_order = endian::little) {
        stream bs(data);
        bs.set_endian(byte_order);
        T obj{};
        bs.get_to(obj);
        bs.ensure_fully_consumed();
        return obj;
    }

    /**
     * @brief 从字符串的原始字节解析恰好一个完整对象，并拒绝尾随数据。
     * @tparam T 可默认构造且支持 stream 解码的类型。
     * @param str 保存完整编码数据的字符串；不执行字符集转换。
     * @param byte_order 解析多字节标量使用的线字节序。
     * @return 解析得到的对象。
     * @throws byte_stream_error 输入无效、数据截断或解析后仍有剩余字节时抛出。
     */
    template <typename T>
    [[nodiscard]] static T parse(
        const std::string& str,
        endian byte_order = endian::little) {
        stream bs(str.data(), str.size());
        bs.set_endian(byte_order);
        T obj{};
        bs.get_to(obj);
        bs.ensure_fully_consumed();
        return obj;
    }

    /**
     * @brief 从原始字节解析恰好一个完整对象，并拒绝尾随数据。
     * @tparam T 可默认构造且支持 stream 解码的类型。
     * @param data 输入首地址；size 为 0 时可以为空指针。
     * @param size 输入字节数。
     * @param byte_order 解析多字节标量使用的线字节序。
     * @return 解析得到的对象。
     * @throws byte_stream_error 指针、输入数据或尾随字节无效时抛出。
     */
    template <typename T>
    [[nodiscard]] static T parse(
        const void* data,
        size_t size,
        endian byte_order = endian::little) {
        stream bs(data, size);
        bs.set_endian(byte_order);
        T obj{};
        bs.get_to(obj);
        bs.ensure_fully_consumed();
        return obj;
    }

    /**
     * @brief 将当前完整缓冲区转换为十六进制字符串。
     * @param with_spaces 为 true 时在相邻字节之间插入一个空格。
     * @param uppercase 为 true 时使用大写字母，默认为小写。
     * @return 十六进制字符串；空缓冲区返回空字符串。
     * @throws byte_stream_error 结果长度溢出或超过字符串最大容量时抛出。
     */
    [[nodiscard]] std::string to_hex(bool with_spaces = false, bool uppercase = false) const {
        return to_hex(buf_, with_spaces, uppercase);
    }

    /**
     * @brief 将指定字节数组转换为十六进制字符串。
     * @param data 要格式化的字节数组。
     * @param with_spaces 为 true 时在相邻字节之间插入一个空格。
     * @param uppercase 为 true 时使用大写字母，默认为小写。
     * @return 十六进制字符串；空数组返回空字符串。
     * @throws byte_stream_error 结果长度溢出或超过字符串最大容量时抛出。
     */
    [[nodiscard]] static std::string to_hex(
        const std::vector<uint8_t>& data, bool with_spaces = false, bool uppercase = false) {
        if (data.empty()) {
            return {};
        }

        const char* const hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        if (data.size() > (std::numeric_limits<size_t>::max)() / 2) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: hexadecimal string size overflow");
        }
        size_t result_size = data.size() * 2;
        if (with_spaces && data.size() > 1) {
            const size_t separator_count = data.size() - 1;
            if (separator_count > (std::numeric_limits<size_t>::max)() - result_size) {
                throw byte_stream_error(byte_stream_errc::size_overflow,
                    "byte_stream: hexadecimal string size overflow");
            }
            result_size += separator_count;
        }

        std::string result;
        if (result_size > result.max_size()) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: hexadecimal string exceeds string maximum");
        }
        result.resize(result_size);
        size_t output = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            uint8_t byte = data[i];
            result[output++] = hex_chars[(byte >> 4) & 0x0F];
            result[output++] = hex_chars[byte & 0x0F];
            if (with_spaces && i + 1 != data.size()) {
                result[output++] = ' ';
            }
        }
        return result;
    }

    /**
     * @brief 将十六进制文本转换为独立持有的字节数组，不保留输入视图。
     * @param text 接受大小写混合，忽略 ASCII 空白（空格、\t、\n、\r、\f、\v）。
     * @return 解码后的字节数组；空文本或全空白返回空数组。
     * @throws byte_stream_error 非法字符或奇数个十六进制字符抛出 invalid_value；
     * 输出超过 vector 最大容量时抛出 size_overflow。不支持 0x 前缀或其他分隔符。
     */
    [[nodiscard]] static std::vector<uint8_t> from_hex(std::string_view text) {
        const auto is_space = [](char ch) noexcept {
            return ch == ' ' || ch == '\t' || ch == '\n' ||
                ch == '\r' || ch == '\f' || ch == '\v';
        };
        const auto digit = [](char ch) noexcept -> int {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f') {
                return ch - 'a' + 10;
            }
            if (ch >= 'A' && ch <= 'F') {
                return ch - 'A' + 10;
            }
            return -1;
        };

        // 先验证并计数，避免为畸形输入或大量空白分配输出存储。
        size_t digit_count = 0;
        for (char ch : text) {
            if (is_space(ch)) {
                continue;
            }
            if (digit(ch) < 0) {
                throw byte_stream_error(byte_stream_errc::invalid_value,
                    "byte_stream: invalid hexadecimal character");
            }
            ++digit_count;
        }
        if (digit_count % 2 != 0) {
            throw byte_stream_error(byte_stream_errc::invalid_value,
                "byte_stream: odd hexadecimal digit count");
        }

        std::vector<uint8_t> result;
        if (digit_count / 2 > result.max_size()) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: hexadecimal output exceeds vector maximum");
        }
        result.reserve(digit_count / 2);
        int high = -1;
        for (char ch : text) {
            if (is_space(ch)) {
                continue;
            }
            const int value = digit(ch);
            if (high < 0) {
                high = value;
            }
            else {
                result.push_back(static_cast<uint8_t>((high << 4) | value));
                high = -1;
            }
        }
        return result;
    }

    /**
     * @brief 获取流所持有字节缓冲区的只读引用。
     * @return 内部 vector 的引用；流被修改、移动或销毁后，该引用及其迭代器可能失效。
     */
    [[nodiscard]] const std::vector<uint8_t>& buffer() const noexcept { return buf_; }

    /**
     * @brief 不检查边界地访问完整缓冲区中的一个字节。
     * @param index 相对于缓冲区起点的字节下标，与读取位置无关。
     * @return 指定字节的只读引用；stream 修改、移动或销毁后可能失效。
     * @note index 必须小于 size()，否则行为未定义。
     */
    [[nodiscard]] const uint8_t& operator[](size_t index) const noexcept {
        return buf_[index];
    }

    /**
     * @brief 检查边界地访问完整缓冲区中的一个字节。
     * @param index 相对于缓冲区起点的字节下标，与读取位置无关。
     * @return 指定字节的只读引用；stream 修改、移动或销毁后可能失效。
     * @throws byte_stream_error index 不小于 size() 时抛出 invalid_size。
     */
    [[nodiscard]] const uint8_t& at(size_t index) const {
        if (index >= buf_.size()) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: byte index is outside the buffer");
        }
        return buf_[index];
    }

    /**
     * @brief 获取完整缓冲区起点的只读迭代器，与读取位置无关。
     * @return 指向首字节的只读迭代器。
     */
    [[nodiscard]] const_iterator begin() const noexcept { return buf_.cbegin(); }

    /**
     * @brief 获取完整缓冲区末尾的只读迭代器，与读取位置无关。
     * @return 指向末尾后一位置的只读迭代器。
     */
    [[nodiscard]] const_iterator end() const noexcept { return buf_.cend(); }

    /** @brief 等价于 begin() 的显式只读迭代器接口。 */
    [[nodiscard]] const_iterator cbegin() const noexcept { return buf_.cbegin(); }

    /** @brief 等价于 end() 的显式只读迭代器接口。 */
    [[nodiscard]] const_iterator cend() const noexcept { return buf_.cend(); }

    /**
     * @brief 获取内部连续字节存储的首地址。
     * @return 只读指针；空缓冲区时可能为空，缓冲区重分配后会失效。
     */
    [[nodiscard]] const uint8_t* data() const noexcept { return buf_.data(); }

    /**
     * @brief 将内部字节数组的所有权移出，并复位读取位置。
     * @return 原内部缓冲区；调用后当前流为空，但字节序设置保持不变。
     */
    [[nodiscard]] std::vector<uint8_t> take_buffer() noexcept {
        pos_ = 0;
        return std::exchange(buf_, {});
    }

    /**
     * @brief 预留至少指定容量，以减少连续写入时的重复分配和复制。
     * @param new_capacity 期望的最小容量，单位为字节。
     * @throws byte_stream_error 请求容量超过 vector 最大容量时抛出。
     */
    void reserve(size_t new_capacity) {
        if (new_capacity > buf_.max_size()) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: requested capacity exceeds buffer maximum");
        }
        buf_.reserve(new_capacity);
    }

    /**
     * @brief 清空逻辑字节内容并将读取位置复位到起点。
     *
     * 此操作保留已分配容量和当前字节序，便于复用流对象。
     */
    void clear() noexcept {
        buf_.clear();
        pos_ = 0;
    }

    /**
     * @brief 将读取位置移动到缓冲区内的指定偏移。
     * @param new_pos 相对于缓冲区起点的目标字节偏移。
     * @return 目标不超过缓冲区长度时返回 true，否则保持原位置并返回 false。
     */
    [[nodiscard]] bool seek(size_t new_pos) const noexcept {
        if (new_pos > buf_.size()) {
            return false;
        }
        pos_ = new_pos;
        return true;
    }

    /**
     * @brief 将读取位置复位到缓冲区起点，不改变内容。
     */
    void reset_position() const noexcept { pos_ = 0; }

    /**
     * @brief 获取当前读取位置。
     * @return 相对于缓冲区起点的字节偏移。
     */
    [[nodiscard]] size_t position() const noexcept { return pos_; }

    /**
     * @brief 获取缓冲区逻辑长度。
     * @return 缓冲区包含的字节数。
     */
    [[nodiscard]] size_t size() const noexcept { return buf_.size(); }

    /**
     * @brief 判断缓冲区是否为空。
     * @return 缓冲区不包含字节时返回 true。
     */
    [[nodiscard]] bool empty() const noexcept { return buf_.empty(); }

    /**
     * @brief 判断当前位置是否已经到达缓冲区末尾。
     * @return 没有剩余可读字节时返回 true。
     */
    [[nodiscard]] bool eof() const noexcept { return remaining() == 0; }

    /**
     * @brief 获取从当前位置到缓冲区末尾的剩余字节数。
     * @return 剩余可读字节数。
     */
    [[nodiscard]] size_t remaining() const noexcept {
        return pos_ <= buf_.size() ? buf_.size() - pos_ : 0;
    }

    /**
     * @brief 设置后续整数、枚举和浮点数读写使用的线字节序。
     * @param e 新字节序；不会转换缓冲区内已经存在的数据。
     */
    void set_endian(endian e) noexcept { endian_ = e; }

    /**
     * @brief 获取当前线字节序。
     * @return 当前字节序。
     */
    [[nodiscard]] endian get_endian() const noexcept { return endian_; }

private:
    template <typename, typename>
    friend struct byte_stream_codec;

    std::vector<uint8_t> buf_;
    mutable size_t pos_{ 0 };
    mutable size_t read_depth_{ 0 };
    endian endian_{ endian::little };

    static constexpr bool is_default_size(uint32_t n) noexcept {
        return n == default_size;
    }

    template <typename Reader>
    void read_transaction(Reader&& reader) const {
        if (read_depth_ != 0) {
            reader();
            return;
        }
        const size_t old_pos = pos_;
        ++read_depth_;
        try {
            reader();
        }
        catch (...) {
            --read_depth_;
            pos_ = old_pos;
            throw;
        }
        --read_depth_;
    }

    template <typename T, typename Reader>
    void read_into_atomic(T& value, Reader&& reader) const {
        static_assert(std::is_default_constructible_v<T>,
            "atomic byte_stream reads require a default-constructible destination type");
        static_assert(std::is_nothrow_swappable_v<T>,
            "atomic byte_stream reads require a nothrow-swappable destination type");

        T decoded{};
        read_transaction([&reader, &decoded] {
            reader(decoded);
        });
        using std::swap;
        swap(value, decoded);
    }

    void append_bytes(const void* data, size_t size) {
        if (size == 0) {
            return;
        }

        // 处理源数据位于 buf_ 内部的情况，因为 resize 可能导致底层存储重分配。
        const auto* first = static_cast<const uint8_t*>(data);
        const size_t old_size = buf_.size();
        size_t source_offset = 0;
        bool source_in_buffer = false;

        if (buf_.capacity() != 0) {
            const auto begin = reinterpret_cast<uintptr_t>(buf_.data());
            const auto logical_end = begin + old_size;
            const auto allocation_end = begin + buf_.capacity();
            const auto source = reinterpret_cast<uintptr_t>(first);
            if (source >= begin && source <= logical_end) {
                source_offset = static_cast<size_t>(source - begin);
                source_in_buffer = true;
            }
            else if (source > logical_end && source <= allocation_end) {
                throw byte_stream_error(byte_stream_errc::invalid_size,
                    "byte_stream: appended source is outside the current logical buffer");
            }
        }

        if (source_in_buffer && size > old_size - source_offset) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: appended source range exceeds the current buffer");
        }

        const size_t new_size = checked_new_buffer_size(size);

        if (!source_in_buffer) {
            buf_.insert(buf_.end(), first, first + size);
            return;
        }

        buf_.resize(new_size);
        std::memcpy(buf_.data() + old_size, buf_.data() + source_offset, size);
    }

    void read_bytes(void* data, size_t size) const {
        if (size == 0) {
            return;
        }
        if (data == nullptr) {
            throw byte_stream_error(byte_stream_errc::null_pointer,
                "byte_stream: null data pointer");
        }

        ensure(size);
        read_bytes_unchecked(data, size);
    }

    void ensure(size_t n) const {
        if (n > remaining()) {
            throw_insufficient_data(n);
        }
    }

    void ensure_fully_consumed() const {
        if (!eof()) {
            throw byte_stream_error(byte_stream_errc::trailing_data,
                "byte_stream: trailing data after complete object");
        }
    }

    [[noreturn]] void throw_insufficient_data(size_t n) const {
        throw byte_stream_error(
            byte_stream_errc::insufficient_data,
            "byte_stream: not enough data, pos=" + std::to_string(pos_) +
            ", need=" + std::to_string(n) +
            ", remaining=" + std::to_string(remaining()));
    }

    void read_bytes_unchecked(void* data, size_t size) const noexcept {
        if (size == 0) {
            return;
        }
        std::memcpy(data, buf_.data() + pos_, size);
        pos_ += size;
    }

    template <typename UInt>
    void write_fixed_uint(UInt value) {
        static_assert(std::is_unsigned_v<UInt>, "write_fixed_uint expects an unsigned integer");
        if constexpr (sizeof(UInt) == sizeof(uint8_t)) {
            try {
                buf_.push_back(static_cast<uint8_t>(value));
            }
            catch (const std::length_error&) {
                throw byte_stream_error(byte_stream_errc::size_overflow,
                    "byte_stream: buffer size exceeds vector maximum");
            }
            return;
        }

        if (endian_ != detail::host_endian()) {
            value = detail::byteswap_uint(value);
        }

        const size_t old_size = buf_.size();
        resize_for_bounded_append(sizeof(UInt));
        std::memcpy(buf_.data() + old_size, &value, sizeof(UInt));
    }

    template <typename UInt>
    UInt read_fixed_uint() const {
        static_assert(std::is_unsigned_v<UInt>, "read_fixed_uint expects an unsigned integer");
        UInt value = 0;
        ensure(sizeof(UInt));
        if constexpr (sizeof(UInt) == sizeof(uint8_t)) {
            value = buf_[pos_++];
            return value;
        }

        std::memcpy(&value, buf_.data() + pos_, sizeof(UInt));
        pos_ += sizeof(UInt);
        if (endian_ != detail::host_endian()) {
            value = detail::byteswap_uint(value);
        }
        return value;
    }

    static void store_scalar_value(uint8_t* out, uint64_t raw, size_t byte_count, bool swap_bytes) {
        if (byte_count == sizeof(uint8_t)) {
            out[0] = static_cast<uint8_t>(raw);
            return;
        }

        if (byte_count == sizeof(uint16_t)) {
            auto value = static_cast<uint16_t>(raw);
            if (swap_bytes) {
                value = detail::byteswap16(value);
            }
            std::memcpy(out, &value, sizeof(value));
            return;
        }

        if (byte_count == sizeof(uint32_t)) {
            auto value = static_cast<uint32_t>(raw);
            if (swap_bytes) {
                value = detail::byteswap32(value);
            }
            std::memcpy(out, &value, sizeof(value));
            return;
        }

        auto value = static_cast<uint64_t>(raw);
        if (swap_bytes) {
            value = detail::byteswap64(value);
        }
        std::memcpy(out, &value, sizeof(value));
    }

    static uint64_t load_scalar_value(const uint8_t* in, size_t byte_count, bool swap_bytes) {
        if (byte_count == sizeof(uint8_t)) {
            return in[0];
        }

        if (byte_count == sizeof(uint16_t)) {
            uint16_t value = 0;
            std::memcpy(&value, in, sizeof(value));
            return swap_bytes ? detail::byteswap16(value) : value;
        }

        if (byte_count == sizeof(uint32_t)) {
            uint32_t value = 0;
            std::memcpy(&value, in, sizeof(value));
            return swap_bytes ? detail::byteswap32(value) : value;
        }

        uint64_t value = 0;
        std::memcpy(&value, in, sizeof(value));
        return swap_bytes ? detail::byteswap64(value) : value;
    }

    void write_uint(uint64_t value, size_t byte_count) {
        switch (byte_count) {
        case sizeof(uint8_t):
            write_fixed_uint(static_cast<uint8_t>(value));
            return;
        case sizeof(uint16_t):
            write_fixed_uint(static_cast<uint16_t>(value));
            return;
        case sizeof(uint32_t):
            write_fixed_uint(static_cast<uint32_t>(value));
            return;
        case sizeof(uint64_t):
            write_fixed_uint(static_cast<uint64_t>(value));
            return;
        default:
            break;
        }

        const size_t old_size = buf_.size();
        resize_for_bounded_append(byte_count);
        auto* out = buf_.data() + old_size;

        for (size_t i = 0; i < byte_count; ++i) {
            const size_t shift = (endian_ == endian::little)
                ? i * 8
                : (byte_count - 1 - i) * 8;
            out[i] = static_cast<uint8_t>((value >> shift) & 0xFFu);
        }
    }

    uint64_t read_uint(size_t byte_count) const {
        switch (byte_count) {
        case sizeof(uint8_t):
            return read_fixed_uint<uint8_t>();
        case sizeof(uint16_t):
            return read_fixed_uint<uint16_t>();
        case sizeof(uint32_t):
            return read_fixed_uint<uint32_t>();
        case sizeof(uint64_t):
            return read_fixed_uint<uint64_t>();
        default:
            break;
        }

        ensure(byte_count);

        uint64_t value = 0;
        const auto* in = buf_.data() + pos_;
        if (endian_ == endian::little) {
            for (size_t i = 0; i < byte_count; ++i) {
                value |= static_cast<uint64_t>(in[i]) << (i * 8);
            }
        }
        else {
            for (size_t i = 0; i < byte_count; ++i) {
                value = (value << 8) | in[i];
            }
        }

        pos_ += byte_count;
        return value;
    }

    void write_float(float v) {
        uint32_t u = 0;
        static_assert(sizeof(u) == sizeof(v), "unexpected float size");
        std::memcpy(&u, &v, sizeof(u));
        write_uint(u, sizeof(u));
    }

    void write_double(double v) {
        uint64_t u = 0;
        static_assert(sizeof(u) == sizeof(v), "unexpected double size");
        std::memcpy(&u, &v, sizeof(u));
        write_uint(u, sizeof(u));
    }

    float read_float() const {
        uint32_t u = static_cast<uint32_t>(read_uint(sizeof(uint32_t)));
        float f = 0.0f;
        std::memcpy(&f, &u, sizeof(f));
        return f;
    }

    double read_double() const {
        uint64_t u = read_uint(sizeof(uint64_t));
        double d = 0.0;
        std::memcpy(&d, &u, sizeof(d));
        return d;
    }

    void write_compact_uint(uint64_t value) {
        uint8_t encoded[10]{};
        size_t count = 0;

        do {
            uint8_t byte = static_cast<uint8_t>(value & 0x7Fu);
            value >>= 7;
            if (value != 0) {
                byte |= 0x80u;
            }
            encoded[count++] = byte;
        } while (value != 0);

        const size_t old_size = buf_.size();
        resize_for_bounded_append(count);
        std::memcpy(buf_.data() + old_size, encoded, count);
    }

    uint64_t read_compact_uint(size_t max_bits) const {
        uint64_t value = 0;
        size_t shift = 0;
        size_t cursor = pos_;

        for (size_t i = 0; i < 10; ++i) {
            if (cursor >= buf_.size()) {
                throw_insufficient_data(1);
            }
            const uint8_t byte = buf_[cursor++];
            const uint64_t payload = byte & 0x7Fu;

            if (shift == 63 && payload > 1) {
                throw byte_stream_error(byte_stream_errc::invalid_size,
                    "byte_stream: compact integer is too large");
            }

            value |= payload << shift;
            if ((byte & 0x80u) == 0) {
                if (i != 0 && payload == 0) {
                    throw byte_stream_error(byte_stream_errc::non_canonical_encoding,
                        "byte_stream: compact integer is not minimally encoded");
                }
                if (max_bits < 64 && (value >> max_bits) != 0) {
                    throw byte_stream_error(byte_stream_errc::invalid_size,
                        "byte_stream: compact integer exceeds target type");
                }
                pos_ = cursor;
                return value;
            }

            shift += 7;
        }

        throw byte_stream_error(byte_stream_errc::invalid_size,
            "byte_stream: unterminated compact integer");
    }

    template <typename T>
    void write_scalar_array(const T* data, size_t count) {
        if (count == 0) {
            return;
        }
        if (data == nullptr) {
            throw byte_stream_error(byte_stream_errc::null_pointer,
                "byte_stream: null scalar array pointer");
        }

        const size_t byte_count = checked_byte_count(count, sizeof(T));
        if constexpr (detail::can_raw_copy_scalar<T>::value) {
            if (endian_ == detail::host_endian()) {
                append_bytes(data, byte_count);
                return;
            }
        }

        const size_t old_size = buf_.size();
        buf_.resize(checked_new_buffer_size(byte_count));
        auto* out = buf_.data() + old_size;
        const bool swap_bytes = endian_ != detail::host_endian();

        for (size_t i = 0; i < count; ++i) {
            const uint64_t raw = detail::scalar_to_uint(data[i]);
            store_scalar_value(out + i * sizeof(T), raw, sizeof(T), swap_bytes);
        }
    }

    template <typename T>
    void read_scalar_array(T* data, size_t count) const {
        if (count == 0) {
            return;
        }
        if (data == nullptr) {
            throw byte_stream_error(byte_stream_errc::null_pointer,
                "byte_stream: null scalar array pointer");
        }

        const size_t byte_count = checked_byte_count(count, sizeof(T));
        if constexpr (detail::can_raw_copy_scalar<T>::value) {
            if (endian_ == detail::host_endian()) {
                ensure(byte_count);
                read_bytes_unchecked(data, byte_count);
                return;
            }
        }

        ensure(byte_count);
        const auto* in = buf_.data() + pos_;
        const bool swap_bytes = endian_ != detail::host_endian();
        for (size_t i = 0; i < count; ++i) {
            const uint64_t raw = load_scalar_value(in + i * sizeof(T), sizeof(T), swap_bytes);
            data[i] = detail::scalar_from_uint<T>(raw);
        }
        pos_ += byte_count;
    }

    template <typename T>
    static size_t checked_integer_size(uint32_t n) {
        const size_t byte_count = is_default_size(n) ? sizeof(T) : static_cast<size_t>(n);
        if (byte_count == 0 || byte_count > sizeof(T) || byte_count > sizeof(uint64_t)) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: invalid integer byte count");
        }
        return byte_count;
    }

    template <typename T>
    static void check_floating_size(uint32_t n) {
        if (!is_default_size(n) && n != sizeof(T)) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: invalid floating point byte count");
        }
    }

    static size_t checked_fixed_element_count(size_t extent, uint32_t n) {
        const size_t count = is_default_size(n) ? extent : static_cast<size_t>(n);
        if (count > extent) {
            throw byte_stream_error(byte_stream_errc::count_out_of_range,
                "byte_stream: element count exceeds fixed array size");
        }
        return count;
    }

    static size_t checked_byte_count(size_t count, size_t element_size) {
        if (element_size != 0 && count > (std::numeric_limits<size_t>::max)() / element_size) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: byte count overflow");
        }
        return count * element_size;
    }

    size_t checked_new_buffer_size(size_t additional_size) const {
        const size_t available_capacity = buf_.capacity() - buf_.size();
        if (additional_size <= available_capacity) {
            return buf_.size() + additional_size;
        }
        if (additional_size > buf_.max_size() - buf_.size()) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: buffer size overflow");
        }
        return buf_.size() + additional_size;
    }

    void resize_for_bounded_append(size_t additional_size) {
        // 调用方保证增量较小或受 uint32_t 限制；64 位平台上的合法 vector 长度不会因此回绕，
        // 32 位平台仍保留显式算术检查。
        if constexpr (sizeof(size_t) <= sizeof(uint32_t)) {
            if (additional_size > (std::numeric_limits<size_t>::max)() - buf_.size()) {
                throw byte_stream_error(byte_stream_errc::size_overflow,
                    "byte_stream: buffer size overflow");
            }
        }
        try {
            buf_.resize(buf_.size() + additional_size);
        }
        catch (const std::length_error&) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: buffer size exceeds vector maximum");
        }
    }
};

/**
 * @brief 默认的自定义类型 codec，通过 ADL 转发到用户定义的编解码函数。
 * @tparam T 用户类型。
 * @tparam Enable 扩展点的启用参数。
 *
 * 该实现使公共宏生成的函数以及手写的 to_byte_stream/from_byte_stream 函数均可工作。
 */
template <typename T, typename Enable>
struct byte_stream_codec {
    static void write(stream& stream, const T& value, uint32_t) {
        if constexpr (detail::has_adl_serializer<T>::value) {
            to_byte_stream(stream, value);
        }
        else {
            static_assert(detail::always_false<T>::value,
                "unsupported type: define to_byte_stream(stream&, const T&) or specialize byte_stream_codec<T>");
        }
    }

    static void read(const stream& stream, T& value, uint32_t) {
        if constexpr (detail::has_adl_deserializer<T>::value) {
            from_byte_stream(stream, value);
        }
        else {
            static_assert(detail::always_false<T>::value,
                "unsupported type: define from_byte_stream(const stream&, T&) or specialize byte_stream_codec<T>");
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<std::is_integral_v<T>>> {
    static void write(stream& stream, T value, uint32_t n) {
        using U = detail::unsigned_integral_t<T>;
        const size_t byte_count = stream::checked_integer_size<T>(n);
        stream.write_uint(static_cast<uint64_t>(static_cast<U>(value)), byte_count);
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        const size_t byte_count = stream::checked_integer_size<T>(n);
        uint64_t raw = detail::sign_extend<T>(stream.read_uint(byte_count), byte_count);

        if constexpr (std::is_same_v<T, bool>) {
            if (raw > 1) {
                stream.pos_ -= byte_count;
                throw byte_stream_error(byte_stream_errc::invalid_value,
                    "byte_stream: boolean wire value must be 0 or 1");
            }
            value = raw == 1;
        }
        else if constexpr (std::is_signed_v<T>) {
            using U = detail::unsigned_integral_t<T>;
            U u = static_cast<U>(raw);
            std::memcpy(&value, &u, sizeof(value));
        }
        else {
            value = static_cast<T>(raw);
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<std::is_enum_v<T>>> {
    static void write(stream& stream, T value, uint32_t n) {
        using U = std::underlying_type_t<T>;
        byte_stream_codec<U>::write(stream, static_cast<U>(value), n);
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        using U = std::underlying_type_t<T>;
        U raw{};
        byte_stream_codec<U>::read(stream, raw, n);
        value = static_cast<T>(raw);
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static void write(stream& stream, T value, uint32_t n) {
        stream::check_floating_size<T>(n);
        if constexpr (sizeof(T) == sizeof(float)) {
            stream.write_float(static_cast<float>(value));
        }
        else if constexpr (sizeof(T) == sizeof(double)) {
            stream.write_double(static_cast<double>(value));
        }
        else {
            static_assert(sizeof(T) == sizeof(float) || sizeof(T) == sizeof(double),
                "unsupported floating point size");
        }
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        stream::check_floating_size<T>(n);
        if constexpr (sizeof(T) == sizeof(float)) {
            value = static_cast<T>(stream.read_float());
        }
        else if constexpr (sizeof(T) == sizeof(double)) {
            value = static_cast<T>(stream.read_double());
        }
        else {
            static_assert(sizeof(T) == sizeof(float) || sizeof(T) == sizeof(double),
                "unsupported floating point size");
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<std::is_array_v<T>>> {
    static void write(stream& stream, const T& value, uint32_t n) {
        constexpr size_t extent = std::extent<T>::value;
        const size_t count = stream::checked_fixed_element_count(extent, n);
        using element_type = std::remove_cv_t<std::remove_extent_t<T>>;

        if constexpr (detail::is_byte_like<element_type>::value) {
            stream.append_bytes(value, count * sizeof(element_type));
            return;
        }
        else if constexpr (detail::is_fixed_width_scalar<element_type>::value) {
            stream.write_scalar_array(value, count);
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            stream.set(value[i]);
        }
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        constexpr size_t extent = std::extent<T>::value;
        const size_t count = stream::checked_fixed_element_count(extent, n);
        using element_type = std::remove_cv_t<std::remove_extent_t<T>>;

        if constexpr (detail::is_byte_like<element_type>::value) {
            stream.read_bytes(value, count * sizeof(element_type));
            return;
        }
        else if constexpr (detail::is_fixed_width_scalar<element_type>::value) {
            stream.read_scalar_array(value, count);
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            stream.get_to(value[i]);
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<detail::is_std_array<T>::value>> {
    static void write(stream& stream, const T& value, uint32_t n) {
        const size_t count = stream::checked_fixed_element_count(value.size(), n);
        using element_type = typename T::value_type;

        if constexpr (detail::is_byte_like<element_type>::value) {
            stream.append_bytes(value.data(), count * sizeof(element_type));
            return;
        }
        else if constexpr (detail::is_fixed_width_scalar<element_type>::value) {
            stream.write_scalar_array(value.data(), count);
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            stream.set(value[i]);
        }
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        const size_t count = stream::checked_fixed_element_count(value.size(), n);
        using element_type = typename T::value_type;

        if constexpr (detail::is_byte_like<element_type>::value) {
            stream.read_bytes(value.data(), count * sizeof(element_type));
            return;
        }
        else if constexpr (detail::is_fixed_width_scalar<element_type>::value) {
            stream.read_scalar_array(value.data(), count);
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            stream.get_to(value[i]);
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<detail::is_optional<T>::value>> {
    static void write(stream& stream, const T& value, uint32_t n) {
        stream.set(value.has_value());
        if (value.has_value()) {
            stream.set(*value, n);
        }
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        const bool has_value = stream.get<bool>();
        if (!has_value) {
            value.reset();
            return;
        }

        value.emplace();
        stream.get_to(*value, n);
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<
    detail::is_unique_ptr<T>::value || detail::is_shared_ptr<T>::value>> {
    static void write(stream& stream, const T& value, uint32_t n) {
        if (!value) {
            throw byte_stream_error(byte_stream_errc::null_pointer,
                "byte_stream: null smart pointer");
        }

        stream.set(*value, n);
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        if constexpr (detail::is_unique_ptr<T>::value) {
            using element_type = typename detail::is_unique_ptr<T>::element_type;
            auto ptr = std::make_unique<element_type>();
            stream.get_to(*ptr, n);
            value = std::move(ptr);
        }
        else {
            using element_type = typename detail::is_shared_ptr<T>::element_type;
            auto ptr = std::make_shared<element_type>();
            stream.get_to(*ptr, n);
            value = std::move(ptr);
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<detail::is_pair<T>::value>> {
    static void write(stream& stream, const T& value, uint32_t) {
        stream.set(value.first);
        stream.set(value.second);
    }

    static void read(const stream& stream, T& value, uint32_t) {
        stream.get_to(value.first);
        stream.get_to(value.second);
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<detail::is_tuple<T>::value>> {
    template <size_t... Indexes>
    static void write_tuple(stream& stream, const T& value, std::index_sequence<Indexes...>) {
        (stream.set(std::get<Indexes>(value)), ...);
    }

    static void write(stream& stream, const T& value, uint32_t) {
        write_tuple(stream, value, std::make_index_sequence<std::tuple_size<T>::value>{});
    }

    template <size_t... Indexes>
    static void read_tuple(const stream& stream, T& value, std::index_sequence<Indexes...>) {
        (stream.get_to(std::get<Indexes>(value)), ...);
    }

    static void read(const stream& stream, T& value, uint32_t) {
        read_tuple(stream, value, std::make_index_sequence<std::tuple_size<T>::value>{});
    }
};

template <typename Container, size_t MaxCount>
struct byte_stream_codec<length_prefixed<Container, MaxCount>> {
    using value_type = length_prefixed<Container, MaxCount>;

    static void write(stream& stream, const value_type& value, uint32_t) {
        if (value.value.size() > MaxCount) {
            throw byte_stream_error(byte_stream_errc::count_out_of_range,
                "byte_stream: length-prefixed value exceeds its declared maximum");
        }
        stream.set_with_size(value.value);
    }

    static void read(const stream& stream, value_type& value, uint32_t) {
        stream.get_to_with_size(value.value, MaxCount);
    }
};

template <>
struct byte_stream_codec<std::monostate> {
    static void write(stream&, const std::monostate&, uint32_t) noexcept {}
    static void read(const stream&, std::monostate&, uint32_t) noexcept {}
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<detail::is_variant<T>::value>> {
    static_assert(std::variant_size_v<T> <= (std::numeric_limits<uint32_t>::max)(),
        "byte_stream variants cannot have more than UINT32_MAX alternatives");
    static_assert(!detail::variant_has_unframed_dynamic_container<T>::value,
        "dynamic-container variant alternatives must use byte_stream::length_prefixed<Container, MaxCount>");

    static void write(stream& stream, const T& value, uint32_t) {
        if (value.valueless_by_exception()) {
            throw byte_stream_error(byte_stream_errc::invalid_value,
                "byte_stream: cannot serialize a valueless variant");
        }

        stream.set_compact(static_cast<uint32_t>(value.index()));
        std::visit([&stream](const auto& alternative) {
            using alternative_type = std::decay_t<decltype(alternative)>;
            if constexpr (!std::is_same_v<alternative_type, std::monostate>) {
                stream.set(alternative);
            }
        }, value);
    }

    static void read(const stream& stream, T& value, uint32_t) {
        const uint32_t index = stream.get_compact<uint32_t>();
        read_alternative(stream, value, index);
    }

private:
    template <size_t Index = 0>
    static void read_alternative(const stream& stream, T& value, uint32_t index) {
        if constexpr (Index == std::variant_size_v<T>) {
            throw byte_stream_error(byte_stream_errc::invalid_value,
                "byte_stream: variant alternative index is out of range");
        }
        else {
            if (index == Index) {
                using alternative_type = std::variant_alternative_t<Index, T>;
                static_assert(std::is_default_constructible_v<alternative_type>,
                    "byte_stream variant alternatives must be default constructible");

                if constexpr (std::is_same_v<alternative_type, std::monostate>) {
                    value.template emplace<Index>();
                }
                else {
                    alternative_type decoded{};
                    stream.get_to(decoded);
                    value.template emplace<Index>(std::move(decoded));
                }
                return;
            }
            read_alternative<Index + 1>(stream, value, index);
        }
    }
};

template <typename T>
struct byte_stream_codec<T, std::enable_if_t<
    detail::is_container<T>::value && !detail::is_std_array<T>::value>> {
    static void write(stream& stream, const T& value, uint32_t n) {
        static_assert(!detail::has_key_type<T>::value,
            "byte_stream does not implicitly serialize associative containers; define a custom codec");
        if constexpr (detail::is_contiguous_byte_container<T>::value) {
            const size_t count = stream::is_default_size(n) ? value.size() : static_cast<size_t>(n);
            if (value.size() < count) {
                throw byte_stream_error(byte_stream_errc::container_too_small,
                    "byte_stream: container has fewer elements than requested");
            }

            stream.append_bytes(value.data(), count * sizeof(typename T::value_type));
            return;
        }
        else if constexpr (detail::is_contiguous_scalar_container<T>::value) {
            const size_t count = stream::is_default_size(n) ? value.size() : static_cast<size_t>(n);
            if (value.size() < count) {
                throw byte_stream_error(byte_stream_errc::container_too_small,
                    "byte_stream: container has fewer elements than requested");
            }

            stream.write_scalar_array(value.data(), count);
            return;
        }

        if (stream::is_default_size(n)) {
            for (const auto& elem : value) {
                stream.set(elem);
            }
            return;
        }

        if (value.size() < n) {
            throw byte_stream_error(byte_stream_errc::container_too_small,
                "byte_stream: container has fewer elements than requested");
        }

        uint32_t written = 0;
        for (const auto& elem : value) {
            stream.set(elem);
            if (++written == n) {
                break;
            }
        }
    }

    static void read(const stream& stream, T& value, uint32_t n) {
        static_assert(!detail::has_key_type<T>::value,
            "byte_stream does not implicitly deserialize associative containers; define a custom codec");
        if (stream::is_default_size(n)) {
            throw byte_stream_error(byte_stream_errc::missing_element_count,
                "byte_stream: container read needs an element count");
        }

        if constexpr (detail::has_max_size<T>::value) {
            if (n > value.max_size()) {
                throw byte_stream_error(byte_stream_errc::count_out_of_range,
                    "byte_stream: requested element count exceeds container maximum");
            }
        }

        if constexpr (detail::is_contiguous_byte_container<T>::value && detail::has_resize<T>::value) {
            const size_t byte_count = static_cast<size_t>(n) * sizeof(typename T::value_type);
            stream.ensure(byte_count);
            value.resize(n);
            stream.read_bytes_unchecked(value.data(), byte_count);
            return;
        }
        else if constexpr (detail::is_contiguous_scalar_container<T>::value && detail::has_resize<T>::value) {
            const size_t byte_count = stream::checked_byte_count(
                static_cast<size_t>(n),
                sizeof(typename T::value_type));
            stream.ensure(byte_count);
            value.resize(n);
            stream.read_scalar_array(value.data(), n);
            return;
        }

        value.clear();
        if constexpr (detail::has_reserve<T>::value) {
            value.reserve(n);
        }

        for (uint32_t i = 0; i < n; ++i) {
            static_assert(std::is_default_constructible_v<typename T::value_type>,
                "byte_stream container elements must be default constructible");
            typename T::value_type elem{};
            stream.get_to(elem);
            if constexpr (detail::has_push_back<T>::value) {
                value.push_back(std::move(elem));
            }
            else {
                value.insert(value.end(), std::move(elem));
            }
        }
    }
};

} // namespace byte_stream

#define BYTE_STREAM_DETAIL_CAT(a, b) BYTE_STREAM_DETAIL_CAT_I(a, b)
#define BYTE_STREAM_DETAIL_CAT_I(a, b) a##b

#define BYTE_STREAM_DETAIL_NARG(...) \
    BYTE_STREAM_DETAIL_NARG_I(__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, \
        21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define BYTE_STREAM_DETAIL_NARG_I(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, \
    _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, \
    _29, _30, _31, _32, N, ...) N

#define BYTE_STREAM_DETAIL_FOR_EACH(action, ...) \
    BYTE_STREAM_DETAIL_CAT(BYTE_STREAM_DETAIL_FOR_EACH_, BYTE_STREAM_DETAIL_NARG(__VA_ARGS__))(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_1(action, x) action(x)
#define BYTE_STREAM_DETAIL_FOR_EACH_2(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_1(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_3(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_2(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_4(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_3(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_5(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_4(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_6(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_5(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_7(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_6(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_8(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_7(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_9(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_8(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_10(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_9(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_11(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_10(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_12(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_11(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_13(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_12(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_14(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_13(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_15(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_14(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_16(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_15(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_17(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_16(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_18(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_17(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_19(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_18(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_20(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_19(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_21(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_20(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_22(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_21(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_23(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_22(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_24(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_23(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_25(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_24(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_26(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_25(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_27(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_26(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_28(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_27(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_29(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_28(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_30(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_29(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_31(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_30(action, __VA_ARGS__)
#define BYTE_STREAM_DETAIL_FOR_EACH_32(action, x, ...) action(x) BYTE_STREAM_DETAIL_FOR_EACH_31(action, __VA_ARGS__)

#define BYTE_STREAM_DETAIL_FIELD_ARG_COUNT(...) BYTE_STREAM_DETAIL_FIELD_ARG_COUNT_I(__VA_ARGS__, 2, 1, 0)
#define BYTE_STREAM_DETAIL_FIELD_ARG_COUNT_I(_1, _2, N, ...) N

#define BYTE_STREAM_DETAIL_PROBE(...) ~, 1
#define BYTE_STREAM_DETAIL_SECOND(a, b, ...) b
#define BYTE_STREAM_DETAIL_IS_PROBE(...) BYTE_STREAM_DETAIL_SECOND(__VA_ARGS__, 0, 0)
#define BYTE_STREAM_DETAIL_IS_PAREN(x) BYTE_STREAM_DETAIL_IS_PROBE(BYTE_STREAM_DETAIL_IS_PAREN_PROBE x)
#define BYTE_STREAM_DETAIL_IS_PAREN_PROBE(...) BYTE_STREAM_DETAIL_PROBE(~)

#define BYTE_STREAM_DETAIL_IF_0(true_expr, false_expr) false_expr
#define BYTE_STREAM_DETAIL_IF_1(true_expr, false_expr) true_expr
#define BYTE_STREAM_DETAIL_IF(condition) BYTE_STREAM_DETAIL_CAT(BYTE_STREAM_DETAIL_IF_, condition)

#define BYTE_STREAM_DETAIL_WRITE_FIELD(field) \
    BYTE_STREAM_DETAIL_IF(BYTE_STREAM_DETAIL_IS_PAREN(field))( \
        BYTE_STREAM_DETAIL_WRITE_FIELD_PAREN field, \
        BYTE_STREAM_DETAIL_WRITE_FIELD_BARE(field))
#define BYTE_STREAM_DETAIL_WRITE_FIELD_BARE(member) stream.set(value.member);
#define BYTE_STREAM_DETAIL_WRITE_FIELD_PAREN(...) \
    BYTE_STREAM_DETAIL_CAT(BYTE_STREAM_DETAIL_WRITE_FIELD_, BYTE_STREAM_DETAIL_FIELD_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define BYTE_STREAM_DETAIL_WRITE_FIELD_1(member) stream.set(value.member);
#define BYTE_STREAM_DETAIL_WRITE_FIELD_2(member, n) stream.set(value.member, n);

#define BYTE_STREAM_DETAIL_READ_FIELD(field) \
    BYTE_STREAM_DETAIL_IF(BYTE_STREAM_DETAIL_IS_PAREN(field))( \
        BYTE_STREAM_DETAIL_READ_FIELD_PAREN field, \
        BYTE_STREAM_DETAIL_READ_FIELD_BARE(field))
#define BYTE_STREAM_DETAIL_READ_FIELD_BARE(member) stream.get_to(value.member);
#define BYTE_STREAM_DETAIL_READ_FIELD_PAREN(...) \
    BYTE_STREAM_DETAIL_CAT(BYTE_STREAM_DETAIL_READ_FIELD_, BYTE_STREAM_DETAIL_FIELD_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)
#define BYTE_STREAM_DETAIL_READ_FIELD_1(member) stream.get_to(value.member);
#define BYTE_STREAM_DETAIL_READ_FIELD_2(member, n) stream.get_to(value.member, n);

/**
 * @brief 为公开成员类型生成非侵入式序列化函数。
 * @param Type 要支持的类型名；宏应在该类型所在命名空间中调用。
 * @param ... 按线格式顺序排列的成员列表；member 使用默认大小，(member, n) 将 n
 * 传递给 set。
 */
#define BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, ...) \
    inline void to_byte_stream(::byte_stream::stream& stream, const Type& value) { \
        BYTE_STREAM_DETAIL_FOR_EACH(BYTE_STREAM_DETAIL_WRITE_FIELD, __VA_ARGS__) \
    }

/**
 * @brief 为公开成员类型生成非侵入式反序列化函数。
 * @param Type 要支持的类型名；宏应在该类型所在命名空间中调用。
 * @param ... 按线格式顺序排列的成员列表；member 使用默认大小，(member, n) 将 n
 * 传递给 get_to。
 */
#define BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_DESERIALIZE(Type, ...) \
    inline void from_byte_stream(const ::byte_stream::stream& stream, Type& value) { \
        BYTE_STREAM_DETAIL_FOR_EACH(BYTE_STREAM_DETAIL_READ_FIELD, __VA_ARGS__) \
    }

/**
 * @brief 为公开成员类型同时生成非侵入式序列化和反序列化函数。
 * @param Type 要支持的类型名；宏应在该类型所在命名空间中调用。
 * @param ... 按线格式顺序排列的成员列表，支持 member 与 (member, n) 两种写法。
 */
#define BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(Type, ...) \
    BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_SERIALIZE(Type, __VA_ARGS__) \
    BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE_ONLY_DESERIALIZE(Type, __VA_ARGS__)

/**
 * @brief 在类型内部生成友元序列化函数，可访问私有成员。
 * @param Type 当前类型名。
 * @param ... 按线格式顺序排列的成员列表；member 使用默认大小，(member, n) 指定大小。
 */
#define BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, ...) \
    friend void to_byte_stream(::byte_stream::stream& stream, const Type& value) { \
        BYTE_STREAM_DETAIL_FOR_EACH(BYTE_STREAM_DETAIL_WRITE_FIELD, __VA_ARGS__) \
    }

/**
 * @brief 在类型内部生成友元反序列化函数，可访问私有成员。
 * @param Type 当前类型名。
 * @param ... 按线格式顺序排列的成员列表；member 使用默认大小，(member, n) 指定大小。
 */
#define BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_DESERIALIZE(Type, ...) \
    friend void from_byte_stream(const ::byte_stream::stream& stream, Type& value) { \
        BYTE_STREAM_DETAIL_FOR_EACH(BYTE_STREAM_DETAIL_READ_FIELD, __VA_ARGS__) \
    }

/**
 * @brief 在类型内部同时生成友元序列化和反序列化函数，可访问私有成员。
 * @param Type 当前类型名。
 * @param ... 按线格式顺序排列的成员列表，支持 member 与 (member, n) 两种写法。
 */
#define BYTE_STREAM_DEFINE_TYPE_INTRUSIVE(Type, ...) \
    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Type, __VA_ARGS__) \
    BYTE_STREAM_DEFINE_TYPE_INTRUSIVE_ONLY_DESERIALIZE(Type, __VA_ARGS__)

#endif // BYTE_STREAM_BYTE_STREAM_HPP
