# byte_stream

**简体中文** | [English](README.en.md)

`byte_stream` 是一个仅头文件的 C++17 二进制协议编解码库。它按字段顺序写入和读取整数、枚举、浮点数、容器和自定义结构，支持大小端切换，也支持 3/5/6/7 字节这类非标准整数宽度字段。

## 特性

- 仅头文件：只需要包含 `include/byte_stream/byte_stream.hpp`
- C++17：无第三方运行时依赖
- 提供零拷贝 `byte_reader` 和无分配固定容量 `byte_writer`，支持 `-fno-exceptions`
- 支持 little-endian / big-endian
- 支持整数、枚举、`std::byte`、`float`、`double`、`bool`
- 支持非标准整数宽度，例如占 3 字节的 24 位设备标识
- 支持顺序容器、`std::array`、C 数组的批量读写
- 支持 `std::optional`、智能指针、`std::pair`、`std::tuple`、`std::variant`
- 支持通过 ADL、宏或 `byte_stream_codec<T>` 扩展自定义协议结构
- 支持 compact varint/zigzag 小整数编码，以及紧凑长度前缀容器
- 提供独立且规则可配置的数据转义/反转义工具
- 提供 CMake interface target、安装导出、示例和 GTest 测试
- 持续集成覆盖 Linux、macOS、Windows

## 流状态与对象赋值

```cpp
byte_stream::stream bs(byte_stream::endian::big); // 空流，位置为 0
bs = uint16_t{0x1234};                         // 替换：12 34
bs.set(uint16_t{0x5678});                      // 追加：12 34 56 78
const auto first = bs.get<uint16_t>();         // 0x1234
uint16_t second{};
bs.get_to(second);                            // 0x5678
bs.set_endian(byte_stream::endian::little);    // 影响后续读写
```

默认构造使用小端。对象赋值保留当前字节序，替换全部数据、读取位置归零，并复用已有容量；
编码规则及默认宽度与 `set(obj)` 相同。失败时原数据已清除，可能留下部分新数据。
输入对象不得引用目标流自身的 buffer 或其中元素；自定义序列化器必须只追加数据，
不改变字节序或读取位置。

流之间的复制、移动转移完整状态，包括字节数据、读取位置和字节序，不执行对象序列化。
`set_endian()` 不转换已有字节，也不改变读取位置；读取混合字节序字段时需要显式切换。
完整契约见 [DESIGN.md](DESIGN.md)。

## 快速开始

```cpp
#include "byte_stream/byte_stream.hpp"

#include <stdint.h>

int main() {
    byte_stream::stream stream;
    // 默认 wire endian 是 little；也可以显式切换为 big。

    stream.set<uint8_t>(0x01);
    stream.set<uint16_t>(0x1234);
    stream.set<uint32_t>(0x00A1B2C3, 3); // 只写 3 字节

    const auto id = stream.get<uint8_t>();
    const auto seq = stream.get<uint16_t>();
    const auto area = stream.get<uint32_t>(3);

    (void)id;
    (void)seq;
    (void)area;
}
```

默认 wire endian 是 `byte_stream::endian::little`，因此默认编码结果不随主机字节序变化。
公开 API 只提供 `little` 和 `big` 两种 wire endian；协议需要 big-endian 时应显式设置：

```cpp
byte_stream::stream stream;
stream.set_endian(byte_stream::endian::big);
```

完整的标量、容器、compact 编码规则及兼容性约定见 [Wire Format 说明](WIRE_FORMAT.md)。
buffer 所有权、分配行为、原子 API、自定义 codec 和线程约束见 [设计与 API 契约](DESIGN.md)。

查看编码后的字节：

```cpp
const auto& bytes = stream.buffer();
const auto hex = stream.to_hex(true); // "01 34 12 c3 b2 a1"

// 只读下标访问和完整 buffer 范围遍历均不改变读取位置
const auto first = stream[0];
for (uint8_t byte : stream) {
    (void)byte;
}
```

对嵌入式、网络收包和实时热路径，可直接在调用方存储上读写，不复制输入、不扩容、
不抛异常：

