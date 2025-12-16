#SAT Solver High-Performance Benchmarking Tool - 产品需求文档 (PRD) v3.0##1. 产品概述本工具是一个基于 **C++** 开发的高性能、生产级 SAT 求解器自动化调度工具。
其设计目标不仅仅是“批量运行”，更是**“精确评测”**。工具通过**黑盒调度**、**RunSolver 资源监控**以及**严格的 CPU 核心绑定**技术，最大限度减少操作系统调度干扰，为 SAT 求解器提供一个稳定、隔离的运行环境，并将原始输出按标准目录结构归档。

##2. 核心价值与变更点 (Key Features)* **CPU 亲和性绑定 (CPU Affinity)**：**[v3.0 新增]** 能够将每个求解器进程精确锁定在指定的物理 CPU 核心上，消除上下文切换带来的性能抖动。
* **纯净黑盒模式**：不解析输出，只做搬运工，确保对任何非标输出的求解器兼容。
* **结构化归档**：严格执行 `Output/Benchmark_Set/Solver/Instance.log` 的目录规范。

##3. 功能需求 (Functional Requirements)###3.1 资源编排与并发控制 (Resource Orchestration) — **核心升级**工具必须维护一个**“CPU 核心资源池”**，用于管理物理硬件资源。

* **核心资源池管理**：
* 用户需能指定允许使用的 CPU 核心列表（例如：`0,1,2,3` 或 `0-7`）。
* 程序启动时，建立一个“空闲核心队列”。
* **并发逻辑变更**：最大并发任务数不再单纯由 `--jobs` 决定，而是受限于**可用核心数**。即：`并发数 = min(用户设置的jobs, 可用核心数)`。


* **进程级绑定 (Process Pinning)**：
* 在启动求解器任务前，必须从资源池中**借出 (Checkout)** 一个空闲的核心 ID。
* 利用 Linux 的 `taskset` 命令（或 `sched_setaffinity` 系统调用）将 `RunSolver` 及其子进程（实际求解器）绑定到该核心 ID。
* 任务结束后，必须将该核心 ID **归还 (Return)** 给资源池，供下一个任务使用。


* **超线程处理策略**：
* *建议（非强制）*：在文档中提示用户，SAT 求解通常建议绑定物理核心而非逻辑核心（Hyper-threading cores），以避免流水线资源争夺。



###3.2 任务调度与执行 (Execution Flow)* **命令构建链 (Command Chain)**：
为了实现资源限制和 CPU 绑定，实际执行的命令行结构应呈现“洋葱式”包裹：
```text
[最外层] CPU绑定 -> [中间层] 资源限制 -> [最内层] 实际求解器

```


* **格式范例**：
`taskset -c <Core_ID> runsolver [RunSolver参数] -- [求解器路径] [求解器参数]`


* **执行与监控**：
* 工具负责拼接上述命令。
* 阻塞等待命令结束。
* 确保所有子进程（包括 `RunSolver` 和求解器）都运行在同一个指定的 CPU 核心上（`taskset` 的特性是子进程继承父进程的亲和性）。



###3.3 输入管理 (Input Management)* **混合输入源**：
* 支持命令行一次性输入多个文件路径和文件夹路径。
* 递归扫描文件夹中的 `.cnf` 文件。


* **算例集名称自动提取**：
* 逻辑保持不变：取文件所在的**直接父文件夹名称**作为 Benchmark Set Name。
* *Input*: `/data/SAT_Comp_2023/Main_Track/huge.cnf`
* *Set Name*: `Main_Track`



###3.4 输出与归档系统 (Output & Archiving)严格遵守以下层级结构，**不做任何妥协**：

* **根目录结构**：
```text
<Output_Root>/
├── <Benchmark_Set_Name>/       <-- 算例集
│   ├── <Solver_Name>/          <-- 求解器
│       ├── <Instance_Name>.log      <-- 求解器 stdout
│       ├── <Instance_Name>.err      <-- 求解器 stderr
│       └── <Instance_Name>.watcher  <-- RunSolver 监控数据

```


* **索引文件**：
* 在 `<Output_Root>` 下生成 `index.csv`。
* 新增字段 `CPU_Core_ID`，记录该次运行实际绑定的核心编号，便于事后排查硬件差异。



###3.5 配置管理 (Configuration)支持 JSON 配置文件定义求解器：

