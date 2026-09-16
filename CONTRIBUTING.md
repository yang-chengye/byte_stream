# 贡献指南

本文面向所有贡献者，集中维护编码规范、开发流程和验收要求。
库的行为契约见 [DESIGN.md](DESIGN.md)，字节级兼容性见 [WIRE_FORMAT.md](WIRE_FORMAT.md)，AI 工作要求见 [AGENTS.md](AGENTS.md)。

## 开发环境与构建

保持 C++17、GCC 9.x、常见 Clang/MSVC、Windows/Linux 和嵌入式 Linux 交叉编译环境可用。
不直接引入 C++20 concepts、ranges、`std::span` 或 C++23 API；需要时提供 C++17 兼容封装。
不兼容变更必须明确讨论并接受。以下 preset 命令使用仓库的 CMake 3.21+ 配置：

```bash
cmake --preset strict
cmake --build --preset strict
ctest --preset strict

cmake --preset release
cmake --build --preset release
ctest --preset release
```

GoogleTest 是唯一的测试依赖。固定版本源码位于 `3rdparty/googletest`，无需系统安装或在配置时访问网络。

## 编码规范

每项代码变更都必须考虑运行时性能、二进制体积和可移植性，优先简单、可组合的基础能力。

- 优先编译期分发、模板、类型萃取、`constexpr`、`if constexpr` 和容易内联的小函数；使用 `static_assert` 表达编译期假设。
- 使用 `std::memcpy` 安全搬运对象与字节，避免违反 strict aliasing 的类型双关和未对齐对象访问。
- 字节序转换集中封装；编译器 byteswap 或平台优化放在内部函数中，提供编译期检测、portable fallback 和测试，避免在业务代码中重复实现。
- 优先连续访问、预计算 size、提前 `reserve` 和复用容量；避免频繁小步扩容及不必要的数据拷贝。
- 避免不必要的堆分配、虚函数、类型擦除、临时对象、全局可变状态和重复边界检查，但不得移除必要检查。
- 热路径不使用 `std::function`、`std::regex`、`iostream`、日志、格式化或临时字符串构造。
- 错误接口应轻量、可观察、可恢复和可测试。新增热路径接口优先状态码、`bool` 或轻量 result，正确使用 `noexcept` 和 `[[nodiscard]]`；不要以异常作为常规控制流，保留持有型 stream 的现有异常契约。
- public header 保持轻量，能前置声明时避免重头文件；实现细节放在 internal/detail 命名空间。
- 宏仅用于必要的兼容处理和既有类型扩展机制。命名清晰、函数职责单一，注释解释原因，不重复代码。
- 十六进制转换和转义工具与核心热路径分离，不引入重依赖；输出格式应稳定可测，大 buffer 输出应考虑长度限制。
- 不随意修改 public API 名称，不在一次变更中混入无关格式化或清理。

## API 与安全评审

修改 public API 时，对照 DESIGN 中的现有契约，明确零分配用法、嵌入式适用性、误用风险和可观察的错误。
分别考虑调用方已有 buffer 与内部持有 buffer 的使用方式，避免为扩展增加依赖或使常见场景复杂化。
自定义类型扩展优先 ADL、traits 和编译期约束，提供清晰诊断，不要求继承基类或虚函数；保留类外适配能力。

评审 buffer 变更时，检查所有权、view 生命周期、扩容能力、position、剩余空间、已写长度、最大长度、seek 后写入和读写失败语义。
使用明确的 size 类型，在扩容、偏移和长度计算前检查溢出，避免超过配置上限。

所有 buffer 读写必须检查边界，外部输入默认不可信。解码必须拒绝截断输入、长度超过剩余输入、size 溢出、越界 offset/seek 和不匹配的目标大小。
协议要求校验枚举、版本或格式标记时，由相应 codec 拒绝不合法值；通用枚举行为遵守 DESIGN。
除非证明相应情况在所有支持目标上均不可能发生，否则不得移除安全检查。

