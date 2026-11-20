---
name: cs2-api-reference
description: >-
  API dictionary for cs2-map-importer across Core, Domain, Workflow, Application, and UI. Trigger when looking up existing class names, module locations, method signatures, or service contracts before writing or refactoring code.
---

# CS2 Map Importer — 分层 API 架构参考字典

按五层架构归纳各层已实现的核心深模块、服务契约与基础设施。架构依赖拓扑与单点防御规则见根目录 [`ARCHITECTURE.md`](../../ARCHITECTURE.md)。

---

## 1. 日志系统 API (`Core::Logging`)

项目采用任务导向日志系统（Task-Oriented Logging）。UI 层仅经 `Application::Logging::TaskLogService` 门面消费日志（见 §5）。

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
* **外部 CLI 工具子任务 (Tool Sub-Task)**：
  ```cpp
  // Domain::Tool 通过传入的 taskCtx 派生隐藏工具子任务，不直接依赖 LogManager::instance()
  auto toolTask = taskCtx->createToolSubTask(fullCommandLine, assetBaseName);
  // ... ProcessRunner 流式输出至 toolTask->logExternalToolOutput(...) ...
  toolTask->finishToolSubTask(QCoreApplication::translate("ResourceCompilerTool", "Compilation completed"));
  ```

### 1.2 核心类与设施
* `LogManager`：集中管理全局任务注册表、层级树、Sink 分发与 Flush。任务创建入口为 `createTask`（含显式 id 重载）、`createWorkflowTask`、`createChildTask`、`createToolTask`。`cancelTask` 级联取消子孙任务（BFS，环防护）；`forceTaskState` 幂等；`clear()` 不重置任务 id 计数；故障路径 `reportFault` / `beginFaultDraining` / `terminateAfterFault`；块级只读检视 `getSealedBlocks` / `getAllBlocks` / `taskSnapshots`。
* `TaskLoggingContext`：单任务上下文句柄，提供 `info` / `warning` / `error` / `command`、进度汇报、`logExternalToolOutput`（外部工具原始流式行直通，不加人工等级前缀，保留天然换行）、`forceTerminalState`（仲裁覆盖入口；进入 `Completed` 终态时强制刷新进度为 100%），以及**工具子任务派生与终态封装**（内部委托 `LogManager`，使 `Domain::Tool` 无需 include `LogManager.h`）：
  * `std::shared_ptr<TaskLoggingContext> createToolSubTask(const QString& commandLine, const QString& assetBaseName = QString())`
  * `bool finishToolSubTask(const QString& message = QString())`
  * `bool failToolSubTask(const QString& message = QString())`
  * `bool cancelToolSubTask(const QString& message = QString())`
* `LogFileManager`：任务日志路径生成、文件名清洗与 Workflow/Tool 独立日志路径推导；同名冲突自动追加 `_2`、`_3`。
* `TaskFileSink`：实现 `ILogSink`，即时创建与增量写入任务日志；常规行格式 `[LEVEL] %2`，首行写入元数据头（`=== Task: %1 (ID: %2) | Started: %3 ===\n\n`）；外部工具日志保持原始行输出。
* `ApplicationLogger`：应用级日志静态入口（`debug`/`info`/`warning`/`error`），落盘 `application_<yyyyMMdd_HHmmss_zzz>.log`；配套 `ApplicationLogSink` / `FileSink` / `Logger`。
* `FaultBarrier` / `TaskRunContext` / `TaskSnapshot`：致命故障屏障、任务运行上下文与只读快照。

### 1.3 日志级别契约
* `error()`：记录错误日志并累加任务错误计数（**不直接改变任务状态**；`Failed` 终态由 `fail()` 或 `AsyncTaskRunner` 仲裁落地）。
* `reportFault()`：致命故障屏障入口（返回 `LogSubmissionResult`）。
* `warning()`：可恢复问题、降级处理或跳过（不影响成功终态）。
* `info()`：面向用户的宏观阶段里程碑；`debug()`：技术诊断与内部步骤追踪。