```cpp
#include "byte_stream/byte_io.hpp"

#include <array>

std::array<uint8_t, 32> storage{};
byte_stream::byte_writer writer(storage.data(), storage.size());
if (writer.write<uint16_t>(0x1234) != byte_stream::byte_stream_errc::ok ||
    writer.write<uint32_t>(0x00A1B2C3, 3) != byte_stream::byte_stream_errc::ok) {
    // 固定 buffer 空间不足或参数错误
}

byte_stream::byte_reader reader(writer.view());
uint16_t sequence = 0;
uint32_t device_id = 0;
if (reader.read(sequence) != byte_stream::byte_stream_errc::ok ||
    reader.read(device_id, 3) != byte_stream::byte_stream_errc::ok) {
    // 输入截断、非法值或参数错误
}
```

`byte_view`、`byte_reader` 和 `byte_writer` 都不拥有内存；调用方必须保证底层存储在它们
及其返回 view 的使用期间有效。失败的基础读写不会推进游标，也不会部分修改输出。

## 使用 CMake

作为子目录使用：

```cmake
add_subdirectory(byte_stream)
target_link_libraries(your_target PRIVATE byte_stream::byte_stream)
```

## 构建、测试和示例

```bash
cmake -S . -B build/dev -DBYTE_STREAM_BUILD_TESTS=ON -DBYTE_STREAM_BUILD_EXAMPLES=ON
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

运行示例：

```bash
./build/dev/examples/byte_stream_basic
./build/dev/examples/byte_stream_custom_protocol
./build/dev/examples/byte_stream_macro_generated
./build/dev/examples/byte_stream_fixed_buffer
```

测试使用仓库内 `3rdparty/googletest` 固定版本的 GTest，不依赖系统安装，也不会在配置阶段访问网络。GTest 只用于项目测试，不会成为 `byte_stream` 的运行时依赖。

## 跨平台支持

`byte_stream` 使用标准 C++17 和 CMake interface target，不依赖平台 API。CI 会在以下平台构建和测试：

| 平台 | 编译器/工具链 |
| --- | --- |
| Linux | 默认 GCC、GCC 9 兼容任务、Clang ASan/UBSan |
| macOS | AppleClang |
| Windows | MSVC / Visual Studio 生成器 |
| Embedded Linux | ARMv7 hard-float 与 AArch64 交叉编译，`-fno-exceptions -fno-rtti` |

手动运行 CMake 命令时最低支持 CMake 3.16；仓库中的 CMake Presets 使用 schema 3，因此需要 CMake 3.21 或更高版本。

本地推荐使用 CMake Presets：

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

Windows 使用 Visual Studio 或其他多配置生成器时，preset 已经指定了 `Debug` / `Release` configuration；如果手动运行命令，构建使用 `cmake --build ... --config Release`，测试使用 `ctest ... -C Release`，也可以相应改为 `Debug`。

`strict` preset 对项目目标启用 `-Wall -Wextra -Wpedantic -Werror`（MSVC 使用 `/W4 /WX`）。
`sanitizers` preset 在 GCC/Clang 下启用 ASan 和 UBSan。主要 CMake 开关如下：

| 开关 | 默认值 | 用途 |
| --- | --- | --- |
| `BYTE_STREAM_BUILD_TESTS` | 顶层项目时 ON | 构建单元、属性和头文件测试 |
| `BYTE_STREAM_BUILD_EXAMPLES` | 顶层项目时 ON | 构建示例 |
| `BYTE_STREAM_BUILD_BENCHMARKS` | 顶层项目时 ON | 构建 Release benchmark |
| `BYTE_STREAM_ENABLE_WARNINGS` | 顶层项目时 ON | 对项目开发目标启用严格告警 |
| `BYTE_STREAM_WARNINGS_AS_ERRORS` | OFF | 将项目告警视为错误，不污染消费者目标 |
| `BYTE_STREAM_ENABLE_SANITIZERS` | OFF | 启用 ASan/UBSan |
| `BYTE_STREAM_ENABLE_COVERAGE` | OFF | 为 GCC/Clang 开启源码覆盖率插桩 |

## 性能测试

项目内置两个无第三方依赖的 benchmark 可执行文件：

- `byte_stream_performance`：核心基线 benchmark，保留稳定的同名指标，便于修改前后对比大 payload、整数记录、compact 编码和自定义协议对象。
- `byte_stream_coverage_benchmark`：覆盖型 benchmark，补充小/中 payload、浮点数、常见标准容器、大端路径、嵌套结构、重复 parse、空容器、受限长度读取和截断输入拒绝等场景。

```bash
cmake --preset release
cmake --build --preset release
./build/release/benchmarks/byte_stream_performance
./build/release/benchmarks/byte_stream_coverage_benchmark
```

`byte_stream_performance` 会先预热，再采集 7 个样本并报告中位数和范围；可复用 buffer 的分配不计入核心编解码耗时。修改热路径前后可使用 CSV 和对比脚本检查同名指标：

```bash
./build/release/benchmarks/byte_stream_performance --csv > baseline.csv
# 修改并重新构建
./build/release/benchmarks/byte_stream_performance --csv > current.csv
python3 benchmarks/compare.py baseline.csv current.csv --max-regression-percent 5
```

基线和当前结果必须在同一台空闲机器、相同编译器和相同构建参数下采集。共享 CI runner 适合做 benchmark smoke test，但不适合作为严格性能门禁。完整方法见 [benchmarks/README.md](benchmarks/README.md)。

在 Windows/MSVC 多配置生成器下，benchmark 通常位于：

```powershell
.\build\release\benchmarks\Release\byte_stream_performance.exe
.\build\release\benchmarks\Release\byte_stream_coverage_benchmark.exe
```

当前实现针对常见热路径做了优化：

- `std::vector<uint8_t>`、`std::string`、`std::array<uint8_t, N>`、`uint8_t[N]`、`std::byte` 等连续 1 字节数据走批量 `memcpy` 路径
- `std::vector<uint16_t>`、`std::vector<uint32_t>`、`std::array<float, N>` 等连续数值容器会一次扩展缓冲区并批量编解码
- 16/32/64 位整数和连续标量容器在需要换端时优先使用编译器 byteswap 内置函数
- 整数写入会一次扩展缓冲区后填充字节，减少逐字节 `push_back` 的扩容检查
- 读取连续字节容器时会一次检查长度并批量复制
- 对小整数或长度字段，可以用 `set_compact` / `get_compact` 减少空间占用
- 对复杂协议结构，建议提前调用 `reserve()` 估算容量，避免反复扩容

大文件场景下，`byte_stream::stream` 是内存缓冲型工具：适合编码/解析已在内存中的协议帧、文件块或网络包。如果要处理超大文件，建议按协议边界分块读入，再对每个块使用 `byte_stream::stream`，避免一次性把整个文件和解码结果都保留在内存里。

## 数据转义工具

`byte_stream_utils.hpp` 提供独立的数据转义/反转义工具，不放在核心 `byte_stream.hpp` 中。调用方传入规则表即可适配不同协议。以如下规则为例：`0xC0` 编码为 `0xDB 0xDC`，`0xDB` 编码为 `0xDB 0xDD`。

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

规则里的 `escaped_value` 是完整替换序列，可以是 1 个或多个字节。反转义时如果遇到未完成或未知的转义序列，会抛出 `byte_stream::byte_stream_error`。

如果协议帧的头尾不参与转义，可以只处理中间 payload，再把结果交给 `byte_stream::stream`：

```cpp
std::vector<uint8_t> wire_frame{/* 帧头、已转义 payload、帧尾 */};

