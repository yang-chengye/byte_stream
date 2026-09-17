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

#ifndef BYTE_STREAM_BYTE_STREAM_UTILS_HPP
#define BYTE_STREAM_BYTE_STREAM_UTILS_HPP

#include "byte_stream.hpp"

#include <cstddef>
#include <stdint.h>
#include <limits>
#include <string>
#include <vector>

namespace byte_stream {

/**
 * @brief 提供字节转义、反转义及帧载荷转换工具。
 *
 * 该类型只包含静态函数，不能实例化。转换结果使用新容器返回，不会修改输入数据；
 * 字符串接口按原始字节处理，不执行字符集转换。
 */
class byte_stream_utils {
public:
    byte_stream_utils() = delete;

    /**
     * @brief 描述一个原始字节与其转义序列之间的映射。
     */
    struct byte_escape_rule {
        /** @brief 需要转义的原始字节。 */
        uint8_t value;
        /** @brief 写入输出的非空转义序列。 */
        std::vector<uint8_t> escaped_value;
    };

    /**
     * @brief 按给定规则转义字节数组。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param data 待转义的原始字节。
     * @param rules 转义规则；同一原始字节存在重复规则时采用首个规则。
     * @return 转义后的新字节数组。
     * @throws byte_stream_error 规则包含空转义序列或结果长度计算溢出时抛出。
     */
    template <typename RuleRange>
    static std::vector<uint8_t> escape_bytes(
        const std::vector<uint8_t>& data,
        const RuleRange& rules) {
        size_t escaped_size = 0;
        for (const auto byte : data) {
            if (const auto* rule = find_escape_rule(byte, rules)) {
                check_escape_rule(*rule);
                escaped_size = checked_add(escaped_size, rule->escaped_value.size());
            }
            else {
                escaped_size = checked_add(escaped_size, 1);
            }
        }

        std::vector<uint8_t> escaped;
        escaped.reserve(escaped_size);
        for (const auto byte : data) {
            if (const auto* rule = find_escape_rule(byte, rules)) {
                escaped.insert(escaped.end(), rule->escaped_value.begin(), rule->escaped_value.end());
            }
            else {
                escaped.push_back(byte);
            }
        }

        return escaped;
    }

    /**
     * @brief 使用与 escape_bytes 相同的规则反转义字节数组。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param data 待反转义的字节。
     * @param rules 与编码端一致的转义规则。
     * @return 恢复后的新字节数组。
     * @throws byte_stream_error 规则无效，或输入以规则前缀开头但不能匹配完整序列时抛出。
     */
    template <typename RuleRange>
    static std::vector<uint8_t> unescape_bytes(
        const std::vector<uint8_t>& data,
        const RuleRange& rules) {
        std::vector<uint8_t> unescaped;
        unescaped.reserve(data.size());

        for (size_t i = 0; i < data.size();) {
            const byte_escape_rule* matched_rule = nullptr;
            size_t matched_size = 0;
            bool starts_escape_sequence = false;

            for (const auto& rule : rules) {
                check_escape_rule(rule);
                if (rule.escaped_value.front() != data[i]) {
                    continue;
                }

                // 优先匹配最长序列，使存在公共前缀的规则仍具有确定性。
                starts_escape_sequence = true;
                if (rule.escaped_value.size() > matched_size && matches_at(data, i, rule.escaped_value)) {
                    matched_rule = &rule;
                    matched_size = rule.escaped_value.size();
                }
            }

            if (matched_rule != nullptr) {
                unescaped.push_back(matched_rule->value);
                i += matched_size;
                continue;
            }

            if (starts_escape_sequence) {
                throw byte_stream_error(byte_stream_errc::invalid_size,
                    "byte_stream: invalid escape sequence in byte stream");
            }

            unescaped.push_back(data[i]);
            ++i;
        }

        return unescaped;
    }

    /**
     * @brief 仅转义完整帧的载荷部分，并原样保留帧头和帧尾。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param frame 包含帧头、载荷和帧尾的完整字节数组。
     * @param header_size 不参与转义的帧头字节数。
     * @param tail_size 不参与转义的帧尾字节数。
     * @param rules 转义规则。
     * @return 载荷已转义的新帧。
     * @throws byte_stream_error 帧范围、规则或结果长度无效时抛出。
     */
    template <typename RuleRange>
    static std::vector<uint8_t> escape_frame_payload(
        const std::vector<uint8_t>& frame,
        size_t header_size,
        size_t tail_size,
        const RuleRange& rules) {
        return transform_frame_payload(frame, header_size, tail_size,
            [&](const std::vector<uint8_t>& payload) {
                return escape_bytes(payload, rules);
            });
    }

    /**
     * @brief 仅反转义完整帧的载荷部分，并原样保留帧头和帧尾。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param frame 包含帧头、已转义载荷和帧尾的完整字节数组。
     * @param header_size 不参与反转义的帧头字节数。
     * @param tail_size 不参与反转义的帧尾字节数。
     * @param rules 与编码端一致的转义规则。
     * @return 载荷已恢复的新帧。
     * @throws byte_stream_error 帧范围、规则或转义序列无效时抛出。
     */
    template <typename RuleRange>
    static std::vector<uint8_t> unescape_frame_payload(
        const std::vector<uint8_t>& frame,
        size_t header_size,
        size_t tail_size,
        const RuleRange& rules) {
        return transform_frame_payload(frame, header_size, tail_size,
            [&](const std::vector<uint8_t>& payload) {
                return unescape_bytes(payload, rules);
            });
    }

