---
name: cs2-architecture-review
description: >-
  Architecture review checklist, Deep Module & Single-Point Defense audit rules, migration map, and architectural smell catalog for cs2-map-importer. Trigger when reviewing PRs/changes, planning module refactoring, or auditing layering compliance.
---

# CS2 Map Importer — 架构审查与重构演进指南

配合根目录 [`ARCHITECTURE.md`](../../ARCHITECTURE.md) 使用，用于指导 PR 架构审查、演进路线规划与深模块合规性核验。

---

## 1. 模块迁移对照表

| 既有 / 遗留组件 | 目标分层 | 目标路径 | 核心职责与落地状态 |
| :--- | :--- | :--- | :--- |
| `Miscellaneous::RunCommandSync`, `PROGRAM_*` | `Domain::Tool` | `src/Domain/Tool/` | 基于 `Core::Process` 的官方 CLI 强类型封装（`ResourceCompilerTool` [自适应 `-filelist`]、`Source1ImportTool`、`*LogParser`、`ToolErrors`）；通过 `taskCtx->createToolSubTask` 派生隐藏工具子任务，不依赖 `LogManager::instance()`。[已落地] |
| `VmfBspProcess` | `Domain::Vmf` / `Domain::Bsp` | `src/Domain/Vmf/`, `src/Domain/Bsp/` | VMF 处理与 BSP 反编译行为。[规划中] |
| `MaterialFix`, `3dskyboxfix` | `Domain::Material` | `src/Domain/Material/` | VMT/VMAT 材质转换；VTF 解码（`VtfCodec` / `VtfConverter` [经 `Core::Error::ExecutionGuard`]）；`TextureIO` + `TextureProcess` PBR 管线；`SkyboxCubeBuilder` 3D 天空盒立体十字构建器。[已落地] |
| `SoundscapeImport` | `Domain::Audio` + `Workflow::Soundscape` + `Application::Soundscape` | 对应目录 | `Domain::Audio` 负责解析与 KV3 写入；`Workflow::Soundscape::SoundscapeConvertWorkflow` 承接多文件扫描与转换流水线；`Application::Soundscape::SoundscapeConvertService` 为纯静态门面并返回 `TaskHandle`。[已落地] |
| `FileExtractFromVPK` 与跨包盲猜解包 | `Domain::Package` + `Workflow::Common` + `Application::Package` | 对应目录 | 内嵌 `sourcepp` 解包（`PackArchive` 单次 `readEntry` + `verifyFileWithinBase`、`BspPackExtractor` 接受 `CancellationToken` + 局部 `QThreadPool`、`PackArchivePool` 细粒度并发同步）；`VpkIndex` / `VpkIndexBuilder` / `VpkIndexService`（前置声明隔离 + 委托 `GameInstallationResolver` 解析目标）；`AssetExtractor`（共享 `PackArchivePool`、`VpkIndex` $O(1)$ 点查、CS2 原生去重、单点 `expectedBaseDir` 透传）。[已落地] |
| 资产定位与来源探测 | `Domain::Asset` + `Workflow::Common` | `src/Domain/Asset/`, `src/Workflow/Common/` | 精简深模块：`Domain::Asset::AssetLocateStrategy` 直接返回 `Core::Result<AssetLocateOutcome>`；`Workflow::Common::AssetLocator` 提供高保真 `locate()` 与 `exists()`。[已落地] |
| 异常转译与路径安全 | `Core::Error` + `Core::Path` + `Core::FileSystem` | 对应目录 | `Core::Error::ExecutionGuard` 通用单点异常屏障；`FilesystemPath` Tier 1 `resolveBelow` + Tier 2 `verifyFileWithinBase` 在 `AtomicFile` / `FileSystem::copy/move` 单点收口。[已落地] |
| `Miscellaneous::ParseGameInfo`, `Steam` VDF/ACF 解析 | `Domain::Game` | `src/Domain/Game/` | `GameInfoParser` / `SearchPathResolver` / `GameValidator` / `GameInstallationResolver`（含 `resolveCs2VpkTargets`、`resolveSource1VpkTargets`、`listSource2Addons` 严格探测 `content/csgo_addons`）与 `SteamGameLocator`（独占 VDF/ACF 解析）。[已落地] |
| `Ui::AutoDetectPaths`, `IsValid*` | `Application::Environment` + `Domain::Game` | 对应目录 | `SteamLibraryDetector` 仅探测 Windows 注册表/默认根目录，`GameDetectService` 与 `GameEnvironmentService`（含 `static cleanPath` 与共享 `VpkSignatureLeaseService`）直接调用 `Domain::Game` 并映射为 `GameInstallationInfo`。[已落地] |
| `ParticleImporter` | `Workflow::Particle` + `Application::Particle` | 对应目录 | `ParticleImportWorkflow`（接收 `vpkIndex`/`cs2Index`、暂存隔离、自适应 `-filelist` 编译、半成品清理）+ `ParticleImportService`（PassKey 共享所有权、RAII 回滚、透传索引与清洗路径）。[已落地] |
| `ModelImporter` / `MapImporter` | `Workflow::Model` / `Workflow::Map` | `src/Workflow/Model/`, `src/Workflow/Map/` | `.mdl → .vmdl` 与 `BSP → VMF → 编译/提取` 流水线。[规划中] |
| `vpk.signatures` 锁定与导入前置保障 | `Application::Common` + `Application::Environment` + `Application::Package` | 对应目录 | `Main.cpp` 注入共享 `VpkSignatureLeaseService` 至 `GameEnvironmentService` 与 `ImportPrerequisiteService`；前置服务同步校验租约与 `VpkIndex`。[已落地] |
| `Ui::Start`, 工作线程, `CancelAll` | `Application::Async` | `src/Application/Async/` | `AsyncTaskRunner`（`runTask` / `runWorkflowTask` / `runSystemTask`）、`TaskHandle`、`SystemTaskLog`、顶层异常屏障与终态兜底。[已落地] |
| `LogViewModel` 日志通道 | `Application::Logging` | `src/Application/Logging/` | `TaskLogService` + `TaskLogDTOs`（订阅制、`SinkBridge` 双阶段 `detach()` 协议）；`src/UI/` 无 `Core/Logging/*` 依赖。[已落地] |
| `Ui::CheckForUpdate` / `LoadFromCfg` | `Application::Update` / `Application::Config` | 对应目录 | 自动更新检测与配置持久化。[规划中] |
| `Ui.h/.cpp` Q_PROPERTY/slots | `UI` | `src/UI/` | `MainController`、`LogViewModel`、`GameViewModel`（持有 `std::shared_ptr<VpkIndexService>`），CMake `PRIVATE` 物理阻断 UI 接触 `Domain`/`Workflow`。[重构中] |

