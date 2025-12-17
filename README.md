# SAT-Runner 使用文档 (User Manual)

`SAT-Runner` 是一个高性能、生产级的 SAT 求解器自动化调度工具。它专为基准测试设计，支持精确的 CPU 核心绑定、资源隔离和自动化结果归档。

## 1. 安装与编译

### 依赖条件
*   **Operating System**: Linux only (依赖 `taskset` 和 `sched_setaffinity`).
*   **Compiler**: C++17 兼容编译器 (GCC 8+ or Clang).
*   **Build System**: CMake 3.10+.
*   **External Tools**: 必须安装 `runsolver` (v3.4.1+) 并确保其在系统 PATH 中。

### RunSolver 一键安装
本项目提供了一个脚本来自动下载、编译和安装 `runsolver` 及其依赖 (`libnuma-dev`)。

```bash
# 运行安装脚本
./install_runsolver.sh

# 脚本完成后，请按照提示将 runsolver 链接到您的 PATH 中
# 例如：
ln -sf $(pwd)/runsolver_pkg/src/runsolver ./runsolver
export PATH=$PWD:$PATH
```

### SAT-Runner 编译步骤
```bash
mkdir build
cd build
cmake ..
make
# 编译完成后，可执行文件位于 build/sat-runner
```

## 2. 求解器配置 (solvers.json)

`SAT-Runner` 使用 JSON 文件来灵活配置需要评测的求解器。您可以在一个配置文件中定义多个求解器，工具将自动为每个求解器生成独立的输出目录。

### 2.1 配置项说明

| 字段 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `name` | string | 是 | 求解器的唯一标识名称。**重要**：该名称将直接作为结果输出目录的子文件夹名称（例如 `results/set1/my_solver/`）。 |
| `command` | string | 是 | 执行命令模板。支持以下占位符：<br>`{input}`: 实际的 CNF 输入文件路径 (必填)<br>`{timeout}`: 设定的超时时间(秒) |
| `no_pinning` | bool | 否 | 默认为 `false`。仅当您的求解器自带复杂的多线程管理且与外部 CPU 绑定冲突时才设为 `true`。 |

### 2.2 配置实战：使用 test_solver 中的求解器

假设您的项目根目录下有一个 `test_solver` 文件夹，其中包含两个求解器可执行文件：
*   `glucose_static`: Glucose 求解器
*   `kissat`: Kissat 求解器

您可以创建如下内容的 `solvers.json`：

```json
[
    {
        "name": "glucose_default",
        "command": "./test_solver/glucose_static {input}"
    },
    {
        "name": "kissat_quiet",
        "command": "./test_solver/kissat --quiet --time={timeout} {input}"
    }
]
```

#### 详细解析：

1.  **Glucose 配置 (`glucose_default`)**:
    *   **Command**: `"./test_solver/glucose_static {input}"`
    *   **解析**: 这里使用了相对路径调用求解器。运行时，`{input}` 会被替换为具体的 `.cnf` 文件路径（例如 `benchmarks/sc2023/instance_1.cnf`）。
    *   **注意**: 即使求解器本身会输出大量信息到 stdout，您也**不需要**在 command 中手动写 `> log`，因为 `SAT-Runner` 会自动处理标准输出和标准错误的重定向与分离。

2.  **Kissat 配置 (`kissat_quiet`)**:
    *   **Command**: `"./test_solver/kissat --quiet --time={timeout} {input}"`
    *   **解析**:
        *   `--quiet`: 这是一个传递给 kissat 的普通参数，用于减少输出。您可以像在终端中一样随意添加任何求解器支持的参数。
        *   `--time={timeout}`: 这里利用了 `{timeout}` 占位符。如果运行工具时指定了 `--timeout 3600`，那么实际执行的命令将包含 `--time=3600`。这对于需要内部感知超时时间的求解器非常有用。

### 2.3 进阶：如何测试同一个求解器的不同参数？

您可以在数组中多次定义同一个二进制文件，但赋予不同的 `name` 和参数：

