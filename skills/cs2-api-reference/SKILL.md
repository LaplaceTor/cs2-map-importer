---
name: cs2-api-reference
description: >-
  Use this skill as a comprehensive API dictionary and module reference for Core, Domain, Workflow, and Application layers in cs2-map-importer. Consult when looking up available classes, services, parsers, and utilities.
---

# CS2 Map Importer — 分层 API 架构参考字典

本指南按架构分层归纳了项目各层已实现的核心类、服务接口、解析器与基础设施。

---

## 1. 日志系统 API (`Core::Logging`)

项目采用任务导向日志系统（Task-Oriented Logging）。UI 层仅经 `Application::Logging` 门面消费日志（见 §5）；本节为 Core 侧设施。

### 1.1 核心调用范式

* **常规业务任务**：
  ```cpp
  auto task = Core::Logging::LogManager::instance()
                  .createTask(QStringLiteral("导入模型"));

  task->start();
  task->info(QStringLiteral("开始处理"));
  task->updateProgress(0.5, QStringLiteral("转换中"));
  task->complete(QStringLiteral("处理完成"));
  ```
* **Workflow 根任务与独立日志目录**：
  ```cpp
  // 自动创建 logs/<workflowName>_<timestamp>/ 目录与 workflow.log
  auto wfTask = Core::Logging::LogManager::instance()
                    .createWorkflowTask(QStringLiteral("Particle Import"), QStringLiteral("fire"));
  ```
* **外部 CLI 工具任务 (Tool Task)**：
  ```cpp
  // 挂载至父任务，UI 主树静默隐藏，父任务记录 [EXEC]，工具日志独立落盘
  auto toolTask = Core::Logging::LogManager::instance()
                      .createToolTask(stageTaskId, toolCommandLine, assetBaseName);
  ```

### 1.2 核心类与设施
* `LogManager`：集中管理全局任务注册表、层级树、Sink 分发与 Flush。任务创建入口仅有 `createTask`（含显式 id 重载）、`createWorkflowTask`、`createChildTask`、`createToolTask`。生命周期契约：`cancelTask` 级联取消全部子孙任务（BFS，环防护）；终态转移丢弃对应 sink 块游标；`forceTaskState` 幂等；`clear()` 不重置任务 id 计数（跨会话唯一，防迟到报告误投递）；故障路径 `reportFault` / `beginFaultDraining` / `terminateAfterFault`；块级只读检视 API（`getSealedBlocks` / `getAllBlocks` / `taskSnapshots`）；
* `TaskLoggingContext`：单任务上下文句柄，提供 `info` / `warning` / `error` / `command`、进度汇报与 `forceTerminalState`（执行仲裁覆盖入口）；**任务创建方法（`createTask` / `createChildTask` / `createToolTask`）仅在 `LogManager` 上，TaskLoggingContext 没有这些方法**；
* `LogFileManager`：负责任务日志路径生成、文件名清洗（Windows 安全名）与 Workflow/Tool 独立日志路径推导；同名冲突自动追加 `_2`、`_3` 序号；
* `TaskFileSink`：实现 `ILogSink`，负责各任务日志文件的即时创建、增量追加写入与优雅关闭；
* `ApplicationLogger`：应用级日志静态入口（debug/info/warning/error），落盘 `application_<yyyyMMdd_HHmmss_zzz>.log`；`ApplicationLogSink` / `FileSink` / `Logger` 为配套 Sink 设施；
* `FaultBarrier`：致命故障屏障（与 `LogManager::reportFault` 配套的日志排空与终止流程）；
* `TaskRunContext` / `TaskSnapshot`：任务运行上下文与只读快照。

### 1.3 日志级别契约
* `error()`：记录错误日志并累加任务错误计数，供终态仲裁参考（**不直接改变任务状态**；`Failed` 终态由 `fail()` 或 `AsyncTaskRunner` 仲裁落地）。
* `reportFault()`：致命故障屏障入口（返回 `LogSubmissionResult`，触发排空与终止流程）。
* `warning()`：可恢复问题、降级处理或跳过（不影响成功终态）。
* `info()`：面向用户的宏观阶段里程碑。
* `debug()`：技术诊断与内部步骤追踪。

---

