# 设计与 API 契约

本文说明 `byte_stream` 的设计与 API 行为契约。字节级兼容性在 [WIRE_FORMAT.md](WIRE_FORMAT.md) 中单独定义，基本用法见 [README.md](README.md)。
编码规范、测试与性能验收流程统一维护在 [CONTRIBUTING.md](CONTRIBUTING.md)，AI 工作要求见 [AGENTS.md](AGENTS.md)。

## 项目范围

库分为两层。`byte_stream::stream` 是拥有自身内存、面向中小型二进制协议帧的内存编码器和解码器。
`byte_reader` 和 `byte_writer` 是面向嵌入式、网络和实时路径的非持有、零分配基础组件。
项目面向 C++17、GCC 9 及更高版本、常见 Clang/MSVC 工具链和嵌入式 Linux 构建。
核心库为 header-only，不包含第三方运行时依赖。

本库有意不提供 schema、反射、压缩、校验和、传输 I/O 或自动协议版本管理。这些策略应由协议层负责。

## 所有权与生命周期

`byte_stream::stream` 始终拥有其 `std::vector<uint8_t>` buffer：

- 从 `const void*`、`std::string` 或 const 字节 vector 构造时会复制输入。
- 从右值字节 vector 构造时会转移其内存分配。
- `buffer()` 和 `data()` 返回非持有视图。任何写入、`append`、`clear`、赋值或 `take_buffer()` 都可能使其地址失效。
- `operator[]`、`at()` 和迭代器仅提供只读访问，始终面向完整 buffer，不受读取位置影响；修改 stream 后，已取得的引用和迭代器可能失效。
- `operator+` 和 `operator+=` 拼接两个对象的完整 buffer，不读取或改变右操作数的 position；结果保留左操作数的 position 和 endian。
- `take_buffer()` 在不复制的情况下把内存转移给调用者，并将 stream 置为空且 position 归零；已配置的字节序保持不变。

该对象同时包含输出 buffer 和可变读取游标。通过 const 引用也可以读取，因此可能改变 `position()`。

`byte_view`、`byte_reader` 和 `byte_writer` 从不持有存储。调用者必须保证被引用的存储仍然存在，
且地址保持稳定。`byte_reader::read_view` 返回同一输入中的零拷贝子区间。`byte_writer` 分别跟踪
游标和逻辑写入长度：向后 seek 可以修补已有帧头而不截断后续字节；超过逻辑长度的 seek 会被拒绝。
`clear()` 只重置这两个值，不修改或释放调用方存储。

## 流状态与对象赋值

`stream()` 创建小端空流；`explicit stream(endian)` 创建指定字节序的空流，读取位置均为 0。
字节序是流状态，不是 buffer 中的标记。`set_endian(e)` 只改变后续读写的字节序，
不转换已有数据，也不改变读取位置。混合字节序数据需要在读取相应字段前显式设置字节序。

- `bs = obj` 使用 bs 当前字节序和默认编码宽度序列化对象，替换全部数据，读取位置归零，保留字节序。
- `bs.set(obj)` 向末尾追加，不改变读取位置；`get<T>()` / `get_to(obj)` 按当前字节序从当前位置读取。
- 流之间的复制、移动构造和赋值转移完整状态，包括 buffer、读取位置和字节序；不调用对象 codec。
  移动后的源对象有效但内容未指定。流及其派生类型不参与对象赋值模板。
- 字节 vector 的对象赋值按容器 codec 序列化；转移 buffer 所有权可使用 `bs = stream(std::move(bytes))`，
  这属于流状态移动，目标也会继承该临时流的默认小端状态。

对象赋值先 `clear()` 再 `set(obj)`，复用已有容量，不创建临时流。
编码失败时原数据已清除，可能留下部分新数据；对于遵守追加契约的 codec，位置为 0、字节序不变。
输入对象不得引用目标流自身的 buffer 或其中元素，包括 `bs = bs.buffer()` 和 `bs = bs[0]`。
赋值所用自定义 codec 必须只追加数据，不改变字节序或读取位置。
需要强异常保证或输入引用原 buffer 时，可先用相同字节序的独立流编码，成功后再移动赋值。
对象赋值可能因扩容或 codec 失败而抛出异常，返回 `stream&`。

## 零分配和无异常 API

包含 `byte_stream/byte_io.hpp` 后，可以使用固定宽度标量、compact 整数、原始字节和零拷贝视图操作，
无需包含持有型 stream。所有操作均为 `noexcept` 并返回 `byte_stream_errc`，其中 `ok` 表示成功。
失败不会改变游标和标量目标。固定容量耗尽返回 `insufficient_space`，输入截断返回 `insufficient_data`。

