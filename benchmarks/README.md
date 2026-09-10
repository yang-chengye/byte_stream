# 性能测试

使用 Release 模式构建并运行 benchmark：

```bash
cmake --preset release
cmake --build --preset release
./build/release/benchmarks/byte_stream_performance
./build/release/benchmarks/byte_stream_coverage_benchmark
```

`byte_stream_performance` 是性能回归 benchmark。每项指标先执行一次不计时预热，再采集七个样本，
最后报告中位数和范围。可复用 buffer 会在计时前完成分配，校验和则在每个计时循环结束后消费。
不同 workload 组位于独立翻译单元，避免新增 benchmark 改变其他组可用的编译器内联预算。

默认输出便于人工阅读。使用 `--csv` 可获得稳定的机器可读结果：

```bash
./build/release/benchmarks/byte_stream_performance --csv > baseline.csv
# 修改代码并重新构建。
./build/release/benchmarks/byte_stream_performance --csv > current.csv
python3 benchmarks/compare.py baseline.csv current.csv --max-regression-percent 5
```

基线和当前结果应在同一台空闲机器上采集，并使用相同的编译器、编译选项、CPU 调频策略和热状态。
共享 CI runner 适合进行 smoke test；除非已在受控硬件上重复确认回退，否则不适合作为严格性能门禁。

`byte_stream_coverage_benchmark` 是场景覆盖 benchmark，涵盖小型和中型 payload、只读 buffer
访问、浮点数、标准容器、大端路径、嵌套类型、重复解析、有界输入、畸形输入、原子读取和
variant。它用于暴露测试盲区，不提供稳定的通过/失败阈值。