## 2. Core 层 API 参考 (`cs2importer_core`)

通用底层基础设施，无 Valve/CS2 业务策略。

* **`Core::Async`**：
  * `CancellationToken`：基于原子共享标志的协作式取消令牌，显式在调用链间按值拷贝传递，支持 `cancel()` 与 `isCancelled()`。
* **`Core::Path`**：
  * `FilesystemPath`：标准化跨平台宿主文件系统路径抽象与操作；
  * `PathUtils`：通用路径规范化、扩展名提取与安全文件名过滤。
* **`Core::KeyValues`**：
  * `KeyValuesDocument` / `KeyValuesNode` / `KeyValuesParser` / `KeyValuesWriter`：通用 Valve KeyValues/VDF AST 解析与序列化器，支持无引号 Token、嵌套节点、同名兄弟节点、保序输出及原子写入。
* **`Core::FileSystem`**：
  * `FileSystem`：通用文件系统辅助工具（`move()` 部分完成语义：目标副本成功但源删除失败时保留目标副本并抛出结构化异常上报）；
  * `AtomicFile`：基于临时文件重命名的原子落盘写入；
  * `DirectorySnapshot`：目录递归快照；
  * `FileLease`：RAII 移动语义的文件排他锁/租约机制。
* **`Core::Process`**：
  * `ProcessRunner`：封装外部控制台进程调用，支持实时逐行流式重定向（`onStdOutLine` / `onStdErrLine`）、超时控制、取消令牌响应；
  * `ProcessOptions`：进程执行配置参数，集成流式回调、超时与 `CancellationToken`；
  * `ProcessResult`：进程退出码、stdout、stderr 及机械状态载体，提供 `toError()` / `toErrorCode()` 映射。
* **`Core::Temp`**：
  * `TempFile` / `TempDirectory`：RAII 临时资源生命周期管理，析构时自动清理。
* **`Core::Error`**：
  * `ErrorCode`：跨项目通用底层系统/设施错误码枚举；
  * `Error`：机器可解析的结构化错误值对象（包含 code, message, details, 扩展领域码）；枚举匹配必须走**带域匹配** `error.is(domainName, code)`（同时校验域名与码值，防止不同领域枚举数值混判）；
  * `Exception`：跨线程异常传输载体（派生自 `std::exception` / `QException`）。
* **`Core::Result<T>`**：
  * 标准业务结果承载类型（`[[nodiscard]]`），原生支持 `Success`, `Failure`, `Cancelled`, `Skipped` 四态及 `Core::Error`；`failure` / `cancelled` / `skipped` 均提供带 `partialValue` 的重载，用于契约冲突仲裁时的部分业务负载保全。

---

## 3. Domain 层 API 参考 (`cs2importer_domain`)

封装 Valve / Source 业务规则与确定性领域资产解析，仅依赖 Core 层。

* **`Domain::Asset`**：
  * `AssetPath`：Valve 相对资产路径（如 `materials/models/...`）；
  * `AssetTypeDetector`：基于文件名与后缀的资产类型判别。
* **`Domain::Package`**：
  * `PackArchive`：基于 `sourcepp` (vpkpp/bsppp) 的统一资产包抽象，直接在进程内读取/枚举/提取 VPK 与 BSP 嵌入包内容；
  * `BspPackExtractor`：专职从 Source 1 BSP 文件提取嵌入的 Pakfile 资产包；
  * `PackArchivePool`：归档池化缓存机制（LRU 缓存），复用打开的文件句柄，提供线程安全的高性能解包。已彻底废弃外部 VPKEdit CLI。
* **`Domain::Material`**：
  * `VtfConverter`：基于 `vtfpp` 的 VTF 纹理图像解码器，支持转码导出为 PNG、TGA、JPG、BMP、HDR 格式（内存缓冲区或直接落盘）。
* **`Domain::Audio`**：
  * `SoundscapeDefinition`：Source 1 声音景观领域模型（looping sound, random sound 等）；
  * `SoundscapeParser`：解析 `soundscapes_*.txt` VDF 脚本；
  * `SoundscapeToSoundEventConverter`：转为 Source 2 音效事件配置；
  * `SoundEventKv3Writer`：序列化生成 Source 2 KeyValues3 (`.vsndevts`) 格式；
  * `SoundLevelMapper` & `DspPresetRegistry`：声音分贝等级映射与 DSP 空间预设注册表。
