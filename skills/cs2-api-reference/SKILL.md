---
name: cs2-api-reference
description: >-
  Use this skill as a comprehensive API dictionary and module reference for Core, Domain, Workflow, and Application layers in cs2-map-importer. Consult when looking up available classes, services, parsers, and utilities.
---

# CS2 Map Importer — 分层 API 架构参考字典

本指南按架构分层归纳了项目各层已实现的核心类、服务接口、解析器与基础设施。

---

## 1. 日志系统 API (`Core::Logging`)

项目采用任务导向日志系统（Task-Oriented Logging）。

### 1.1 核心调用范式

```cpp
auto task = Core::Logging::LogManager::instance()
                .createTask(QStringLiteral("导入模型"));

task->start();
task->info(QStringLiteral("开始处理"));
task->updateProgress(0.5, QStringLiteral("转换中"));
task->complete(QStringLiteral("处理完成"));
```

### 1.2 日志级别契约
* `error()` / `reportFault()`：当前任务发生不可恢复的业务失败（触发后任务将被置为 `TaskState::Failed`）。
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
  * `FileSystem`：通用文件系统辅助工具；
  * `AtomicFile`：基于临时文件重命名的原子落盘写入；
  * `DirectorySnapshot`：目录递归快照；
  * `FileLease`：RAII 移动语义的文件排他锁/租约机制。
* **`Core::Process`**：
  * `ProcessRunner`：封装外部控制台进程调用，提供超时控制、工作目录设置、环境变量管理；
  * `ProcessOptions`：进程执行配置参数；
  * `ProcessResult`：进程退出码、stdout、stderr 及机械状态载体。
* **`Core::Temp`**：
  * `TempFile` / `TempDirectory`：RAII 临时资源生命周期管理，析构时自动清理。
* **`Core::Error`**：
  * `ErrorCode`：跨项目通用底层系统/设施错误码枚举；
  * `Error`：机器可解析的结构化错误值对象（包含 code, message, details, 扩展领域码）；
  * `Exception`：跨线程异常传输载体（派生自 `std::exception` / `QException`）。
* **`Core::Result<T>`**：
  * 标准业务结果承载类型，原生支持 `Success`, `Failure`, `Cancelled`, `Skipped` 四态及 `Core::Error`。

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
  * `Source1ImportTool`：Valve CS2 官方导入工具（`source1import.exe`）强类型调用封装；
  * `ResourceCompilerLogParser` / `Source1ImportLogParser`：编译器输出的高性能日志解析器，提取统计、警告与错误；
  * `Cs2PathLayout`：推导 CS2 content 树与 game 树分离的标准资产布局规范；
  * `ToolErrors` (`ToolErrorCode`)：工具专属强类型领域错误模型。
* **`Domain::Game`**：
  * `GameType` / `EngineType`：游戏类型与引擎类型强类型枚举；
  * `GameDefinition` / `GameRegistry`：元数据驱动的游戏定义与注册表；
  * `GameInfo` / `GameInfoParser`：`gameinfo.gi` 与 `gameinfo.txt` 结构化解析；
  * `SearchTarget` / `SearchPathResolver`：搜索路径多层级解析推导；
  * `GameValidator`：游戏安装目录确定性校验器。

---

## 4. Workflow 层 API 参考 (`cs2importer_workflow`)

实现具体资产导入用例与流水线编排，依赖 Domain 与 Core，严禁依赖 UI / Application。

* **`Workflow::Common`**：
  * `ImportContext`：组合 `Core::Logging::TaskLoggingContext*` 与 `Core::Async::CancellationToken`，提供统一的任务日志、进度汇报与取消状态检查（`checkCancelled()`）；
  * `AssetExtractor`：按 `SearchTarget` 列表定位并提取资产（目录松散文件 → 目标 `pak01_dir.vpk` → VPK 目标），结合 `PackArchivePool` 进行归档复用；
  * `BspEmbeddedExtractor`：经 `Domain::Package::PackArchive` 与 `BspPackExtractor` 枚举并提取 BSP 内部嵌入资产；
  * `VtfExtractor`：组合 `AssetExtractor` 与 `Domain::Material::VtfConverter`，按指定格式提取并转码 VTF。
* **`Workflow::Particle`**：
  * `ParticleImportWorkflow`：Source 1 `.pcf` 到 Source 2 `.vpcf` 的完整导入工作流，编排依赖提取、content 目录资产生成与 `Domain::Tool::ResourceCompilerTool` 编译；
  * `ParticleImportOptions`：粒子导入配置参数（深度混合、禁用漫反射等）。

---

## 5. Application 层 API 参考 (`cs2importer_application`)

应用服务、业务任务编排与生命周期管理，向 UI 暴露轻量 DTO 契约。

* **异步执行基础设施 (`Application::Async`)**：
  * `AsyncTaskRunner`：双平面异步任务调度器，统一处理生命周期转移、异常安全转译与回调投递；
  * `TaskHandle`：异步任务生命周期句柄，配合 `CancellationToken` 支持协作式取消；
  * `ExecutionGuard` (`Application::Execution`)：在应用服务边界安全捕获异常并转译为 `Result<T>::failure`。
* **通用导入前置服务 (`Application::Common`)**：
  * `ImportPrerequisiteService`：Map、Model、Particle 导入共用的前置保障服务，校验基础参数（`BaseImportRequest`）并在必要时独占获取 CS2 `vpk.signatures` 文件租约。
* **环境与检测服务 (`Application::Environment`)**：
  * `SteamService`：Steam 安装目录与库探测，读取 App Manifest；
  * `GameInstallation` / `GameInstallationInfo`：探测到的游戏安装应用层数据表示；
  * `GameDetectService` / `GameEnvironmentService`：游戏探测与环境校验编排；
  * `VpkSignatureLeaseService`：CS2 `vpk.signatures` 排他性租约策略服务。
* **专项业务服务**：
  * `ParticleImportService` (`Application::Particle`)：粒子导入高层业务编排服务，对外暴露面向 UI 的 DTO 契约（`ParticleImportRequest`, `ParticleImportResult`）；
  * `SoundscapeConvertService` (`Application::Soundscape`)：声音景观批量转换服务，将 Source 1 脚本转为 CS2 KV3 音效事件文件。