auto logical_frame = byte_stream::byte_stream_utils::unescape_frame_payload(
    wire_frame,
    1, // 帧头字节数
    1, // 帧尾字节数
    rules);

byte_stream::stream stream(logical_frame);

// 或者一步构造 byte_stream::stream
auto stream2 = byte_stream::byte_stream_utils::make_unescaped_frame_stream(wire_frame, 1, 1, rules);
```

## 容器

写容器时默认写入全部元素，也可以传入元素数量只写前 N 个。读容器时必须传入元素数量，因为二进制流本身不知道容器边界。

```cpp
std::vector<uint16_t> values{0x1111, 0x2222, 0x3333};

byte_stream::stream stream;
stream.set(values, 2);

std::vector<uint16_t> decoded;
stream.get_to(decoded, 2);
```

如果协议字段希望自带长度，可以使用 compact 长度前缀。读取时必须给出最大元素数量，避免恶意数据触发无边界分配：

```cpp
std::vector<uint16_t> values{0x1111, 0x2222, 0x3333};

byte_stream::stream stream;
stream.set_with_size(values); // 紧凑长度前缀与 payload

std::vector<uint16_t> decoded;
stream.get_to_with_size(decoded, 1024);
```

固定长度数组可以直接读写。`std::array` 和 C 数组默认读写完整长度，也可以传入元素数量读写前 N 个元素。

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

## 类型支持

| 类型 | 编码方式 |
| --- | --- |
| `int8_t` / `uint8_t` / `int16_t` / `uint16_t` / `int32_t` / `uint32_t` / `int64_t` / `uint64_t` | 按当前大小端写入，可用 `n` 指定 1 到类型宽度内的字节数 |
| `bool` | 1 字节，仅接受 `00` / `01`，其他值解析失败 |
| enum / `std::byte` | 按底层整数类型写入 |
| `float` / `double` | 按 IEEE 字节表示写入，受当前大小端影响 |
| `std::vector` / `std::list` / `std::string` 等顺序容器 | 顺序写入元素；读取时需要传入元素数量 |
| `std::array<T, N>` / `T[N]` | 固定长度数组；默认读写 N 个元素 |
| `std::optional<T>` | 先写 1 字节存在标记，再在存在时写入 `T` |
| `std::unique_ptr<T>` / `std::shared_ptr<T>` | 读写指向的 `T`；写入空指针会抛出异常，不自动编码 null 标记 |
| `std::pair<A, B>` / `std::tuple<Ts...>` | 按元素顺序依次读写 |
| `std::variant<Ts...>` | compact `uint32_t` 备选项索引，随后按该类型的普通规则写活动值；动态容器必须显式包装为 `length_prefixed<Container, MaxCount>`；反序列化要求备选类型可默认构造 |
| `length_prefixed<Container, MaxCount>` | compact 元素数量后跟容器内容；编码和解码都强制 `MaxCount`，适合 variant、tuple 等复合值中的自描述边界 |
| 自定义结构 | 通过同命名空间的 `to_byte_stream` / `from_byte_stream` 扩展，也可以用宏生成；高级场景可特化 `byte_stream::byte_stream_codec<T>` |

## 紧凑整数编码

`set_compact` / `get_compact` 提供显式的小整数压缩编码。无符号整数使用 7-bit varint；有符号整数先做 zigzag，因此 `-1`、`-64` 这类小负数也只占 1 字节。解码器只接受最短规范编码，拒绝 overlong 表示。它不会改变普通 `set` / `get` 的固定宽度协议语义，适合长度、计数、枚举值、小范围状态码等字段。

```cpp
byte_stream::stream stream;
stream.set_compact<uint32_t>(300); // ac 02
stream.set_compact<int32_t>(-1);   // 01