---

## 2. 重构演进路线图

1. **Stage 1 — Core 基础设施**（已完成）：`Core::Error::ExecutionGuard`、路径安全 Tier 1/Tier 2、`AtomicFile`、`FileSystem`、`Sha256`、`TempFile`、`KeyValues`、`TaskLoggingContext::createToolSubTask`、`CancellationToken`。
2. **Stage 2 — Domain 领域迁移与深模块收紧**（已完成）：`Domain::Game`（含 `SteamGameLocator` VDF/ACF 解析与 `GameInstallationResolver` VPK 目标解析）、`Domain::Asset`（`AssetLocateStrategy` 无虚接口直查）、`Domain::Package`（`PackArchive`、`BspPackExtractor`、`PackArchivePool`、`VpkIndex`）、`Domain::Material`（`VtfCodec`/`VtfConverter`/`TextureIO`/`TextureProcess`/`SkyboxCubeBuilder`）、`Domain::Audio`、`Domain::Tool`（去 `LogManager` 单例）。
3. **Stage 3 — Workflow 流水线归位与扩展**（进行中）：`Workflow::Common`（`ImportContext`、`AssetLocator`、`AssetExtractor` 共享池、`BspEmbeddedExtractor`、`VtfExtractor`）、`Workflow::Particle`、`Workflow::Soundscape`（`SoundscapeConvertWorkflow`）已落地；待推进：`Workflow::Model`、`Domain::Vmf` + `Domain::Bsp`。
4. **Stage 4 — Application 服务与物理隔离**（进行中）：CMake `PRIVATE` 隔离、`AsyncTaskRunner`、`TaskLogService`、`VpkIndexService`、`ImportPrerequisiteService`（共享租约）、`ParticleImportService`、`SoundscapeConvertService`、`GameEnvironmentService`；待补齐：`ConfigService`、`UpdateService`。
5. **Stage 5 — MapImporter 重构与 UI 瘦身**（进行中）：`SourceFileListBox` 与并排 Tab 布局、`GameViewModel` 共享指针化、`MainController` 去业务判断已落地；待推进：`Workflow::Map`。