* **配置项**：
* `name`: 求解器唯一标识。
* `command`: 命令行模板（如 `/bin/kissat --time={timeout} {input}`）。
* *(可选)* `no_pinning`: 布尔值，如果某些特殊求解器自带多线程并行逻辑且不希望被外部绑定单核，可豁免绑定（一般情况默认为 False）。



---

##4. 界面与交互设计 (Interface Design)###4.1 命令行参数规范 (CLI Specs)新增了控制 CPU 的参数：

| 参数 | 长参数 | 简写 | 说明 | 示例 |
| --- | --- | --- | --- | --- |
| `--input` | `--input` | `-i` | (多值) 输入文件或目录 | `-i ./data` |
| `--output` | `--output` | `-o` | 输出根目录 | `-o ./results` |
| `--cores` | `--cores` | `-C` | **[重要]** 指定可用 CPU 核心列表 (逗号分隔或范围) | `-C 0-7` 或 `-C 1,3,5,7` |
| `--jobs` | `--jobs` | `-j` | 期望并发数 (受限于核心数) | `-j 4` |
| `--config` | `--config` | `-c` | 求解器配置文件 | `-c solvers.json` |
| `--timeout` | `--timeout` | `-t` | 超时时间 (秒) | `-t 3600` |
| `--mem` | `--mem-limit` | `-m` | 内存限制 (MB) | `-m 16384` |

###4.2 交互反馈* **启动检查**：程序启动时，应打印检测到的 CPU 核心数以及用户允许使用的核心列表。
* **进度条**：显示 `[完成数 / 总任务数]` 以及当前负载 `Active Cores: 4/8`。

---

##5. 系统逻辑流程 (System Architecture Logic)1. **初始化阶段**：
* 解析 CLI 参数。
* 解析 CPU 列表字符串（如 `0-3,5` -> `[0, 1, 2, 3, 5]`）。
* 初始化 **SafeQueue<int> CorePool**，将所有可用核心 ID 入队。
* 加载 `solvers.json`。


2. **任务生成阶段**：
* 扫描输入路径，生成 `Job` 对象列表。
* 每个 `Job` 包含：`InputPath`, `BenchmarkSet`, `SolverName`, `CmdTemplate`。
* 准备好所有目标输出目录 `Output/Set/Solver/`。


3. **并发执行循环 (Thread Pool)**：
* 线程池大小 = 用户指定的可用核心数量。
* **Step 3.1: 资源申请**
* 从 `CorePool` 中 `pop()` 一个 `core_id`。如果池为空（理论上线程池大小限制了这种情况，但需防卫性编程），则阻塞等待。


* **Step 3.2: 命令构建**
* `Target_Cmd` = `taskset -c {core_id} runsolver ...`


* **Step 3.3: 执行**
* 调用 `std::system(Target_Cmd)`。
* 此时，操作系统保证该进程及其子进程只会在 `core_id` 上运行。


* **Step 3.4: 资源释放**
* 执行完毕，将 `core_id` `push()` 回 `CorePool`。
* 通知主线程更新进度条。




4. **收尾阶段**：
* 等待所有线程完成。
* 关闭 `index.csv` 文件句柄。



---

##6. 非功能性需求 (Non-functional Requirements)1. **性能损耗最小化**：
* 主控程序必须极其轻量。除了分配任务和文件 IO 外，不应占用任何计算资源，以免抢占被绑定的核心。


2. **平台依赖性**：
* **Linux Only**。由于依赖 `taskset` (util-linux) 和 `RunSolver`，本工具仅支持 Linux 环境。代码中应添加 `#ifdef __linux__` 检查，非 Linux 环境直接报错退出。


3. **异常安全 (Exception Safety)**：
* 如果 `std::system` 调用失败或被信号中断（Ctrl+C），必须保证 **RAII** 机制能够将借出的 CPU 核心归还到池中，避免核心资源泄漏导致死锁（即后续任务永远等待不到核心）。



##7. 技术实现建议 (Technical Notes)* **进程绑定方式**：
虽然 C++ 有 `pthread_setaffinity_np`，但考虑到我们要绑定的是通过 `system()` 或 `exec()` 启动的**子进程**，最稳健且侵入性最小的方法是在命令行前拼接 `taskset -c <id>`。这符合“黑盒”设计原则。
* **并发模型**：
推荐使用 **Producer-Consumer** 模型。主线程是 Producer（生产任务），工作线程是 Consumer（消费核心资源并执行）。
* **核心列表解析**：
需要编写一个简单的解析器处理 `0-3,5,7-9` 这种格式的字符串，转化为 `std::vector<int>`。