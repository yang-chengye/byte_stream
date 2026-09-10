/**
 *   ____        _       ____  _                            
 *  | __ ) _   _| |_ ___/ ___|| |_ _ __ ___  __ _ _ __ ___  
 *  |  _ \| | | | __/ _ \___ \| __| '__/ _ \/ _` | '_ ` _ \ 
 *  | |_) | |_| | ||  __/___) | |_| | |  __/ (_| | | | | | |
 *  |____/ \__, |\__\___|____/ \__|_|  \___|\__,_|_| |_| |_|
 *         |___/                                            
 * https://github.com/yang-chengye/byte_stream
 * Version: 0.1.0
 * License: MIT
 */

#ifndef BYTE_STREAM_BYTE_STREAM_TYPES_HPP
#define BYTE_STREAM_BYTE_STREAM_TYPES_HPP

namespace byte_stream {

/**
 * @brief 指定整数、枚举和浮点数在字节流中的存储字节序。
 *
 * 字节序只影响多字节标量的线格式，不影响单字节值及原始字节块。
 * 各读写类型的默认值均为小端序。
 */
enum class endian {
    /** @brief 小端序：低有效字节在前。 */
    little,
    /** @brief 大端序：高有效字节在前。 */
    big
};

/**
 * @brief 表示字节流读写操作的可机器判断错误类别。
 *
 * 无分配读写接口直接返回该枚举；拥有缓冲区的 stream 接口会将非 ok 状态转换为
 * byte_stream_error。调用者不应依赖枚举值对应的整数表示。
 */
enum class byte_stream_errc {
    /** @brief 操作成功。 */
    ok,
    /** @brief 请求的字段宽度、编码长度或帧范围无效。 */
    invalid_size,
    /** @brief 输入缓冲区中的剩余数据不足。 */
    insufficient_data,
    /** @brief 输出缓冲区中的剩余空间不足。 */
    insufficient_space,
    /** @brief 非零长度操作收到了空指针。 */
    null_pointer,
    /** @brief 元素数量超出接口或调用者设定的上限。 */
    count_out_of_range,
    /** @brief 动态容器解码时未提供元素数量。 */
    missing_element_count,
    /** @brief 固定大小目标容器无法容纳请求的元素数量。 */
    container_too_small,
    /** @brief 完整解析一个对象后仍有未消费字节。 */
    trailing_data,
    /** @brief 输入值不符合目标类型的有效取值范围。 */
    invalid_value,
    /** @brief 变长整数使用了可由更短序列表示的非规范编码。 */
    non_canonical_encoding,
    /** @brief 长度或容量计算发生整数溢出。 */
    size_overflow
};

/**
 * @brief 获取错误类别对应的稳定英文名称。
 * @param code 要转换的错误类别。
 * @return 指向静态字符串的指针；返回值无需释放且始终有效。
 */
[[nodiscard]] constexpr const char* to_string(byte_stream_errc code) noexcept {
    switch (code) {
    case byte_stream_errc::ok:
        return "ok";
    case byte_stream_errc::invalid_size:
        return "invalid_size";
    case byte_stream_errc::insufficient_data:
        return "insufficient_data";
    case byte_stream_errc::insufficient_space:
        return "insufficient_space";
    case byte_stream_errc::null_pointer:
        return "null_pointer";
    case byte_stream_errc::count_out_of_range:
        return "count_out_of_range";
    case byte_stream_errc::missing_element_count:
        return "missing_element_count";
    case byte_stream_errc::container_too_small:
        return "container_too_small";
    case byte_stream_errc::trailing_data:
        return "trailing_data";
    case byte_stream_errc::invalid_value:
        return "invalid_value";
    case byte_stream_errc::non_canonical_encoding:
        return "non_canonical_encoding";
    case byte_stream_errc::size_overflow:
        return "size_overflow";
    }
    return "unknown";
}

} // namespace byte_stream

#endif // BYTE_STREAM_BYTE_STREAM_TYPES_HPP