---

## 3. 架构变更前决策准则

修改代码前先确认：
1. **归属层级**：不违反依赖拓扑图的前提下，能实现该职责的最低层是哪一层？
2. **删除测试（Deletion Test）**：新增的包装类、中间结构体或虚接口是否承载真实不变式或边界，而非浅层透传？
3. **单点防御（Single-Point Defense）**：异常屏障、路径安全校验与前置检查是否仅在权威边界执行一次？
4. **依赖与线程**：CMake 与 include 是否保持单向？阻塞 I/O 是否在 Worker 线程执行？

推荐实现顺序：`底层契约 → Domain/Core → Workflow → Application 门面 → UI 连接 → 临时自测并清理 → 验证 include 与 CMake 依赖`。

---

## 4. 强制架构审查清单 (Architecture Review Checklist)

### 4.1 职责归属与 Deep Module（深模块）
* [ ] 无未通过 **Deletion Test** 的浅层传声筒包装类、单行 `using` 伪装头文件或跨层镜像结构体。
* [ ] 无仅有一个实现类且无多态替换需求的假想纯虚接口与适配器。
* [ ] 多步骤导入/转换流水线归属 `Workflow`（绝不在 `Application` 服务中直接编排文件扫描、解析与批量写入）。
* [ ] Valve 格式、VDF/ACF、`gameinfo.gi`/`gameinfo.txt` 解析归属 `Domain`（`Application` 不直接操纵 `Core::KeyValues` 解析 Valve 配置）。
* [ ] UI 不含业务路径清洗（如 `file://` URL 解析）或游戏类型分支判断，均由 Application 处理。

### 4.2 Single-Point Defense（单点契约防御）
* [ ] **异常屏障**：`Core::Error::ExecutionGuard::guard` 仅出现在 `AsyncTaskRunner`、对外同步入口及会抛异常的底层/第三方库（`sourcepp`/`vtfpp`/`Core::FileSystem`/STL）调用点；内部 `Core::Result<T>` 函数之间无嵌套 `ExecutionGuard` 或自定义 `runGuarded`。
* [ ] **路径安全**：调用方经 `baseDir.resolveBelow(relPath)` 组合相对路径并透传 `expectedBaseDir` 给底层 I/O 原语（`AtomicFile`、`FileSystem::copy/move`、`PackArchive::extractEntryToFile`、`BspPackExtractor`）；中间层无重复 `isSubpathOf` 检查。
* [ ] **前置校验**：`isEmpty()` / `isValid()` / `exists()` / `isCancelled()` 仅由实际消费层单点校验，上层直接透传 `Core::Result<T>`。

### 4.3 依赖关系图与 CMake 物理隔离
* [ ] 未引入任何向上逆向 include 或 CMake 反向链接。
* [ ] `cs2importer_application` 保持以 `PRIVATE` 链接 `cs2importer_workflow` 与 `cs2importer_domain`；Application 公共头文件对底层类型使用前置声明。
* [ ] `src/UI/` 未链接 `Domain`/`Core`，未 include `Core/Logging/*`（日志仅经 `Application::Logging::TaskLogService`）。
* [ ] `Workflow` 不依赖 `Application` / `UI`；`Domain` 不依赖 `Workflow` / `Application` / `UI`。