---

## 2. Core 层 API 参考 (`cs2importer_core`)

通用底层基础设施，零 Valve/CS2 业务策略。

* **`Core::Async`**：
  * `CancellationToken`：基于原子共享标志的协作式取消令牌，按值拷贝传递，提供 `cancel()` 与 `isCancelled()`。
* **`Core::Hash`**：
  * `Sha256`：64KB 分块流式文件散列（支持 `CancellationToken`）与内存缓冲散列，输出 64 位小写十六进制字符串（`compute(path, token)` / `compute(data)`）。
* **`Core::Path`**：
  * `FilesystemPath`：双层路径安全抽象：
    * **Tier 1 逻辑包含性**：`isSubpathOf(baseDir)`、`contains(childPath)`、`resolveBelow(subpath)`（基于 `std::filesystem::weakly_canonical`，拦截绝对路径、`../`、盘符冒号 `:` 与 NTFS ADS 流，规整 `\\?\UNC\`）；
    * **Tier 2 物理内核句柄验证**：`verifyHandleWithinBase` 与接收 `const QFileDevice&` 的 `verifyFileWithinBase`（Win32 `GetFinalPathNameByHandleW`），根除 TOCTOU 与 Junction 逃逸。
  * `PathUtils`：路径规范化、扩展名提取、`sanitizeFilename` 与 `isSubpath`（代理至 `FilesystemPath::isSubpathOf`）。
* **`Core::KeyValues`**：
  * `KeyValuesDocument` / `KeyValuesNode` / `KeyValuesParser` / `KeyValuesWriter`：通用 VDF/KeyValues AST 解析与保序序列化器，支持位置感知 CRUD 与头部注释保留。
* **`Core::FileSystem`**：
  * `FileSystem`：`copy()` 与 `move()` 支持 `CancellationToken` 与可选 `expectedBaseDir`（自动执行 Tier 1 + Tier 2 `verifyFileWithinBase`，越界即清理并抛 `ErrorCode::InvalidPath`）；`writeAll()` 内部经 `AtomicFile` 执行单点路径边界校验；`isSubdirectoryOrEqual` 代理至 `FilesystemPath::isSubpathOf`。
  * `AtomicFile`：临时文件重命名原子落盘，内置 `expectedBaseDir` Tier 1 + Tier 2 校验。
  * `DirectorySnapshot` / `FileLease`：目录递归快照 / RAII 排他文件租约。
* **`Core::Process`**：
  * `ProcessRunner` / `ProcessOptions` / `ProcessResult`：控制台进程封装，支持逐行流式回调（`onStdOutLine` / `onStdErrLine`）、超时、`CancellationToken` 及 `toError()` / `toErrorCode()` 机械结果映射。
* **`Core::Temp`**：
  * `TempFile`：统一 RAII 临时文件管理，支持系统临时文件创建（`TempFile::create`）与现有路径托管清理（`TempFile(path, autoRemove)`），提供 `dismiss()` / `release()` / `cleanup()` / `exists()` / `isValid()`。
  * `TempDirectory`：RAII 临时目录，析构自动递归清理。
* **`Core::Error`**：
  * `ErrorCode`：通用基础设施错误码枚举（含 `ArchiveOpenFailed`, `EntryNotFound` 等）。
  * `ExecutionGuard` & `ExecutionContext`：单点异常转译与诊断富化屏障（`stage`, `resourcePath`, `targetPath`），零领域关键词猜词；仅用于任务调度顶层、外部同步入口及抛异常的底层/第三方库调用边界。
  * `Error` / `Exception`：结构化错误值对象（枚举匹配使用带域匹配 `error.is(domainName, code)`）与跨线程异常传输载体。
* **`Core::Result<T>`**：
  * `[[nodiscard]]` 单层结果载体，支持 `Success` / `Failure` / `Cancelled` / `Skipped` 四态及带 `partialValue` 重载。

---

## 3. Domain 层 API 参考 (`cs2importer_domain`)

封装 Valve / Source 业务规则与确定性资产解析，仅依赖 `Core`。

* **`Domain::Asset`**：
  * `AssetLocateStrategy`：纯领域资产定位策略，直接接收 `(relativeAssetPath, targets, PackArchivePool&, const VpkIndex* s1Index, const VpkIndex* cs2Index, CancellationToken)`，按 `CS2 Native → Loose File → VpkIndex O(1) → Fallback Target VPK` 顺序定位，直接返回 `Core::Result<AssetLocateOutcome>`（无冗余虚接口或观察者适配器）。
  * `AssetMatchKind`（`NotFound`, `LooseFile`, `PackEntry`, `NativeCs2`）与 `AssetLocateOutcome{kind, location}`。
  * `AssetLocation`（`target`, `relativePath`, `fromPack`, `sourceTargetPath`）、`AssetPath`（Valve 相对路径）、`AssetTypeDetector`。
* **`Domain::Package`**：
  * `PackArchive`：基于 `sourcepp` (vpkpp/bsppp) 的进程内 VPK / BSP 嵌入包抽象；`extractEntryToFile(entryPath, destFile, token, expectedBaseDir)` 直接调用 `readEntry`（缺省即返回 `ErrorCode::EntryNotFound`，无冗余 `hasEntry` 查找），并在打开文件后执行 `verifyFileWithinBase`；`readEntry` 与 `hasEntry` 支持 `CancellationToken`。
  * `BspPackExtractor`：Source 1 BSP Pakfile 提取器；`BspExtractOptions` 接收 `Core::Async::CancellationToken token`；通过受控局部 `QThreadPool` 并发提取（不裸建 `std::thread`）；目录与文件路径经 `destDir.resolveBelow()`（Tier 1）与 `verifyFileWithinBase(outFile, destDir)`（Tier 2）双层校验。
  * `PackArchivePool`：归档 LRU 池化缓存，键粒度 `OpeningEntry` 条件变量并发同步，防止同一归档并发重复打开并响应取消。
  * `VpkIndex` / `VpkIndexBuilder`：基于 `sourcepp::vpkpp::runForAllEntries` 构建的全量 VPK 只读索引与二进制持久化载体（`.idx`，含文件大小/修改时间与 `_dir.vpk` SHA-256 校验），提供 $O(1)$ 绝对路径路由与 `isCs2()` / `cs2Stems` 跨引擎去重集合。
* **`Domain::Material`**：
  * `VtfConverter`：基于 `vtfpp` 的 VTF 解码与多格式导出（PNG/TGA/JPG/BMP/HDR），经 `Core::Error::ExecutionGuard::guard` 保护 `vtfpp`/`FileSystem` 调用，路径边界由底层 `AtomicFile` 单点校验。
  * `VtfCodec` / `TgaCodec` / `TextureIO` / `TextureImage`：宽读取（PNG/JPG/BMP/TGA/VTF）、窄写入（仅 PNG）的统一纹理 IO 与线性浮点工作缓冲；`TextureIO` 提供 `loadTexture`/`writeTexture` 与 `readImage`/`writeImage`。
  * `SkyboxTypes` / `SkyboxCubeBuilder`：Source 1 → Source 2 3D 天空盒 4x3 立体十字纯进程内构建器（Inside-Out 拓扑、两阶段边缘对齐与 12 物理接缝容错仲裁、`loadFacesFromDirectory`、`composeCubeImage`、`generateVmat`）。
  * **`Domain::Material::TextureProcess`**：PBR 纹理 CPU 生成管线（`HeightGenerator`、`NormalGenerator`、`AoGenerator`、`MetallicGenerator`、`SmoothnessGenerator`、`DiffuseEditor`、`ChannelPacker`、`TextureBlur`、`TexturePresets`；`ColorMath` 为内部实现），统一签名 `Core::Result<TextureImage>` + `CancellationToken` + 进度回调。
* **`Domain::Audio`**：
  * `SoundscapeDefinition` / `SoundscapeParser` / `SoundscapeToSoundEventConverter` / `SoundEventKv3Writer` / `SoundLevelMapper` / `DspPresetRegistry`：Source 1 `soundscapes_*.txt` 解析至 Source 2 KV3 `.vsndevts` 完整领域链路。
* **`Domain::Tool`**：
  * `ResourceCompilerTool` / `Source1ImportTool`：CS2 官方 CLI 工具（`resourcecompiler.exe`、`source1import.exe`）强类型封装；经 `taskCtx->createToolSubTask(...)` 创建隐藏工具子任务并经 `finishToolSubTask`/`failToolSubTask`/`cancelToolSubTask` 终结（不依赖 `LogManager::instance()`）；批量编译（>1 文件）自适应通过 `Core::Temp::TempFile` 生成 `-filelist`；输出经 `logExternalToolOutput` 原始直通。
  * `ResourceCompilerLogParser` / `Source1ImportLogParser`：签名 `parse(stdOut, stdErr = {}, exitCode = 0, workingDirectory = {})`，支持资产级容错与部分成功统计（`failedCount`, `skippedCount`, `compiledCount`）。
  * `Cs2PathLayout` / `ToolErrors` (`ToolErrorCode`, `ToolErrors::is`)。
* **`Domain::Game`**：
  * `GameType` / `EngineType` / `GameDefinition` / `GameRegistry` / `GameInfo` / `GameInfoParser` / `SearchTarget` / `SearchPathResolver` / `GameValidator` / `GameErrors`。
  * `SteamGameLocator`：独占 Valve Steam VDF/ACF 解析领域逻辑（`SteamLibrary`、`parseLibraryFolders(libraryfolders.vdf)`、`discoverLibraries(steamRootPath)`、`readAppInstallDir`、`readAppName`、`resolveGamesInLibrary`、`resolveGameInLibrary`），直接在 Domain 层读取 `appmanifest_*.acf`（无回调函数参数）。
  * `GameInstallationResolver`：游戏安装解析、`inspectGameInfo`、`listSource2Addons`（严格探测 `content/csgo_addons`），以及面向 VPK 索引的领域目标解析：
    * `static Core::Result<std::vector<Core::Path::FilesystemPath>> resolveCs2VpkTargets(const Core::Path::FilesystemPath& cs2BasePath)`
    * `static Core::Result<std::vector<Core::Path::FilesystemPath>> resolveSource1VpkTargets(const QString& gameId, const Core::Path::FilesystemPath& gameInfoPathOrDir)`

---

## 4. Workflow 层 API 参考 (`cs2importer_workflow`)

编排具体导入/转换多步骤流水线，依赖 `Domain` 与 `Core`，不感知 `Application` / `UI`。

* **`Workflow::Common`**：
  * `ImportContext`：组合 `Core::Logging::TaskLoggingContext*` 与 `Core::Async::CancellationToken`，提供 `checkCancelled()` 与 `runStep`（步骤成功后延后推进进度）。
  * `AssetLocator`：直接委托 `Domain::Asset::AssetLocateStrategy::locate` 并记录日志；`locate()` 返回 `Core::Result<std::optional<AssetLocation>>`，`exists()` 返回 `Core::Result<bool>`（CS2 原生存在返回 `true`，真实失败/取消原样上报）。
  * `AssetExtractor` / `AssetExtractOptions`：资产定位与提取中枢。未传入外部 `archivePool` 时在栈帧创建单一局部 `PackArchivePool` 供定位与提取共享，避免重复开包；遵循 `CS2 Native → Loose File → VpkIndex O(1) → Fallback VPK`；向 `PackArchive::extractEntryToFile` 与 `FileSystem::copy` 透传 `expectedBaseDir` 执行单点内核句柄边界校验；`extractCompanions` 穿透取消令牌。
  * `BspEmbeddedExtractor`：薄工作流编排，直接将 `CancellationToken` 透传给 `Domain::Package::BspPackExtractor::extractAll` 并记录 `taskCtx` 摘要日志。
  * `VtfExtractor`：组合 `AssetExtractor` 与 `VtfConverter`，定位 VTF 并固定导出为 PNG（中间解包文件放 RAII 临时目录）。
* **`Workflow::Particle`**：
  * `ParticleImportWorkflow` / `ParticleImportOptions` / `ParticleImportWorkflowResult`：多 `.pcf → .vpcf` 批量导入流水线。`ParticleImportOptions` 携带 `std::shared_ptr<const Domain::Package::VpkIndex> vpkIndex` 与 `cs2Index`；支持暂存防重名隔离（`_cs2import_tmp_<uuid>_<file>`）、批量 `-filelist` 编译及失败/取消时清理半成品（`cleanupGeneratedArtifacts`）。
* **`Workflow::Soundscape`**：
  * `SoundscapeConvertWorkflow` / `SoundscapeConvertOptions`：Source 1 Soundscape 到 CS2 KV3 `.vsndevts` 的完整转换流水线（`convertContent`、`convertFile`、`execute` 地图级扫描与批量转换），经 `Workflow::Common::ImportContext` 处理取消、日志与进度。

---

## 5. Application 层 API 参考 (`cs2importer_application`)

面向 UI 的服务门面、异步调度、共享租约与 DTO 转换。CMake 以 `PRIVATE` 链接 `Workflow` 与 `Domain`，公共头文件通过前置声明隔离底层类型。

* **异步执行基础设施 (`Application::Async`)**：
  * `AsyncTaskRunner`：三平面异步调度器（`runTask` / `runChildTask` / `runBackground` / `runWorkflowTask` / `runSystemTask`），内置顶层异常屏障、`fallbackTerminalStateOnFatalException` 终态兜底与 `invokeCallbackSafely` 回调异常隔离。
  * `SystemTaskLog`：系统任务日志上下文（仅并入 `application_<timestamp>.log`）。
  * `TaskHandle`：`[[nodiscard]]` 异步取消句柄（系统任务 `taskId == 0`）。
  * 异常转译直接使用 `Core::Error::ExecutionGuard`。
* **通用导入前置服务 (`Application::Common`)**：
  * `ImportPrerequisiteService`：Map / Model / Particle 共用前置保障，校验基础请求、使用与 `GameEnvironmentService` 共享的 `std::shared_ptr<VpkSignatureLeaseService>` 独占获取 CS2 `vpk.signatures` 租约、经 `GameInstallationResolver::inspectGameInfo` 解析 S1 `gameinfo.txt` 并同步确保 `VpkIndex` 就绪；头文件前置声明 `Workflow::Common::ImportContext`。
* **VPK 索引服务 (`Application::Package`)**：
  * `VpkIndexService`：管理 `<AppDir>/data/indices/<game_id>.idx` 持久化缓存。PassKey + `create()` 强制 `std::shared_ptr` 所有权；异步入口前置 `weak_from_this().lock()`；后台构建走 `runSystemTask`；信号经主线程亲和性调度（`dispatchIndexReady` / `dispatchIndexUpdated`）发射；头文件前置声明 `Domain::Package::VpkIndex`；VPK 目标解析委托 `GameInstallationResolver::resolveCs2VpkTargets` / `resolveSource1VpkTargets`；返回 `VpkIndexResult{index, persisted, persistenceError}`。
* **环境与检测服务 (`Application::Environment`)**：
  * `GameInstallationInfo`：面向 UI 的游戏安装 DTO，由 `Domain::Game::ResolvedGameInstallation` 直接映射。
  * `Internal::SteamLibraryDetector`：仅负责 Windows 注册表与默认路径探测 Steam 根目录（`detectSteamInstallPath`），VDF/ACF 库解析委托 `Domain::Game::SteamGameLocator::discoverLibraries`。
  * `GameDetectService`：直接调用 `Domain::Game::SteamGameLocator::resolveGamesInLibrary` 并转换为 `GameInstallationInfo`。
  * `GameEnvironmentService`：持有共享的 `std::shared_ptr<VpkSignatureLeaseService>`；公开 `static QString cleanPath(const QString& pathOrUrl)`（统一处理 `file://` URL 与路径规范化）；`validateSource1/2Folder*` 直接调用 `Domain::Game::GameInstallationResolver`；异步探测/校验/插件列举走 `runSystemTask`。
  * `VpkSignatureLeaseService`：CS2 `vpk.signatures` 排他租约策略服务（由 `Main.cpp` 创建共享实例注入各服务）。