* **`Domain::Tool`**：
  * `ResourceCompilerTool`：Valve CS2 官方资源编译器（`resourcecompiler.exe`）强类型调用封装；
  * `Source1ImportTool`：Valve CS2 官方导入工具（`source1import.exe`）强类型调用封装（`Source1ImportOptions::workingDirectory` 指定工具工作目录，空则继承调用方 cwd）；
  * `ResourceCompilerLogParser` / `Source1ImportLogParser`：编译器输出的高性能日志解析器，签名 `parse(stdOut, stdErr = {}, exitCode = 0, workingDirectory = {})`——stdout 中的相对产物路径按工具工作目录解析；stderr 普通行一律记为 `warnings`（仅显式 `WARNING:` 行被跟踪），非零退出且无显式错误行时将最后一条有意义的 stderr/stdout 行晋升为错误以保证失败有具体原因；
  * `Cs2PathLayout`：推导 CS2 content 树与 game 树分离的标准资产布局规范；
  * `ToolErrors` (`ToolErrorCode`)：工具专属强类型领域错误模型，提供 `ToolErrors::is(error, code)` 类型安全匹配器。
* **`Domain::Game`**：
  * `GameType` / `EngineType`：游戏类型与引擎类型强类型枚举；
  * `GameDefinition` / `GameRegistry`：元数据驱动的游戏定义与注册表；
  * `GameInfo` / `GameInfoParser`：`gameinfo.gi` 与 `gameinfo.txt` 结构化解析；
  * `SearchTarget` / `SearchPathResolver`：搜索路径多层级解析推导；
  * `GameValidator`：游戏安装目录确定性校验器；
  * `GameErrors` (`GameErrorCode`)：游戏领域强类型错误模型，提供 `GameErrors::is(error, code)` 类型安全匹配器；
  * `GameInstallationResolver` / `SteamGameLocator`：游戏安装信息解析与 Steam 定位的纯领域逻辑。

---

## 4. Workflow 层 API 参考 (`cs2importer_workflow`)

实现具体资产导入用例与流水线编排，依赖 Domain 与 Core，严禁依赖 UI / Application。

* **`Workflow::Common`**：
  * `ImportContext`：组合 `Core::Logging::TaskLoggingContext*` 与 `Core::Async::CancellationToken`，提供统一的任务日志、进度汇报与取消状态检查（`checkCancelled()`）；
  * `AssetExtractor`：按 `SearchTarget` 列表定位并提取资产（目录松散文件 → 目标 `pak01_dir.vpk` → VPK 目标），结合 `PackArchivePool` 进行归档复用；
  * `BspEmbeddedExtractor`：经 `Domain::Package::PackArchive` 与 `BspPackExtractor` 枚举并提取 BSP 内部嵌入资产；
  * `VtfExtractor`：组合 `AssetExtractor` 与 `Domain::Material::VtfConverter`，按指定格式提取并转码 VTF。
* **`Workflow::Particle`**：
  * `ParticleImportWorkflow`：Source 1 `.pcf` 到 Source 2 `.vpcf` 的完整导入工作流，编排依赖提取、content 目录资产生成与 `Domain::Tool::ResourceCompilerTool` 编译；以 `Cs2PathLayout::gameDirectory` 作为两个工具的工作目录锚点（保证 stdout 相对产物路径可解析）；**产物生命周期归属工作流**——编译步骤被取消或失败时清理半成品 `.vpcf` / `.vpcf_c`（`cleanupGeneratedArtifacts`）；
  * `ParticleImportOptions`：粒子导入配置参数（深度混合、禁用漫反射、`toolTimeoutMs` 单工具超时覆盖（0 = 沿用各工具默认 120s）等）。

---

## 5. Application 层 API 参考 (`cs2importer_application`)

应用服务、业务任务编排与生命周期管理，向 UI 暴露轻量 DTO 契约。

