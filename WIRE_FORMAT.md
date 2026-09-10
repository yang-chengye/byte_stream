# byte_stream 二进制格式

本文定义 `byte_stream` 0.1.x 输出的规范 wire format。它是编码字节的兼容性契约；实现细节和内存中的 C++ 对象布局不属于该格式。

## 字节流模型

值按照函数调用顺序依次拼接。格式不会隐式加入字段名、类型标识符、对齐填充、schema 标识符或版本字节。需要这些字段的协议必须显式写入。

持有型 `byte_stream::stream`（`set` / `get_to`）与非持有型 `byte_writer` / `byte_reader`（`write` / `read`）采用相同的标量和 compact 编码。选择零分配 API 不会改变规范字节。

默认 wire 字节序为 `byte_stream::endian::little`。`set_endian(byte_stream::endian::big)` 会让后续固定宽度标量使用大端。
公共 API 只提供这两种字节序模式；本库有意不暴露主机原生字节序，因为它无法定义可移植的wire 表示。

切换字节序会影响固定宽度整数、枚举、浮点值以及由这些类型构成的容器，不影响单字节原始数据或 compact 整数。

## 固定宽度标量

### 整数

可移植 wire 契约覆盖 `<stdint.h>` 中明确宽度的整数类型，其自然编码宽度为 `sizeof(T)`。
向 `set` 或 `get` 传入 `n` 时会恰好使用 `n` 个字节，其中 `1 <= n <= sizeof(T)`。

无符号整数按模 2^(8n) 编码。有符号整数使用其二进制补码表示的低 `8n` 位。读取缩短的有符号整数时，会把第 `8n - 1` 位符号扩展至目标宽度。

普通 `char`、`short`、`int`、`long`、`long long` 和其他由实现决定宽度的整数类型使用其平台 `sizeof(T)`，不建议在可移植协议定义中使用。

### 布尔值

`false` 编码为 `00`，`true` 编码为 `01`。解码器会把其他所有字节作为 `invalid_value` 拒绝。

### 枚举与 `std::byte`

枚举按照其底层整数类型编码，也可以使用相同的缩短宽度参数 `n`。通用解码器不会验证解码结果是否对应某个具名枚举项；需要验证的协议必须提供自定义 codec。

`std::byte` 编码为一个原始字节。

### 浮点值

`float` 和 `double` 的 IEEE 754 二进制表示会先复制到 32 位或 64 位无符号整数，再应用所选字节序。
该过程不执行数值转换，因此在支持 IEEE 754 的平台上会保留有符号零、无穷大和 NaN payload 位。

如果传入可选参数 `n`，其值必须等于 `sizeof(T)`。可移植格式不支持 16 字节 `long double` 等扩展浮点格式。

## 紧凑整数编码

`set_compact` 使用无符号 LEB128 编码无符号值：每个字节包含 7 个 payload 位，低位组在前；如果后面还有字节，则设置第 7 位。编码器始终输出最短表示。

有符号值先经过 ZigZag 转换，再按无符号 LEB128 写入。枚举使用其底层类型的符号性和宽度。
Compact 编码与已配置的字节序无关。

解码器会拒绝未终止值、超过 64 位的值、无法放入请求目标类型的值和过长编码。因此，每个被接受的 compact 整数都只有一种规范的最短表示。

## 容器与复合值

调用 `set(container)` 时，会按照迭代顺序写入元素，但不隐式写入元素数量。
因此，调用 `get_to(container, count)` 时必须由外层协议提供元素数量。连续的单字节容器会按原始字节复制，其他元素使用各自的常规 codec。

`set_with_size(container)` 会在元素前写入 compact `uint32_t` 数量。
`get_to_with_size(container, max_count)` 会拒绝超过调用者上限的数量。便利 API 还要求元素数量小于 `2^32 - 1`。

复合格式按照以下顺序拼接：

- `std::array<T, N>` 和 C 数组：按顺序写入第 `0` 至 `N - 1` 个元素，除非显式提供更小数量。
- `std::optional<T>`：先写一个布尔存在标记；仅当值存在时再写入 `T`。
- `std::unique_ptr<T>` 和 `std::shared_ptr<T>`：只写入指向的 `T`，不包含空指针标记；编码空指针属于错误。
- `std::pair<A, B>`：先写 `A`，再写 `B`。
- `std::tuple<Ts...>`：按照 tuple 索引递增顺序写入元素。
- `std::variant<Ts...>`：先将从零开始的备选项索引编码为规范 compact `uint32_t`，再使用活动类型的独立编码规则写入值。
  `std::monostate` 没有 payload。
  动态容器备选项必须显式表示为 `length_prefixed<Container, MaxCount>`；未包装的动态容器会在编译期被拒绝。
  无效索引会被拒绝，且反序列化要求每个备选类型均可默认构造。