* **日志投递门面 (`Application::Logging`)**：
  * `TaskLogService` / `TaskLogDTOs`：**UI 消费日志唯一通道**。私有 `SinkBridge` 实现双阶段 `detach()` 协议（原子标记 + 互斥锁指针置空）防并发析构 UAF；订阅制投递 `logBatchReceived(subscriptionId, taskId, taskName, QVector<TaskLogMessage>)`；提供 `subscribe()` / `unsubscribe()` / `taskInfo()` / `applicationLogFilePath()` / `logsDirectory()` 等。
* **专项业务门面**：
  * `ParticleImportService` (`Application::Particle`)：PassKey + `create()` 强制 `std::shared_ptr`、`weak_from_this().lock()`、`ImportScopeGuard` RAII 回滚；`toWorkflowOptions` 将 `base.vpkIndex` / `base.cs2Index` 透传至 `ParticleImportOptions`，按 `request.gameId` 自动推导 `isCsgo`，经 `GameEnvironmentService::cleanPath` 清洗 `sourcePcfPaths`；经 `runWorkflowTask` 执行。
  * `SoundscapeConvertService` (`Application::Soundscape`)：纯无状态静态门面，将实际转换委托至 `Workflow::Soundscape::SoundscapeConvertWorkflow`，`convertMapSoundscapesAsync` 返回有效 `Async::TaskHandle` 并经 `runWorkflowTask` 执行。