    /**
     * @brief 反转义帧载荷，并用恢复后的完整逻辑帧构造 stream。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param frame 包含帧头、已转义载荷和帧尾的完整字节数组。
     * @param header_size 不参与反转义的帧头字节数。
     * @param tail_size 不参与反转义的帧尾字节数。
     * @param rules 与编码端一致的转义规则。
     * @return 拥有恢复后帧字节的 stream。
     * @throws byte_stream_error 帧范围、规则或转义序列无效时抛出。
     */
    template <typename RuleRange>
    static stream make_unescaped_frame_stream(
        const std::vector<uint8_t>& frame,
        size_t header_size,
        size_t tail_size,
        const RuleRange& rules) {
        return stream(unescape_frame_payload(frame, header_size, tail_size, rules));
    }

    /**
     * @brief 按原始字节转义字符串，不进行字符编码转换。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param data 待转义字符串，其内容可包含空字节。
     * @param rules 转义规则。
     * @return 保存转义后原始字节的新字符串。
     * @throws byte_stream_error 规则无效或结果长度计算溢出时抛出。
     */
    template <typename RuleRange>
    static std::string escape_string(
        const std::string& data,
        const RuleRange& rules) {
        return bytes_to_string(escape_bytes(string_to_bytes(data), rules));
    }

    /**
     * @brief 按原始字节反转义字符串，不进行字符编码转换。
     * @tparam RuleRange 可迭代且元素类型兼容 byte_escape_rule 的规则集合类型。
     * @param data 待反转义字符串，其内容可包含空字节。
     * @param rules 与编码端一致的转义规则。
     * @return 保存恢复后原始字节的新字符串。
     * @throws byte_stream_error 规则或转义序列无效时抛出。
     */
    template <typename RuleRange>
    static std::string unescape_string(
        const std::string& data,
        const RuleRange& rules) {
        return bytes_to_string(unescape_bytes(string_to_bytes(data), rules));
    }

private:
    /**
     * @brief 查找首个适用于指定原始字节的转义规则。
     */
    template <typename RuleRange>
    static const byte_escape_rule* find_escape_rule(uint8_t value, const RuleRange& rules) {
        for (const auto& item : rules) {
            if (item.value == value) {
                return &item;
            }
        }
        return nullptr;
    }

    /**
     * @brief 检查转义规则是否包含非空替换序列。
     */
    static void check_escape_rule(const byte_escape_rule& rule) {
        if (rule.escaped_value.empty()) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: empty escape replacement sequence");
        }
    }

    /**
     * @brief 判断指定位置是否完整匹配预期字节序列。
     */
    static bool matches_at(const std::vector<uint8_t>& data, size_t position, const std::vector<uint8_t>& expected) {
        if (position > data.size() || expected.size() > data.size() - position) {
            return false;
        }

        for (size_t i = 0; i < expected.size(); ++i) {
            if (data[position + i] != expected[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief 检查帧长度是否足以容纳指定的帧头和帧尾。
     */
    static void check_frame_payload_range(size_t frame_size, size_t header_size, size_t tail_size) {
        if (header_size > frame_size || tail_size > frame_size - header_size) {
            throw byte_stream_error(byte_stream_errc::invalid_size,
                "byte_stream: frame is smaller than header and tail");
        }
    }

    /**
     * @brief 对帧载荷应用转换，同时原样保留帧头和帧尾。
     */
    template <typename Transform>
    static std::vector<uint8_t> transform_frame_payload(
        const std::vector<uint8_t>& frame,
        size_t header_size,
        size_t tail_size,
        Transform transform) {
        check_frame_payload_range(frame.size(), header_size, tail_size);

        const auto payload_begin = frame.begin() + static_cast<std::ptrdiff_t>(header_size);
        const auto payload_end = frame.end() - static_cast<std::ptrdiff_t>(tail_size);
        const std::vector<uint8_t> payload(payload_begin, payload_end);
        const auto transformed_payload = transform(payload);

        const size_t result_size = checked_add(
            checked_add(header_size, transformed_payload.size()), tail_size);
        std::vector<uint8_t> result;
        result.reserve(result_size);
        result.insert(result.end(), frame.begin(), payload_begin);
        result.insert(result.end(), transformed_payload.begin(), transformed_payload.end());
        result.insert(result.end(), payload_end, frame.end());
        return result;
    }

    /**
     * @brief 将字符串原始字节复制到字节数组，不改变字符编码。
     */
    static std::vector<uint8_t> string_to_bytes(const std::string& data) {
        std::vector<uint8_t> bytes;
        bytes.reserve(data.size());
        for (const unsigned char byte : data) {
            bytes.push_back(static_cast<uint8_t>(byte));
        }
        return bytes;
    }

    /**
     * @brief 将字节数组复制到字符串，不改变字符编码。
     */
    static std::string bytes_to_string(const std::vector<uint8_t>& data) {
        std::string text;
        text.reserve(data.size());
        for (const auto byte : data) {
            text.push_back(static_cast<char>(byte));
        }
        return text;
    }

    /**
     * @brief 执行带溢出检查的长度加法。
     */
    static size_t checked_add(size_t lhs, size_t rhs) {
        if (rhs > (std::numeric_limits<size_t>::max)() - lhs) {
            throw byte_stream_error(byte_stream_errc::size_overflow,
                "byte_stream: utility output size overflow");
        }
        return lhs + rhs;
    }
};

} // namespace byte_stream

#endif // BYTE_STREAM_BYTE_STREAM_UTILS_HPP
