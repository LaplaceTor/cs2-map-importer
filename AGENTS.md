# CS2 Map Importer — Agent 开发指导规范

## 0. 目标与优先级

本文档是参与本仓库开发的所有 **AI Agent 及人类贡献者必须遵守的架构契约**。

本项目正处于从遗留单体 Qt 应用程序向严格分层架构重构的演进过程中。最核心规则为：

> **切勿仅仅将文件放置在对应的目录中。代码的依赖关系图与职责划分必须严格遵守以下分层规则。**

当本规范与局部的“快捷实现方式”发生冲突时，必须坚守架构契约，通过适配器/门面（Adapter/Facade）保持既有行为，严禁跨层违规调用。

### 核心不可违背原则

1. 表现层/UI 只能调用 **Application 层**。
2. Application 层可编排 **Workflow、Domain 与 Core 层**。
3. Workflow 层可调用 **Domain 与 Core 层**，但绝不可依赖 UI 或 Application 层。
4. Domain 层只能调用 **Core 层**，绝不可依赖 UI、Application 或 Workflow 层。
5. Core 层为通用可复用基础设施，严禁包含任何 Valve 业务规则、导入工作流、Application 服务或 QML 逻辑。
6. 任何层均不得为了编码便利而跳过下层直接调用更底层/上层实现。
7. **目录名本身并不代表架构边界，真正的边界在于 include / link / 依赖拓扑图。**

---

## 1. 项目概览

用于将 Source 1 游戏资产（地图、模型、粒子、音频等）导入至 Counter-Strike 2 的 Windows 桌面 GUI 应用程序。

* **开发语言：** C++20
* **技术框架：** Qt 6.8+
* **构建系统：** 现代 CMake (3.28+)
* **目标平台：** **仅限 Windows**（程序仅支持 Windows 平台构建、编译与运行；代码库严禁保留或新增对 Linux / macOS 等非 Windows 平台的兼容代码、多平台宏守卫或条件分支）
* **UI 技术：** QML / Qt Quick Controls 2 (Fusion 样式)

---

## 2. 目标分层架构

```text
┌──────────────────────────────────────────────────────────────┐
│ Presentation / UI (表现层)                                   │
│ QML 视图 <-> ViewModels / Controllers                        │
│ 允许: Qt/QML + Application 契约                              │
│ 禁止: 直接编排 Domain/Core                                   │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用 / 信号槽连接
┌─────────────────────────────▼────────────────────────────────┐
│ Application (应用层)                                         │
│ 服务 / 任务编排 / 配置 / 自动更新 / 环境感知                  │
│ 将 UI 契约转换为 Domain/Workflow 输入                        │
│ 负责异步调度、生命周期控制及面向 UI 的结果封装               │
└─────────────────────────────┬────────────────────────────────┘
                              │ 触发 / 组装
┌─────────────────────────────▼────────────────────────────────┐
│ Workflow (工作流层)                                          │
│ 具体导入用例 / 流水线编排 / 取消响应                         │
│ 可使用 Domain + Core                                         │
│ 严禁依赖 UI 或 Application                                   │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用
┌─────────────────────────────▼────────────────────────────────┐
│ Domain (领域层)                                              │
│ Valve / Source 1/2 业务规则、领域模型、解析器、处理器、工具  │
│ 仅可调用 Core，无 UI、Application、Workflow 依赖             │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用底层设施
┌─────────────────────────────▼────────────────────────────────┐
│ Core (基础设施层)                                            │
│ 路径 / 文件系统 / KeyValues / 进程 / 日志 / 临时文件 / 错误 │
│ 无业务逻辑 / 无 Valve 专用策略 / 无 UI                       │
└──────────────────────────────────────────────────────────────┘
```

### 规范依赖流向

```text
Presentation → Application → Workflow → Domain → Core
```

Application 亦可直接调用 Domain/Core 提供的非工作流服务，但 **UI 层严禁跨层直接调用**。

严禁出现**逆向依赖**：
* Core ✗→ Domain / Workflow / Application / UI
* Domain ✗→ Workflow / Application / UI
* Workflow ✗→ Application / UI
* Application ✗→ UI

---

## 3. Agent 强制分层规则

### 3.1 表现层 / UI 规则 (`src/UI/`, `src/qml/`)