---

## 6. Presentation / UI 层 API 参考 (`cs2importer_ui`)

消费 Application 门面与 DTO，不直接链接或包含 `Domain` / `Workflow` / `Core::Logging`。

* **`UI::ViewModels`**：
  * `LogViewModel` / `LogTaskModel` / `LogMessageListModel`：经 `Application::Logging::TaskLogService` 订阅日志并驱动平铺任务树与 `ToolLogWindow`；`roleNames()` 使用局部静态常量 `QHash`，纯属性访问器无 `get` 前缀，`entries()` 返回 `const QVector<LogMessageItem>&`。
  * `GameViewModel`：持有 `std::shared_ptr<Application::Package::VpkIndexService>`，在 `applyS1Installation` / `applyS2Installation` 时触发后台索引预热；`refreshS2Addons()` 异步列举插件；`updateVpkLease` / `retryVpkLease` 为有意同步的 UI 线程轻量 Win32 租约检查。
* **`UI::Controllers`**：
  * `MainController`：主窗口交互编排，聚合 Application 服务、`LogViewModel` 与 `VpkIndexService`；提供 `startImport` / `startParticleImport` / `stopImport` / `cancelAllOperations()` / `setActiveTab(int)`；URL 清洗与 CSGO 判定交由 Application 层处理，异步回调直接落地无需二次嵌套 `invokeMethod`。
* **QML 组件 (`src/qml/cs2importer/`)**：
  * `LogWindow.qml`、`ToolLogWindow.qml`、`components/LogTaskCard.qml`、`components/SourceFileListBox.qml`。