涉及 wire format 时，明确 endian、整数宽度、容器长度编码和字符串编码假设；浮点数转换使用安全的位复制，非标准宽度整数遵循 WIRE_FORMAT。
任何字节级变化都必须同步更新 WIRE_FORMAT 和黄金向量。

## 单元测试与输入验证

新增功能或修复 bug 时增加针对性测试，解码器和 parse 变更增加畸形输入或 fuzz 风格测试。
根据受影响行为覆盖以下场景：

- serialize/deserialize round-trip，嵌套结构体和自定义类型。
- 整数 0、min、max 和负数；浮点数普通值及库支持的 NaN/Inf。
- 空容器、大容器、字符串长度边界、异常容器长度字段。
- buffer 不足、大小端转换、offset/seek、同一 stream 重复使用和失败后的错误状态。

不要因为测试失败而删除测试；应修复行为，或明确说明旧测试为何不再合理。
代码变更完成后运行严格构建和 Release 构建及单元测试。内存访问或输入校验变更还应使用 ASan/UBSan：

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

纯文档变更检查链接、命令与现有配置的一致性，以及 `git diff --check`；不要求构建或性能测试，但报告中应明确哪些验证未运行。

## 性能验证

序列化、反序列化、buffer 管理、endian、容器、字符串、parse、边界检查及可能影响热路径的 public API 变更，都必须在修改前后运行两组 Release benchmark 并比较基线。
新增热路径功能必须新增或更新对应 benchmark。

```bash
./build/release/benchmarks/byte_stream_performance
./build/release/benchmarks/byte_stream_coverage_benchmark
```

采样方式、CSV 导出和比较脚本用法集中维护在 [benchmarks/README.md](benchmarks/README.md)。
在同一机器和相同编译配置下多次采样，报告编译器、主机、构建类型、中位数或区间及受影响指标。
不得根据单次噪声采样或没有 benchmark 数据时声称性能提升。

benchmark 覆盖范围至少包括：

- 固定宽度整数、浮点数、struct、嵌套 struct 和常见标准容器的序列化与反序列化。
- 小、中、大 payload，空容器和最大允许长度。
- 小端、大端、顺序写入、顺序读取、重复 parse 和 buffer 不足。

测量时分离 setup 和核心编解码成本；除非目标就是分配性能，否则将分配放在计时外。
被测循环中不输出日志，防止编译器消除被测逻辑，尽量报告操作次数、payload 大小、耗时和吞吐率。

可接受的结果是性能提升、合理噪声范围内不变，或为兼容性、安全性、易用性、可维护性及正确性作出的明确取舍。
任何性能回退都必须列出 benchmark 名称、修改前后数据、接受原因、考虑过的更快实现和后续优化空间。

## 依赖与文档

核心库保持无第三方运行时依赖。若提出引入依赖，必须说明标准库为何不足、轻量替代方案、可选开关、嵌入式交叉编译影响、编译时间、二进制体积和 license 风险。
测试与 benchmark 的开发依赖不得传递到核心库。

新增或修改 public API 时同步更新 [README.md](README.md) 的用法和 DESIGN 的契约，说明所有权、生命周期、endian、错误处理、分配与拷贝行为、wire 兼容性和性能注意事项。
示例应短小、可编译并贴近真实使用场景。开发流程只在本文维护，避免复制到 AGENTS 或 DESIGN。

`README.md` 为默认中文版，英文版位于 [README.en.md](README.en.md)。修改 README 的内容或示例时同步更新两种语言，并保留顶部的语言切换链接。

## 提交前检查与完成标准

- 适用的构建和单元测试通过，新增行为具有针对性覆盖。
- 性能敏感变更完成 Release benchmark 和基线对比，回退有数据和取舍说明。
- public API 文档已同步，wire format 变化有明确记录及黄金向量。
- 未引入无关依赖、重命名或格式化变更。
- 提交或 PR 说明清楚描述问题、最终行为、验证结果、风险和取舍；未运行的检查明确注明，不宣称验证完成。