### 4.4 运行时、集成边界与并发安全
* [ ] 阻塞性 I/O 绝不在 UI 线程执行；`Domain` 不裸建 `std::thread`。
* [ ] `Domain::Tool` + `Core::Process` 之外无直接 `QProcess` / Shell 调用；`Domain::Tool` 仅通过 `taskCtx->createToolSubTask(...)` 派生隐藏工具任务，不 include `LogManager.h` 或直调 `LogManager::instance()`；多文件编译自适应使用 `-filelist` 临时清单。
* [ ] VPK 提取基于 `VpkIndex` $O(1)$ 点查或松散文件优先，定位与提取共享同一 `PackArchivePool`；后台索引构建走 `runSystemTask` 系统任务平面。
* [ ] 共享资源（如 `VpkSignatureLeaseService`）由 `Main.cpp` 创建单一 `std::shared_ptr` 注入全部协作服务；异步服务（如 `VpkIndexService`）在 UI 侧以 `std::shared_ptr` 持有。
* [ ] 继承 `std::enable_shared_from_this` 的异步服务使用 PassKey + `create()` 工厂、前置 `weak_from_this().lock()`、RAII 状态回滚与主线程亲和性信号分发（`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`）。
* [ ] 跨模块 Sink 桥接（如 `TaskLogService::SinkBridge`）实现双阶段 `detach()` 协议；无状态转换门面（如 `SoundscapeConvertService`）声明为静态纯函数并返回 `TaskHandle`。

### 4.5 编码、i18n 与临时测试契约
* [ ] 纯属性访问器无 `get` 前缀；Qt Model `roleNames()` 使用局部 `static const QHash`；容器只读访问器返回 `const QVector<T>&`；枚举显式 `: int` + 尾随逗号；含 `Q_ASSERT` 的函数不标 `noexcept`。
* [ ] `Core::Error::ExecutionGuard` 零领域关键词猜词；`AssetLocator::exists()` 高保真返回 `Core::Result<bool>`（CS2 原生存在返回 `true`，失败/取消不吞没为 `false`）。
* [ ] 用户可见文案在创建处包裹 `tr()` / `QCoreApplication::translate("<类名>", ...)`；`details()` 与工具原始输出保持英文。
* [ ] 任务完成前已彻底删除 `test_tmp_*` 临时测试源码及任何临时 CMake 修改，主干零常驻测试残留。

---

## 5. 架构红线异味清单 (必须重构)

发现以下任一模式时立即重构：

```text
[深模块与单点防御违例]
- 浅层传声筒包装类 / 镜像结构体 / 单行 using 伪装头文件（未通过 Deletion Test）
- 仅有一个实现类的假想纯虚接口与适配器
- 内部 Result<T> 函数链层层嵌套 ExecutionGuard::guard 或自定义 runGuarded
- 中间层在调用 expectedBaseDir 保护的底层原语前重复手写 isSubpathOf
- 相邻层逐行复制相同的 isEmpty / isValid / exists 前置 if 校验块
- 在 Application 层直接编写多步骤导入/转换流水线（应归位至 Workflow）
- 在 Application 层直接操纵 Core::KeyValues 解析 Valve VDF/ACF/GameInfo（应下沉至 Domain）

[分层与集成违例]
- UI/ViewModel → Domain / Workflow / Core::FileSystem / Core::KeyValues / Core::Process / Core::Logging
- UI/Controller → 手写 file:// URL 清洗、游戏类型判断或冗余二次 QMetaObject::invokeMethod
- Domain::Tool → #include "Core/Logging/LogManager.h" 或直调 LogManager::instance()
- Domain → 裸建 std::thread / 弹窗 / 依赖 Application 或 UI
- Workflow → 依赖 Application 或 UI / 定位与提取各自新建 PackArchivePool 重复开包
- 任何业务文件 → QProcess / system() / 全局静态 Logger / 盲目遍历 VPK 撞库
- Core 层 → 按异常文本关键词猜词推断 Valve 领域错误码

[并发、生命周期与规范违例]
- 异步服务 → 绕过 PassKey/shared_ptr 构造、Worker 线程直接 emit UI 信号、回调空 catch (...) {}
- Sink 桥接 → 析构未执行双阶段 detach() 导致并发 UAF
- 组合根 → 重复构造两份独立 VpkSignatureLeaseService 或丢弃已构建的 vpkIndex/cs2Index
- Qt/i18n/测试 → roleNames 动态构造 QHash / getter 带 get 前缀 / Q_ASSERT 函数标 noexcept / 硬编码用户文案 / 残留临时测试
```