* **异步执行基础设施 (`Application::Async`)**：
  * `AsyncTaskRunner`：三平面异步任务调度器，统一处理生命周期转移、异常安全转译与回调投递；入口含 `runTask` / `runChildTask` / `runBackground` / `runWorkflowTask`（创建工作流日志目录并注册 Workflow 根任务）/ `runSystemTask`（系统任务，taskId=0）；
  * `SystemTaskLog` (`Async/SystemTaskLog.h`)：系统任务日志上下文（debug/info/warning/error），仅并入应用日志 `application_<timestamp>.log`（`[TaskName]` 前缀），线程安全；Worker 签名 `(const SystemTaskLog&, [CancellationToken])`，必须返回 `Result<T>`；
  * `TaskHandle`：异步任务生命周期句柄（`[[nodiscard]]`），配合 `CancellationToken` 支持协作式取消；系统任务句柄 taskId=0，`cancel()` 仅触发令牌；
  * `ExecutionGuard` (`Application::Execution`)：在应用服务边界安全捕获异常并转译为 `Result<T>::failure`。
* **通用导入前置服务 (`Application::Common`)**：
  * `ImportPrerequisiteService`：Map、Model、Particle 导入共用的前置保障服务，校验基础参数（`BaseImportRequest`）并在必要时独占获取 CS2 `vpk.signatures` 文件租约。
* **环境与检测服务 (`Application::Environment`)**：
  * `SteamService`：Steam 安装目录与库探测，读取 App Manifest；
  * `GameInstallation` / `GameInstallationInfo`：探测到的游戏安装应用层数据表示；
  * `GameDetectService` / `GameEnvironmentService`：游戏探测与环境校验编排；探测/校验异步入口（`detectEnvironmentAsync`、`validateSource1/2FolderAsync`）走 `runSystemTask` 系统任务平面（日志参数为 `const SystemTaskLog*`，不再是 `TaskLoggingContext`）；`GameEnvironmentService::listSource2AddonsAsync` 异步列举 S2 插件目录（同步重载保留供内部使用）；
  * `GameInstallationValidator`：安装信息与目录结构校验编排；
  * `VpkSignatureLeaseService`：CS2 `vpk.signatures` 排他性租约策略服务。
* **日志投递门面 (`Application::Logging`)**：
  * `TaskLogService`：**UI 消费日志的唯一通道**——以私有 `SinkBridge`（`Core::Logging::ILogSink`）桥接 `LogManager`，将密封 `LogBlock` 转为 UI DTO，经队列信号 `logBatchReceived(subscriptionId, taskId, taskName, QVector<TaskLogMessage>)` 投递；API：`subscribe()` / `unsubscribe(id)`（订阅制陈旧批次抑制；无订阅者时跳过 DTO 转换）、`taskInfo(taskId)`（未知 id 返回 `isValid==false` 的空 `TaskInfo`）、`applicationLogFilePath()` / `expectedApplicationLogFilePath()`、`logsDirectory()` / `ensureLogsDirectory()`、`fallbackTaskLogFilePath(...)`；
  * `TaskLogDTOs`：UI 侧值类型——`TaskState` / `LogLevel` 枚举（**UI 代码一律使用此层枚举，禁止使用 Core 侧枚举**）、`taskStateToString` / `logLevelToString`、`TaskLogMessage{sequence, timestamp, level, message, toolTaskId}`、`TaskInfo{taskId, parentTaskId, startTimestamp, taskName, state, progress, currentMessage, isToolTask, logFilePath, workflowDirectory, isValid}`；
  * **红线**：`src/UI/` 严禁 include `Core/Logging/*`，日志一律经本门面获取。
* **专项业务服务**：
  * `ParticleImportService` (`Application::Particle`)：粒子导入高层业务编排服务，对外暴露面向 UI 的 DTO 契约（`ParticleImportRequest`, `ParticleImportResult`）；签名 `importParticlesAsync(request, callback)`（无 loggingCtx 参数），恒经 `runWorkflowTask` 执行；并发导入快速失败（第二个调用经回调返回 `InvalidState`，句柄无效）；`enable_shared_from_this` 保证任务期间服务存活；
  * `SoundscapeConvertService` (`Application::Soundscape`)：声音景观批量转换服务，将 Source 1 脚本转为 CS2 KV3 音效事件文件；签名 `convertMapSoundscapesAsync(request, callback)`（无 loggingCtx 参数），经 `runWorkflowTask` 执行（以地图名为资产基名，拥有独立工作流日志目录与可见任务树节点）。