- `length_prefixed<Container, MaxCount>`：使用与 `set_with_size` 相同的 compact 数量和元素序列。
  编码和解码都会拒绝超过 `MaxCount` 的数量。无论独立使用，还是嵌套在 variant、tuple、optional 或自定义 codec 中，其字节均保持一致。
- 自定义 ADL、宏和 `byte_stream_codec<T>` 类型：严格按照相应 codec 的调用顺序写入字段。

字符串是原始字节序列。`byte_stream` 不添加终止符、不验证文本，也不指定字符编码。

## 解析与游标行为

`byte_stream::stream::parse<T>(bytes, endian)` 恰好解码一个 `T`。字节序参数默认为规范小端，也可以显式选择大端。
支持字节 vector、字符串原始字节以及 `const void*` 加长度的输入。解析会拒绝截断输入和尾随字节。
需要从较大数据帧中逐字段解析时，应构造 `byte_stream::stream` 后直接调用 `get`/`get_to`。

读取成功后，`position()` 会按照字段编码长度前移。只有目标位置处于 buffer 范围内时，`seek()` 才会改变读取位置。

`get_to` 是开销最低的原地路径。基础标量和连续容器 codec 会在修改目标前验证输入，但失败的复合或自定义 codec 可能已经推进 position 并部分修改目标。
`get_atomic` 会在失败时恢复 position。
`get_to_atomic` 和 `get_to_with_size_atomic` 保证 position 和目标均保持不变；目标类型必须可以默认构造且可无异常交换。
显式原子 API 会解码至临时对象，因此反复解码至自身持有容量的对象时，可能比原地 API 成本更高。

编码失败时，`set_atomic` 和 `set_with_size` 会恢复先前的 buffer 长度。普通 `set` 是开销最低的追加路径，失败的复合自定义 codec 可能已经追加前缀。
以原子方式使用的自定义 codec 必须通过 stream 写入 API 追加数据，且写入过程中不得清空或以其他方式重写先前已编码的字节。

## 黄金字节向量

下表中的空格仅为便于阅读，不会被编码。

| 操作 | 字节序 | 规范字节 |
| --- | --- | --- |
| `set<uint16_t>(0x1234)` | 默认/小端 | `34 12` |
| `set<uint32_t>(0x00A1B2C3, 3)` | 默认/小端 | `c3 b2 a1` |
| `set<uint16_t>(0x1234)` | 大端 | `12 34` |
| `set<uint32_t>(0x00A1B2C3, 3)` | 大端 | `a1 b2 c3` |
| `set<int16_t>(-2)` | 默认/小端 | `fe ff` |
| `set<int16_t>(-2, 1)` | 任意 | `fe` |
| `set<float>(1.5f)` | 默认/小端 | `00 00 c0 3f` |
| `set<float>(1.5f)` | 大端 | `3f c0 00 00` |
| `set(true); set(false)` | 任意 | `01 00` |
| `set_compact<uint64_t>(0)` | 任意 | `00` |
| `set_compact<uint64_t>(127)` | 任意 | `7f` |
| `set_compact<uint64_t>(128)` | 任意 | `80 01` |
| `set_compact<uint64_t>(300)` | 任意 | `ac 02` |
| `set_compact<int32_t>(-1)` | 任意 | `01` |
| `set_compact<int32_t>(-64)` | 任意 | `7f` |
| `set_compact<int32_t>(64)` | 任意 | `80 01` |
| `set_with_size(vector<uint16_t>{0x1234, 0x5678, 0x9ABC})` | 默认/小端 | `03 34 12 78 56 bc 9a` |
| `set(optional<uint16_t>{0x1234}); set(nullopt)` | 默认/小端 | `01 34 12 00` |
| `set(variant<monostate, uint16_t>{uint16_t{0x1234}})` | 默认/小端 | `01 34 12` |
| `set(length_prefixed<string, 8>{"wire"})` | 任意 | `04 77 69 72 65` |

`tests/main.cpp` 中可执行的黄金向量测试是权威示例。本文发生变化时，必须有意且同步地更新这些测试。

## 兼容性策略

已记录操作所输出的规范字节不得静默改变。未来的格式变化必须使用显式选择启用的 API 或新的主版本，并提供迁移说明、更新后的黄金向量以及修改前后的 benchmark 数据。

畸形或非规范输入不在兼容性保证范围内，会被拒绝。未来如改变可接受输入或规范输出，必须在变更日志中明确说明。