* **允许：** 暴露 `Q_PROPERTY`、Qt 信号/槽；校验基础界面输入；调用 Application 服务/门面；将 UI 数据转换为 Application 请求 DTO；展示结果与错误。
* **严禁：** 直接 include `Domain/*` 或 `Core/*` 执行业务操作；调用 Domain 校验器/解析器/处理器；调用进程/文件系统执行业务；扫描游戏目录/Steam；拥有工作流线程或取消令牌；直接创建 `QProcess` 或弹窗；include `Core/Logging/*` 或以任何形式直接消费 `Core::Logging::LogManager`/日志文件（**日志唯一通道为 Application 的 `Application::Logging::TaskLogService` 门面及其 DTO**）。

### 3.2 Application 规则 (`src/Application/`)

* **允许：** 暴露面向 UI 的门面/服务 API；将 UI 契约转换为 Domain/Workflow 输入；编排 `AsyncTaskRunner`、Worker 线程池与工作流；持有 `Application::Logging::TaskLogService` 日志门面（订阅制 DTO 分发与日志路径查询，UI 消费日志的唯一通道）；统一管理应用配置、更新与环境检测（Steam 探测、文件租约）；提供弹窗交互抽象接口的具体实现。
* **严禁：** 包含 QML 或直接操作 UI 控件；实现属于 Domain 的数据格式解析或转换；包含属于 Workflow 的具体导入流水线；在已有契约时向 QML 暴露底层 AST/指针细节。

### 3.3 Workflow 规则 (`src/Workflow/`)

* **允许：** 定义具体导入流水线（如 `ParticleImportWorkflow`）；使用 `ImportContext` 处理取消与进度；按序调用 Domain 处理器与工具；返回 `Core::Result<T>` 表达单层业务结果。
* **严禁：** include 或调用 UI / QML；依赖 Application 策略或全局配置；直接弹出交互对话框；自行发现 Steam 或扫描全局环境。

### 3.4 Domain 规则 (`src/Domain/`)

* **允许：** 解析与校验 Valve 专属数据格式；抽象 Source 1/2 领域资产与相对路径；封装官方 CLI 工具（`Domain::Tool`）；保持确定性与无状态纯计算。
* **严禁：** include `Application/*`、`Workflow/*`、`UI/*` 或 QML 头文件；发送 UI 通知或弹窗；访问应用全局配置或日志器；自行启动线程。

### 3.5 Core 规则 (`src/Core/`)

* **严禁包含：** 游戏定义与 CS2/CSGO/HL2 专有规则；导入工作流决策；Steam 探测逻辑；VPK 业务策略；材质转码逻辑；UI / QML 代码；Application 服务。Core 必须保持通用性，可无缝脱离本项目复用。

---

## 4. UI 与 Application 边界契约

面向 UI 的 Application API 应优先使用 **Application 自有 DTO/值对象** 或基础 Qt 值类型，避免要求 UI 代码构造 Domain/Core 内部实现类型。

```text
UI 字符串/路径
   ↓
Application 请求 DTO
   ↓
Application 解析为 GameType / FilesystemPath
   ↓
Domain 校验器执行
   ↓
Application 结果 DTO
   ↓
UI 属性/信号
```

日志数据流同样单向：`Core::Logging`（采集/落盘）→ `Application::Logging`（`TaskLogService` 门面 + `TaskLogDTOs` DTO 转换）→ UI（订阅 `logBatchReceived` / 查询 `taskInfo`）。UI 严禁直接 include `Core/Logging/*`。

---

## 5. 核心异步、日志与错误处理原则

1. **阻塞操作绝不上 UI 线程**：文件扫描、大文件解析、包解压、工具编译与导入流水线均必须由 Application 调度在 Worker 线程执行。
2. **任务平面架构（三平面）**：
   - **任务执行生命周期平面 (`TaskState`)**：面向用户的工作流任务由 `LogManager::createWorkflowTask` / `createTask` / `createToolTask` 注册，经 `TaskLoggingContext` 管理状态流转（`Pending → Running → Completed | Failed | Cancelled | Skipped`），在 UI 任务树可见；
   - **系统任务平面 (`SystemTaskLog`)**：环境检测、安装校验、插件列举等非导入后台任务必须经 `AsyncTaskRunner::runSystemTask` 执行——无 LogManager 任务、无 `TaskState`、不进任务树（taskId 恒为 0），日志经 `Application::Async::SystemTaskLog` 并入 `application_<timestamp>.log`（`[TaskName]` 前缀）；
   - **业务执行结果平面 (`Result<T>`)**：单层承载业务数据与结构化错误。
3. **单层 Result 契约**：严禁嵌套 `Result<Result<T>>`。严禁基于异常进行常规业务控制流，访问 `result.value()` 前必须通过 `isSuccess()` 检查。
4. **三层诊断分层**：
   - 操作总结 (`Result::message()`)：面向用户的宏观操作概括；
   - 失败原因 (`Error::message()`)：具体领域或系统失败事实；
   - 技术诊断 (`Error::details()`)：绝对路径、CLI 参数、stderr 等技术细节。