---

## 6. Presentation / UI 层 API 参考 (`cs2importer_ui`)

为 QML 界面提供数据绑定模型与交互控制器，消费 Application 层门面与 DTO。

* **日志模型与视图模型 (`UI::ViewModels`)**：
  * `LogViewModel`：集中管理面向界面的任务树平铺投影、树节点动态增删、同级排他手风琴折叠（`toggleTaskExpanded` / `toggleTaskExpandedById`）、Tool 任务专项提取（`getToolMessagesModel` / `getToolTaskState` / `getToolTaskLogFilePath`）；构造注入 `Application::Logging::TaskLogService*`，经 `attachToLogService()` / `detachFromLogService()` 挂接门面，以订阅 id 抑制陈旧批次（严禁 include `Core/Logging`）；展开默认规则：根任务展开、子任务收起，无任何"完成触发自动折叠"行为；
  * `LogTaskModel`：层次化任务项列表模型（`LogViewModel` 的基类，亦作为每节点 `subTasksModel` 使用），角色含 `ParentTaskIdRole`, `DepthRole`, `TaskNameRole`, `StateRole`, `StateStringRole`, `ProgressRole`, `CurrentMessageRole`, `ExpandedRole`, `MessageCountRole`, `SubTasksCountRole`, `HasSubTasksRole`, `MessagesModelRole`, `SubTasksModelRole` 等（**不含 ToolTaskIdRole**，该角色在 LogMessageListModel 上）；
  * `LogMessageListModel`：单任务内部日志条目列表模型（包含 `MessageRole`, `LevelStringRole`, `ToolTaskIdRole` 等；`level` 为 `Application::Logging::LogLevel`）；
  * `GameViewModel`：游戏检测与路径选择状态绑定 ViewModel；`refreshS2Addons()` 异步列举插件（经 `listSource2AddonsAsync`，带过期结果丢弃守卫）；VPK 租约（`updateVpkLease` / `retryVpkLease`）为**有意同步**的 UI 线程调用（单次 Win32 排他文件打开，结果经 `vpkLeaseStatusChanged` 信号上报，严禁在 Worker 线程调用）。
* **控制器与交互门面 (`UI::Controllers`)**：
  * `MainController`：主窗口业务编排中枢，聚合 Application 服务与 `LogViewModel`（无独立 Tab 控制器层）；提供 `startImport` / `startParticleImport`（启动前全局 `collapseAll()`）、`stopImport`、`cancelAllOperations()`（组合根关停时先于线程池清理调用）、`setActiveTab(int)` 公共槽（QML TabBar 直连，带 isProcessing 守卫）。
* **QML 专用日志视窗与组件 (`src/qml/cs2importer/`)**：
  * `LogWindow.qml`：宏观导入工作流与任务卡片列表窗口（集成任务树平铺与平滑滚动）；
  * `ToolLogWindow.qml`：专用外部 CLI 工具（如 resourcecompiler）独立控制台实时日志窗口；窗口可见且任务未达终态期间以 250ms 定时器轮询 `getToolMessagesModel` / `getToolTaskState` / `getToolTaskLogFilePath`（弥合隐藏工具任务节点首次日志块到达才投影的时序差）；
  * `components/LogTaskCard.qml`：支持层次缩进与手风琴折叠交互的任务卡片组件。

---

## 7. 测试工程参考 (`tests/`)

测试代码库严格按生命周期隔离：

* **常驻单元测试 (`test_core_*`)**：
  * 目标可执行文件：`test_core_logging`；
  * 链接契约：`PRIVATE cs2importer_core Qt6::Core Qt6::Test`；
  * 范围：仅测试 Core 层基础设施（`LogManager`, `TaskLoggingContext`, `TaskFileSink`, `LogFileManager`, `ProcessRunner`, `Result`, `Error`）。
* **临时单任务测试（Task-Scoped / Ephemeral Tests）**：
  * 任何针对 Domain、Workflow、Application、UI 层的单元或集成测试，仅在对应特性研发任务中临时存在；
  * **用完即删**，严禁合入主线，严禁在 `tests/` 下建立对上层模块的永久性 CMake 链接。