auto a = stream.get_compact<uint32_t>();
auto b = stream.get_compact<int32_t>();
```

## 自定义协议结构

对自定义结构，在同一命名空间里提供 `to_byte_stream` 和 `from_byte_stream` 函数即可，`byte_stream::stream` 会通过 ADL 自动调用。`to_byte_stream` 的第二个参数应为 `const T&`；`from_byte_stream` 的第一个参数应为 `const byte_stream::stream&`。

```cpp
namespace protocol {

struct packet {
    uint8_t id;
    uint32_t device_id; // 24 位设备标识，协议里占 3 字节
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

如果结构体字段都按默认规则顺序读写，可以用宏生成这两个函数。非侵入式宏放在类型所在命名空间内，适合 public 成员：

```cpp
namespace protocol {

struct header {
    uint8_t version;
    uint16_t sequence;
    uint32_t device_id; // 24 位设备标识，协议里占 3 字节
    std::array<uint8_t, 4> magic;
};

BYTE_STREAM_DEFINE_TYPE_NON_INTRUSIVE(header,
    version,
    sequence,
    (device_id, 3),
    magic)

} // namespace protocol
```

侵入式宏放在类型内部，会生成 friend 函数，适合 private 成员：

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

每个类型宏支持 1～64 个字段参数，不包含类型名；`(member, n)` 计为一个字段参数。
宏字段可以写成 `member` 或 `(member, n)`。裸字段使用默认宽度；`n` 会作为第二个参数传给 `set` / `get_to`，因此可以表达 3 字节整数、固定读取前 N 个数组元素等协议字段。宏按字段列表顺序编码，不会把字段名或长度信息写入二进制流。遇到前置长度字段、校验和、变长容器读取等更复杂协议细节时，可以继续手写 `to_byte_stream` / `from_byte_stream`。

这些字段宏依赖标准预处理器展开规则。通过 `byte_stream::byte_stream` CMake target 使用时，MSVC 会自动启用 `/Zc:preprocessor`；直接调用 MSVC 编译器时需要手动添加该选项。

如果类型只需要单向转换，可以使用只生成一个函数的宏，减少不需要的 ADL 函数：

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

如果要把某个类型的支持完全放在类外，也可以特化 `byte_stream::byte_stream_codec<T>`。这适合库适配层、第三方类型、或不想在类型命名空间里暴露 ADL 函数的场景：

```cpp
namespace protocol {

struct frame_id {
    uint8_t channel;
    uint32_t value; // 协议里占 3 字节
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

然后可以直接读写：

```cpp
protocol::packet p{/* ... */};

byte_stream::stream stream;
stream.set(p);

auto decoded = byte_stream::stream::parse<protocol::packet>(stream.buffer());
```

## 常用 API

| API | 说明 |
| --- | --- |
| `set(value, n)` | 写入一个值；`n` 对整数/枚举表示字节数，对容器表示元素数量 |
| `get<T>(n)` | 读取并返回一个值 |
| `get_atomic<T>(n)` | 读取并返回一个值；失败时恢复读取位置 |
| `get_to(value, n)` | 最低开销地原位读取到已有对象 |
| `get_to_atomic(value, n)` | 原子读取；失败时读取位置和目标对象都不变 |
| `parse<T>(bytes, endian)` | 精确解析一个对象；支持 vector/string/raw bytes 和显式端序 |
| `set_compact(value)` / `get_compact<T>()` | 使用 varint/zigzag 读写紧凑整数或枚举 |
| `set_atomic(value, n)` | 原子写入；失败时恢复原 buffer 长度 |
| `set_with_size(value)` / `get_to_with_size(value, max_count)` | 读写 compact 长度前缀容器 |
| `get_to_with_size_atomic(value, max_count)` | 原子读取 compact 长度前缀容器 |
| `buffer()` | 获取底层 `std::vector<uint8_t>` |
| `operator[]` / `at()` | 只读访问完整 buffer 中的字节；`at()` 会检查边界 |
| `begin()` / `end()` | 只读遍历完整 buffer，与当前读取位置无关 |
| `take_buffer()` | 零拷贝移出底层 buffer，并将 stream 置空、position 归零 |
| `operator+` / `operator+=` | 拼接两个 stream 的完整 buffer；保留左操作数的 position 和 endian |
| `to_hex(with_spaces, uppercase)` | 将完整 buffer 转为十六进制文本；两个参数默认均为 false |
| `stream::from_hex(text)` | 将十六进制文本转为字节 vector，接受混合大小写并忽略 ASCII 空白 |
| `seek(pos)` / `reset_position()` | 调整读取位置 |
| `remaining()` / `eof()` | 查看剩余可读字节 |
| `set_endian(endian)` | 切换 wire 字节序，可选 `little` / `big`，默认 `little` |

无分配基础 API 位于 `byte_stream/byte_io.hpp`：`byte_writer::write` / `write_compact` /
`write_bytes` 写入调用方固定 buffer；`byte_reader::read` / `read_compact` / `read_bytes` 读取
调用方 buffer；`read_view` 可直接返回输入的子视图。所有操作返回 `byte_stream_errc`，
其中 `ok` 表示成功，`insufficient_space` 表示 writer 剩余容量不足。

## 十六进制转换

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

`to_hex` 返回持有型字符串，静态重载接受 `const std::vector<uint8_t>&`。
它转换完整 buffer，不改变读取位置或字节序设置。
`from_hex(std::string_view)` 返回独立持有的 `std::vector<uint8_t>`，不保留输入视图。
它接受大小写混合，忽略任意位置的 ASCII 空格、制表符、换行、回车、换页和垂直制表符；
因此 `"A B"` 解码为 `0xAB`。空文本或全空白输入返回空 vector。
十六进制字符数为奇数或包含非法字符时抛出错误码为 `invalid_value` 的 `byte_stream_error`；
不接受 `0x` 前缀、逗号或冒号。输出长度溢出报告 `size_overflow`。
两种转换都可能分配内存，与端序设置无关，不改变二进制 wire format。

## 异常处理

持有型 `byte_stream::stream` API 内部检测到的错误通过 `byte_stream::byte_stream_error` 报告。它继承自 `std::runtime_error`，可以用 `what()` 获取错误信息，也可以用 `code()` 获取机器可判断的错误类别。上文的基础 reader/writer API 则返回错误码。

普通 `get` / `get_to` 是最低开销路径；复合或自定义 codec 失败时，读取位置以及 `get_to` 的目标对象可能已部分改变。需要失败时恢复位置可使用 `get_atomic<T>`；需要位置和目标对象都保持不变时，使用 `get_to_atomic` / `get_to_with_size_atomic`，对应类型需要支持默认构造和 `noexcept` 交换。写入热路径使用 `set`；复合 codec 需要失败回滚时使用 `set_atomic`。`set_with_size` 默认保证失败时恢复原 buffer 长度。原子写入中的自定义 codec 应只追加字节，不应清空或改写既有内容。

```cpp
try {
    auto value = stream.get<uint16_t>();
}
catch (const byte_stream::byte_stream_error& err) {
    if (err.code() == byte_stream::byte_stream_errc::insufficient_data) {
        // 输入数据不足
    }
}
```

目前的错误类别包括：

| 错误码 | 说明 |
| --- | --- |
| `ok` | 无异常 API 操作成功 |
| `invalid_size` | 字节数参数不合法，例如 `uint16_t` 读取 3 字节 |
| `insufficient_data` | 剩余可读字节不足 |
| `insufficient_space` | 固定容量 writer 的剩余空间不足 |
| `null_pointer` | 传入空裸指针或空智能指针 |
| `count_out_of_range` | 固定数组读写数量超过数组长度 |
| `missing_element_count` | 读取动态容器时未传入元素数量 |
| `container_too_small` | 容器元素数量少于请求写入数量 |
| `trailing_data` | `parse<T>` 完成对象解析后仍有未消费字节 |
| `invalid_value` | wire 值违反类型约束，例如布尔值不是 `00` / `01` |
| `non_canonical_encoding` | compact 整数不是最短规范编码 |
| `size_overflow` | buffer 或工具输出长度计算溢出、超过容器最大容量 |

## 项目结构

```text
include/byte_stream/byte_stream.hpp       # 持有型高层仅头文件 API
include/byte_stream/byte_io.hpp           # 固定 buffer、无异常 reader/writer API
include/byte_stream/byte_stream_types.hpp # 公共字节序和错误定义
WIRE_FORMAT.md                            # 规范二进制格式和兼容性契约
DESIGN.md                                 # 所有权、分配、原子性、线程和扩展设计
examples/                                 # 可运行示例
tests/                                    # 单元、属性、畸形输入和头文件测试
3rdparty/googletest/                      # 随仓库提供且仅用于测试的依赖
benchmarks/                               # 性能 workload 和回归比较工具
cmake/                                    # 软件包和嵌入式 smoke 项目
.github/workflows/                        # CI、覆盖率和发布自动化
```

## 设计参考

本项目的自定义类型扩展接口参考了 [nlohmann/json](https://github.com/nlohmann/json) 的设计思路，包括通过 ADL 查找转换函数，以及使用侵入式和非侵入式宏生成转换函数。对应到本项目的是 `to_byte_stream` / `from_byte_stream` 和 `BYTE_STREAM_DEFINE_TYPE_*` 宏，实际编码遵循本项目的二进制协议约定。核心库不依赖 nlohmann/json。

## 许可证

MIT 许可证