5. **任务导向日志**：严禁使用全局静态 Logger（如 `Logger::info(...)`）。工作流/工具任务必须通过 `TaskLoggingContext` 显式向下传递；系统任务使用 `SystemTaskLog`（见第 2 条）。
   - **层级化与工作流任务**：顶层导入流程通过 `LogManager::createWorkflowTask` 创建 Workflow 根任务，在 `logs/<workflowName>_<timestamp>/` 下生成独立目录与主工作流日志 `workflow.log`；
   - **外部工具隐藏任务（Tool Task）**：外部 CLI 工具（如 `resourcecompiler`, `source1import`, `bspsrc`）必须通过 `LogManager::createToolTask` 创建。Tool 任务从主 UI 任务树中隐蔽（避免日志噪音），父任务接收携带 `toolTaskId` 的 `[EXEC]` 启动通知；工具输出实时流式写入独立文件（`<asset>_<tool>_<timestamp>.log`，位于父任务目录下，同毫秒冲突自动追加 `_2` 序号），UI 表现层通过独立 `ToolLogWindow` 按需查看。
6. **异常边界转译**：Application 服务边界统一通过 `ExecutionGuard` 或 `AsyncTaskRunner` 将异常转译为 `Result<T>::failure`，严禁在内部 helper 中静默使用 `catch (...)` 吞没异常。