CI 会在禁用异常的配置下编译这个底层头文件。它有意提供可组合的基础组件，不提供需要分配内存的标准容器或用户 codec 便利接口。
持有型 `byte_stream::stream` 仍作为这些场景的高层 API，并通过 `byte_stream_error` 报告错误。

## 快速操作与原子操作

默认 API 针对协议热路径优化。原子行为需要显式选择，避免调用者在未请求时承担临时对象开销，或丢失可复用的容器容量。

| 操作 | Codec 失败时的行为 |
| --- | --- |
| `set` | 复合自定义 codec 可能已经追加前缀 |
| `set_atomic` | 恢复先前的 buffer 长度 |
| `get` / `get_to` | 游标和目标可能已被部分推进或修改 |
| `get_atomic` | 恢复游标 |
| `get_to_atomic` | 游标和目标均保持不变 |
| `set_with_size` | 恢复先前的 buffer 长度 |
| `get_to_with_size` | 恢复游标；非连续目标可能已被部分修改 |
| `get_to_with_size_atomic` | 游标和目标均保持不变 |
| `parse` | 精确解析，失败则抛出异常；所有解析器状态均为局部状态 |

目标原子读取要求类型可以默认构造且可无异常交换。原子写入可以恢复追加长度，但无法撤销恶意自定义codec 对旧字节的清空或重写。
与 `set_atomic` 配合的自定义序列化器必须只通过 `byte_stream::stream` API 执行追加操作。

## 内存分配行为

- 输出容量充足时，固定宽度标量读写不分配内存。
- 输出容量充足时，compact 整数不分配内存。
- 调用 `reserve()` 可以消除写入热路径中的扩容分配。
- 读取连续容器时会复用其容量。
- 通用顺序容器按照自身正常的插入行为分配内存。
- `get_to_atomic` 会构造临时目标；即使调用方目标已有可复用容量，也可能发生分配。
- `to_hex`、`from_hex` 和转义辅助函数按设计返回持有型对象，因此会分配内存。

读取带长度前缀的数据时，调用者必须提供最大值。协议 codec 在根据不可信计数字段分配内存前，也必须施加合适的限制。

## 十六进制文本 API

- `to_hex(with_spaces = false, uppercase = false)` 转换完整 buffer，返回持有型字符串；不改变 position 或 endian。
  静态重载接受字节 vector，具有相同的格式选项。空输入返回空字符串，相邻字节可用单个空格分隔，无首尾空格。
- `from_hex(std::string_view)` 返回持有型字节 vector，不保留输入视图，不修改任何 stream。
  接受大小写混合；忽略任意位置的 ASCII 空格、`\t`、`\n`、`\r`、`\f`、`\v`，不依赖 locale。
  空输入或全空白返回空 vector；`"A B"` 按 `"AB"` 解析。
- 非十六进制字符（包括 `0x` 前缀、逗号、冒号和 NUL）或奇数个十六进制字符报告 `invalid_value`。
  输出超出目标容器最大容量时报告 `size_overflow`；内存分配失败遵循标准容器的异常行为。
- 转换与端序无关，不改变 wire format。它们不是零分配 API；解码先验证并计数，再分配输出，不创建去空白文本副本。

## 线程模型

不同的 `byte_stream::stream` 对象可以并发使用。单个对象不能被多个线程并发访问，包括通过 const 引用执行读取，因为读取会修改游标。共享对象时需要外部同步。

## 扩展契约

ADL 函数和 `byte_stream_codec<T>` 特化必须：

- 按确定顺序编码字段；
- 在可移植协议中使用明确的固定宽度整数类型；
- 校验协议特有的枚举范围、保留位、版本和计数上限；
- 在协议能够写入预留存储时避免隐藏分配；
- 记录每项字节级变化；
- 如果调用者可能使用 `set_atomic`，则只执行追加操作。

通用枚举 codec 会保留未知数值。协议特有的枚举校验应由协议 codec 完成，因为只有该层知道未知值是否需要保持向前兼容。

动态容器在嵌套时不会静默改变表示方式。特别是 variant 备选项会使用与该类型独立使用时相同的 codec。
需要在复合值内放置自描述边界容器的协议必须使用 `length_prefixed<Container, MaxCount>`；将上限写入类型可以让分配限制明确且便于评审。