```json
[
    {
        "name": "kissat_default",
        "command": "./test_solver/kissat {input}"
    },
    {
        "name": "kissat_sat_mode",
        "command": "./test_solver/kissat --sat {input}"
    },
    {
        "name": "kissat_unsat_mode",
        "command": "./test_solver/kissat --unsat {input}"
    }
]
```
这样，一次运行即可对比同一求解器在不同配置下的表现。

## 3. 快速开始

### 运行基准测试
```bash
# 自动探测所有物理核心，并发数限制为 8
./build/sat-runner \
    --input ./benchmarks/SAT2023 \
    --output ./results/experiment_1 \
    --config solvers.json \
    --jobs 8

# 或者手动指定核心范围
./build/sat-runner \
    --input ./benchmarks/SAT2023 \
    --output ./results/experiment_2 \
    --cores 0-7,16-23 \
    --config solvers.json \
    --jobs 8
```

## 4. 命令行参数详解

| 参数 | 简写 | 必选 | 说明 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| `--input` | `-i` | 是 | 输入文件或目录路径 (支持多个)。工具会递归扫描所有 `.cnf` 文件。 | `-i ./data -i ./extra` |
| `--output` | `-o` | 是 | 输出根目录。 | `-o ./results` |
| `--cores` | `-C` | **否** | **核心资源池**。指定允许使用的物理 CPU 核心 ID。**若省略，自动探测所有可用物理核心。** | `-C 0-7` (默认: 自动) |
| `--config` | `-c` | 是 | 求解器配置文件路径 (.json)。 | `-c solvers.json` |
| `--jobs` | `-j` | 否 | 最大并发任务数。实际并发数取 `min(jobs, available_cores)`。 | `-j 16` (默认 1) |
| `--timeout`| `-t` | 否 | 每个任务的超时时间 (秒)。 | `-t 5000` (默认 3600) |
| `--mem-limit`| `-m` | 否 | 内存限制 (MB)。传递给 runsolver (`--vsize-limit`)。 | `-m 32768` (默认 16384) |

## 5. 输出结构

工具执行后，`--output` 指定的目录下将生成以下结构：

```text
results/
├── index.csv               # 汇总索引文件
├── <Benchmark_Set_Name>/   # 对应输入文件所在的父文件夹名称
│   ├── <Solver_Name>/
│       ├── instance1.log      # 标准输出 (stdout)
│       ├── instance1.err      # 标准错误 (stderr)
│       ├── instance1.var      # RunSolver 关键值日志 (易于解析)
│       └── instance1.watcher  # RunSolver 原始监控数据
```

### index.csv 格式
CSV 文件包含每次运行的关键元数据，便于快速分析：
`Benchmark_Set, Solver, Instance, CPU_Core_ID, Exit_Code`

*   **CPU_Core_ID**: 记录该任务实际绑定到了哪个物理核心，用于排查硬件性能差异。

## 6. 高级特性：CPU 亲和性与资源管理

本工具采用“洋葱式”执行模型来保证评测的准确性：

1.  **Core Pool**: 启动时解析 `--cores` 参数建立空闲核心池。
2.  **Pinning**: 每个任务在启动前必须申请一个独占的核心 ID。
3.  **Execution**: 使用 `taskset -c <CoreID> runsolver ...` 启动任务。这确保了求解器及其所有子进程被严格锁定在指定核心上，避免操作系统调度带来的上下文切换开销和缓存污染。
4.  **No-Pinning**: 对于某些原生多线程求解器，可在 `solvers.json` 中设置 `"no_pinning": true` 来跳过核心绑定（但仍受并发数限制）。

## 7. 常见问题 (FAQ)

*   **Q: 为什么并发数没有达到我设置的 `--jobs`？**
    *   A: 并发数受限于 `--cores` 指定的可用核心数。如果你设置了 `-j 10` 但只提供了 `-C 0-3` (4个核心)，那么实际并发只有 4。

*   **Q: 找不到 `runsolver`？**
    *   A: 请确保 `runsolver` 已经编译并安装在系统的 `$PATH` 中，或者使用 `install_runsolver.sh` 脚本进行安装。

*   **Q: 如何处理非 CNF 文件？**
    *   A: 当前版本扫描器仅识别后缀为 `.cnf` 的文件。

---
**License**: MIT
**Author**: SAT-Runner Team