> 💡 **详细规范与完整决策表**：请查阅专用技能 [`skills/cs2-async-error-handling/SKILL.md`](file:///c:/Users/KEY/Documents/GitHub/cs2-map-importer/skills/cs2-async-error-handling/SKILL.md) 获取三平面任务体系、终态冲突仲裁矩阵、构造正反模式代码及进程机械结果转译规则。

---

## 6. 系统交互与外部工具规范

1. **禁止直接操作外部进程**：业务代码严禁使用 `QProcess`、`system()`、`popen()` 或 `WinExec()`。
2. **外部工具强类型封装**：所有外部 CLI 工具（`bspsrc`，官方 `resourcecompiler`, `source1import`）必须封装于 `Domain::Tool` 并通过 `Core::Process::ProcessRunner` 执行。
3. **流式输出与取消绑定**：`ProcessRunner` 必须支持基于 `onStdOutLine` / `onStdErrLine` 的逐行实时流式日志捕获，并与 `CancellationToken` 强绑定，严禁无超时的静默阻塞式黑盒调用。
4. **内嵌原生库替代**：严禁再引入或调用外部 `vpkeditcli` 与 `vtfcmd`，归档解包与 VTF 转码已全量由内嵌原生库（`Domain::Package` 与 `Domain::Material`，基于 `sourcepp`）在进程内完成。
5. **用户交互解耦**：Domain / Workflow 严禁直接弹出模态对话框。必须通过抽象 Prompt 接口定义契约，由 Application 实现并调度 UI 呈现。

---

## 7. 目标目录结构

```text
src/
├── Core/             # 通用基础设施 (Async, Error, FileSystem, KeyValues, Logging, Path, Process, Result, Temp)【全部已有】
├── Domain/           # Valve/Source 专有领域模型 (Asset, Audio, Game, Material, Package, Tool【已有】; Bsp, Vmf【规划】)
├── Workflow/         # 具体导入流水线 (Common, Particle【已有】; Map, Model【规划】)
├── Application/      # 应用服务与任务调度 (Async, Common, Environment, Execution, Logging, Particle, Soundscape【已有】; Config, Task, Update【规划】)
├── UI/               # 表现层 ViewModel 与控制器 (Controllers, ViewModels)【全部已有】
└── qml/              # QML 界面视图与组件 (cs2importer/components, cs2importer/tabs, Main.qml)【全部已有】
```

`src/Legacy/` 仅用于过渡，新代码严禁依赖 Legacy。

---

## 8. CMake 依赖强制规范

CMake 依赖必须严格映射架构单向依赖拓扑：

```text
cs2importer_core
    ↑
cs2importer_domain
    ↑
cs2importer_workflow
    ↑
cs2importer_application
    ↑
cs2importer_ui
    ↑
cs2importer (主程序 / QML)
```

### CMake 架构红线

* `cs2importer_core` 严禁链接 Domain / Application / UI；
* `cs2importer_domain` 仅链接 Core；
* `cs2importer_workflow` 链接 Domain + Core；
* `cs2importer_application` 链接 Workflow + Domain + Core；
* `cs2importer_ui` 链接 Application 及 Qt 模块。**严禁在 `src/UI/CMakeLists.txt` 中添加对 `cs2importer_domain` 或 `cs2importer_core` 的直接链接。**
* **测试链接红线**：`tests/` 下的常驻单元测试目标仅限针对 Core 层，仅允许链接 `cs2importer_core` 及 `Qt6::Core`、`Qt6::Test`；**严禁在常驻测试目标中链接 `cs2importer_domain`、`cs2importer_workflow`、`cs2importer_application` 或 `cs2importer_ui`**。

---

## 9. 编码与构建规范

* **C++ 标准**：C++20，适度使用 Qt 类型，严格遵循 RAII、值传递/移动语义与 `const` 正确性。
* **平台限制**：**仅限 Windows**。根目录 `CMakeLists.txt` 统一执行 `if(NOT WIN32)` 报错守卫；严禁添加或保留非 Windows 条件编译（`#ifdef Q_OS_WIN` 等）。
* **命名规范**：类名与枚举采用 `PascalCase`；函数、变量采用 `camelCase`。
* **第三方库**：置于 `third_party/`，由最底层实际消费模块 `PRIVATE` 链接，第三方类型严禁暴露在项目公共头文件中。
* **构建指令**：
  ```bash
  cmake -B build -S .
  cmake --build build --config Release
  ```
  或使用 Preset：
  ```bash
  cmake --preset windows-debug
  cmake --build --preset windows-debug
  ```

### 9.1 测试生命周期契约 (Testing Lifecycle Contract)

* **Core 层测试（长期常驻）**：作为系统可复用基础设施的质量底座，纯 Core 单元测试（`test_core_*`）长期驻留于 `tests/` 目录中，用于守护基础原语的向后兼容与确定性；
* **非 Core 层测试（面向单任务，用完即删）**：针对 Domain、Workflow、Application、UI 层的测试，均严格定义为**临时单任务测试（Task-Scoped / Ephemeral Tests）**。仅用于在研发、重构或定位缺陷的单个任务期间进行即时验证。**一旦任务完成，必须立即清理或删除，严禁将包含上层复杂依赖的测试长期留存在代码库中**。

---

## 10. 项目专属技能（Skills）快速索引

针对复杂、深入的专项开发工作，必须按需激活对应的专业 Skill：

| 任务类型 / 查阅需求 | 对应 Skill 路径 | 核心内容 |
| :--- | :--- | :--- |
| **异步调度、错误契约与诊断** | [`skills/cs2-async-error-handling/SKILL.md`](file:///c:/Users/KEY/Documents/GitHub/cs2-map-importer/skills/cs2-async-error-handling/SKILL.md) | 三平面任务体系（工作流/工具/系统任务）、仲裁矩阵、三层诊断规范、异常转译边界与进程机械结果映射。 |
| **分层 API 架构参考字典** | [`skills/cs2-api-reference/SKILL.md`](file:///c:/Users/KEY/Documents/GitHub/cs2-map-importer/skills/cs2-api-reference/SKILL.md) | Core、Domain、Workflow、Application 已实现的原语、服务类与接口字典。 |
| **架构审查、迁移计划与重构决策** | [`skills/cs2-architecture-review/SKILL.md`](file:///c:/Users/KEY/Documents/GitHub/cs2-map-importer/skills/cs2-architecture-review/SKILL.md) | 架构审查清单 (Checklist)、重构演进路线图、迁移对照表与红线异味清单。 |
| **Qt/QML/CMake 通用开发** | `third_party/agent-skills/skills/` | Qt C++ 规范、QML Review 与 UI 设计通用技能。 |

---

## 11. 绝对禁止事项与终极准则

* 严禁任何形式的跨层逆向调用（Core/Domain/Workflow 绝对不感知 Application/UI）。
* 严禁 UI 为了执行业务直接调用 Domain / Core 或在 UI CMake 中链接底层库。
* 严禁在 `tests/` 常驻测试目标中反向链接非 Core 模块，严禁将用完的非 Core 临时任务测试残留于主干代码库。
* 严禁为配置、服务、取消标志或日志器添加全局静态变量。
* 严禁在业务代码中直接调用 `QProcess`、`system()` 或 Shell 命令。
* 严禁从 Domain / Workflow 中弹出模态对话框。
* 严禁将“通过编译”等同于“架构设计正确”。

> **终极思考准则**：
> 正确的思考出发点不是：“这段代码写在哪里能让当前的构建通过？”
> 而是：“**哪个分层拥有该职责？跨越该边界的公开契约是什么？如何在不让任何层感知其上层的前提下优雅实现它？**”

