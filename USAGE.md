# SAT-Runner 使用文档 (User Manual)

`SAT-Runner` 是一个高性能、生产级的 SAT 求解器自动化调度工具。它专为基准测试设计，支持精确的 CPU 核心绑定、资源隔离和自动化结果归档。

## 1. 安装与编译

### 依赖条件
*   **Operating System**: Linux only (依赖 `taskset` 和 `sched_setaffinity`).
*   **Compiler**: C++17 兼容编译器 (GCC 8+ or Clang).
*   **Build System**: CMake 3.10+.
*   **External Tools**: 必须安装 `runsolver` 并确保其在系统 PATH 中。

### 编译步骤
```bash
mkdir build
cd build
cmake ..
make
# 编译完成后，可执行文件位于 build/sat-runner
```

## 2. 快速开始

### 准备求解器配置
创建一个 JSON 文件 (例如 `solvers.json`) 定义要测试的求解器：
```json
[
    {
        "name": "kissat_default",
        "command": "/usr/bin/kissat --time={timeout} {input}"
    },
    {
        "name": "minisat",
        "command": "/usr/bin/minisat {input} /dev/null"
    }
]
```
*   `{input}`: 将被替换为实际的 `.cnf` 文件路径。
*   `{timeout}`: 将被替换为设定的超时时间 (秒)。

### 运行基准测试
```bash
# 自动探测所有物理核心，并发数限制为 8
./sat-runner \
    --input ./benchmarks/SAT2023 \
    --output ./results/experiment_1 \
    --config solvers.json \
    --jobs 8

# 或者手动指定核心范围
./sat-runner \
    --input ./benchmarks/SAT2023 \
    --output ./results/experiment_2 \
    --cores 0-7,16-23 \
    --config solvers.json \
    --jobs 8
```

## 3. 命令行参数详解

| 参数 | 简写 | 必选 | 说明 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| `--input` | `-i` | 是 | 输入文件或目录路径 (支持多个)。工具会递归扫描所有 `.cnf` 文件。 | `-i ./data -i ./extra` |
| `--output` | `-o` | 是 | 输出根目录。 | `-o ./results` |
| `--cores` | `-C` | **否** | **核心资源池**。指定允许使用的物理 CPU 核心 ID。**若省略，自动探测所有可用物理核心。** | `-C 0-7` (默认: 自动) |
| `--config` | `-c` | 是 | 求解器配置文件路径 (.json)。 | `-c solvers.json` |
| `--jobs` | `-j` | 否 | 最大并发任务数。实际并发数取 `min(jobs, available_cores)`。 | `-j 16` (默认 1) |
| `--timeout`| `-t` | 否 | 每个任务的超时时间 (秒)。 | `-t 5000` (默认 3600) |
| `--mem-limit`| `-m` | 否 | 内存限制 (MB)。传递给 runsolver。 | `-m 32768` (默认 16384) |

## 4. 输出结构

工具执行后，`--output` 指定的目录下将生成以下结构：

```text
results/
├── index.csv               # 汇总索引文件
├── <Benchmark_Set_Name>/   # 对应输入文件所在的父文件夹名称
│   ├── <Solver_Name>/
│       ├── instance1.log      # 标准输出 (stdout)
│       ├── instance1.err      # 标准错误 (stderr)
│       └── instance1.watcher  # RunSolver 监控数据 (时间、内存等)
```

### index.csv 格式
CSV 文件包含每次运行的关键元数据，便于快速分析：
`Benchmark_Set, Solver, Instance, CPU_Core_ID, Exit_Code`

*   **CPU_Core_ID**: 记录该任务实际绑定到了哪个物理核心，用于排查硬件性能差异。

## 5. 高级特性：CPU 亲和性与资源管理

本工具采用“洋葱式”执行模型来保证评测的准确性：

1.  **Core Pool**: 启动时解析 `--cores` 参数建立空闲核心池。
2.  **Pinning**: 每个任务在启动前必须申请一个独占的核心 ID。
3.  **Execution**: 使用 `taskset -c <CoreID> runsolver ...` 启动任务。这确保了求解器及其所有子进程被严格锁定在指定核心上，避免操作系统调度带来的上下文切换开销和缓存污染。
4.  **No-Pinning**: 对于某些原生多线程求解器，可在 `solvers.json` 中设置 `"no_pinning": true` 来跳过核心绑定（但仍受并发数限制）。

## 6. 常见问题 (FAQ)

*   **Q: 为什么并发数没有达到我设置的 `--jobs`？**
    *   A: 并发数受限于 `--cores` 指定的可用核心数。如果你设置了 `-j 10` 但只提供了 `-C 0-3` (4个核心)，那么实际并发只有 4。

*   **Q: 找不到 `runsolver`？**
    *   A: 请确保 `runsolver` 已经编译并安装在系统的 `$PATH` 中，或者使用绝对路径修改源码中的调用逻辑（目前默认为直接调用命令）。

*   **Q: 如何处理非 CNF 文件？**
    *   A: 当前版本扫描器仅识别后缀为 `.cnf` 的文件。

---
**License**: MIT
**Author**: SAT-Runner Team
