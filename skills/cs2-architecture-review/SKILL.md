---
name: cs2-architecture-review
description: >-
  Use this skill when conducting architecture reviews, validating layering compliance, planning module refactoring, or checking against architectural smells and checklists in cs2-map-importer.
---

# CS2 Map Importer — 架构审查与重构演进指南

本指南用于指导架构审查、阶段性重构演进规划以及变更合规性检查。

---

## 1. 模块迁移对照表

| 既有 / 遗留组件 | 目标分层 | 目标路径 | 核心职责与落地状态 |
| :--- | :--- | :--- | :--- |
| `Miscellaneous::RunCommandSync`, `PROGRAM_*` | `Domain::Tool` | `src/Domain/Tool/` | 基于 `Core::Process` 的 Valve 官方工具强类型封装（`ResourceCompilerTool`, `Source1ImportTool`，配备结构化日志解析器 `*LogParser` 与 `ToolErrors`）。[已落地] |
| `VmfBspProcess` | `Domain::Vmf` / `Domain::Bsp` | `src/Domain/Vmf/`, `src/Domain/Bsp/` | VMF 处理与 BSP 反编译行为。 |
| `MaterialFix` | `Domain::Material` | `src/Domain/Material/` | VMT/VMAT 材质转换与修正；`VtfConverter` 负责 VTF 图像解码与格式转换。[已落地] |
| `SoundscapeImport` | `Domain::Audio` + `Application::Soundscape` | 对应目录 | Source 1 Soundscape 脚本解析、KV3 Soundevents 转换与批量服务。[已落地] |
| `FileExtractFromVPK`, Pakfile 提取 | `Domain::Package` + `Workflow::Common` | 对应目录 | 基于 `sourcepp` 的内嵌包解析与提取（`PackArchive`, `BspPackExtractor`, `PackArchivePool` 归档池化缓存）及 `AssetExtractor`，完全移除外部 VPKEdit CLI 依赖。[已落地] |
| `Miscellaneous::ParseGameInfo`, `SearchTarget` | `Domain::Game` | `src/Domain/Game/` | GameInfo 解析、校验与搜索路径解析。[已落地] |
| `ModelImporter` | `Workflow::Model` | `src/Workflow/Model/` | `.mdl → .vmdl` 导入流水线。 |
| `ParticleImporter` | `Workflow::Particle` + `Application::Particle` | 对应目录 | `.pcf → .vpcf` 导入流水线（`ParticleImportWorkflow` + `ParticleImportService`），调用官方资源编译器编译。[已落地] |
| `MapImporter` | `Workflow::Map` | `src/Workflow/Map/` | BSP → VMF → 编译/资产提取流水线。 |
| `Ui::AutoDetectPaths`, `IsValid*` | `Application::Environment` + `Domain::Game` | 对应目录 | Application 编排 + Domain 校验。[已落地] |
| `vpk.signatures` 锁定 / 导入前置保障 | `Application::Common` + `Application::Environment` | 对应目录 | `ImportPrerequisiteService` 统一校验基础参数并获取 CS2 文件租约。[已落地] |
| `Ui::CheckForUpdate` | `Application::Update` | `src/Application/Update/` | 自动更新检测。 |
| `Ui::LoadFromCfg`, `SaveToCfg` | `Application::Config` | `src/Application/Config/` | 配置持久化。 |
| `Ui::Start`, 工作线程, `CancelAll` | `Application::Async`（任务服务 `Application::Task`【规划】） | 对应目录 | `AsyncTaskRunner`（`runTask` / `runWorkflowTask` / `runSystemTask`）、`TaskHandle`、`SystemTaskLog`（系统任务平面）与协作式取消及任务生命周期管理。[已落地] |
| `LogViewModel` 直连 `Core::Logging`（`registerWithLogManager` 时代） | `Application::Logging` | `src/Application/Logging/` | `TaskLogService` 日志投递门面 + `TaskLogDTOs` UI 侧值类型；UI 消费日志唯一通道（订阅制投递、陈旧批次抑制），`src/UI/` 严禁 include `Core/Logging/*`。[已落地] |
| `Ui.h/.cpp` Q_PROPERTY/slots | `UI` | `src/UI/` | 极薄的表现层适配器（`MainController`, `LogViewModel` 等）。[重构中] |

---

## 2. 重构演进路线图

重构按阶段逐步推进，**严禁为了让临时代码通过编译而跨阶段混杂实现**。

1. **Stage 1 — Core 基础设施解耦提取**（已完成：错误体系、文件系统、KeyValues、任务导向日志、异步取消令牌 `CancellationToken`）
2. **Stage 2 — Domain 领域基础迁移**（已完成：游戏模型/注册表/校验器、`Domain::Package` [PackArchive, BspPackExtractor, PackArchivePool]、`Domain::Material` [VtfConverter]、`Domain::Audio` [Soundscape 解析与转换]、`Domain::Tool` [CS2 官方工具与日志解析器]）
3. **Stage 3 — 导入器与领域逻辑迁移**（进行中：`Workflow::Particle` 已落地；`Workflow::Common` 资产提取与 `ImportContext` 已就绪；待推进：ModelImporter → `Workflow::Model`、VmfBspProcess → `Domain::Vmf` + `Domain::Bsp`）
4. **Stage 4 — Application 应用编排重构**（进行中：`AsyncTaskRunner`、`TaskHandle`、`runWorkflowTask` / `runSystemTask` / `SystemTaskLog`、`TaskLogService` 日志门面、`ImportPrerequisiteService`、`ParticleImportService`、`SoundscapeConvertService`、`GameEnvironmentService`、`GameInstallationValidator` 已落地；待补齐：统一 ConfigService、UpdateService）
5. **Stage 5 — MapImporter 重构与 UI 瘦身**（待推进：MapImporter → `Workflow::Map`、UI 彻底收敛为纯展示与 Application 调用）

---

## 3. 架构变更必须执行的准则（八问决策）

在修改代码前，必须明确回答以下问题：

1. **该行为归属于哪一层？**
2. **在不违反依赖拓扑图的前提下，能够实现该功能的最低层级是哪一层？**
3. **应该由谁来负责编排该流程？**
4. **跨越分层边界的公开契约是什么？**
5. **拟定引入的头文件是否包含了当前层级之上的模块？**
6. **CMake 目标依赖图是否依然保持严格单向？**
7. **该操作是否会阻塞 UI 线程？**
8. **该修改是否引入了全局状态、全局静态日志、直接 QProcess 调用或 UI 耦合？**

若任一答案暴露了架构边界违规，**必须在编码前重新设计**。

### 推荐实现次序

```text
1. 定义/调整底层契约
2. 实现 Domain / Core 行为
3. 添加 Application 编排 / 门面
4. 连接 UI 与 Application 契约
5. 编写 / 更新自动化测试
6. 验证 include 与 CMake 依赖方向
```

---

## 4. 强制架构审查清单 (Architecture Review Checklist)

任何重构代码提交前必须对照本清单自查：

### 4.1 职责归属
* [ ] 修改的每个函数均归属于正确的层级。
* [ ] UI 类中无 Domain 业务编排。
* [ ] Application 类中无本应属于更底层的具体 Domain 转换逻辑。
* [ ] Domain / Core 类绝不感知 Application / UI。

### 4.2 依赖关系图
* [ ] 未引入任何向上逆向 include。
* [ ] CMake 中未引入向上的反向依赖。
* [ ] UI 模块未为访问底层细节而链接 Domain / Core。
* [ ] UI 未 include `Core/Logging/*`——日志仅经 `Application::Logging::TaskLogService` 门面与其 DTO 消费。
* [ ] Workflow 不依赖 Application / UI。

### 4.3 运行时表现
* [ ] 阻塞性 I/O 绝不在 UI 线程执行。
* [ ] Worker 回调具备生命周期安全防护，并通过排队连接安全回到 UI 线程。
* [ ] 取消操作显式、协作且确定。

### 4.4 集成边界
* [ ] `Domain::Tool` + `Core::Process` 之外无直接 `QProcess` / Shell 调用。
* [ ] 外部 CLI 工具必须通过 `LogManager::createToolTask` 封装为隐藏任务，并经 `ProcessOptions` 回调实现流式日志重定向与取消令牌绑定。
* [ ] Application / UI 弹窗桥接之外无直接模态对话框调用。
* [ ] 未引入全局静态日志器。
* [ ] 未引入新的全局可变状态。

### 4.5 API 规范
* [ ] UI 接收 Application 契约对象，而非 Domain AST / 底层设施对象。
* [ ] Domain API 使用强领域类型。
* [ ] 错误处理结构化并保留诊断上下文。
* [ ] 未重复编写已有 Core 基础设施的功能。
* [ ] 用户可见文案已包裹 `tr()` / `QCoreApplication::translate` 且上下文为类名（i18n 契约，见 AGENTS.md §5.7）；`details()` 与外部工具原始输出保持英文原文。

### 4.6 测试生命周期与分层契约
* [ ] `tests/` 目录中的长期常驻测试仅限于 Core 层（`test_core_*`），仅链接 `cs2importer_core` 与 Qt6::Core/Test。
* [ ] 非 Core 层（Domain / Workflow / Application / UI）测试仅作为开发验证期间的临时单任务测试（Task-Scoped / Ephemeral Tests）。
* [ ] 任务完成后，上层临时测试必须彻底清理/删除，严禁合入主线或在 CMakeLists.txt 中残留对非 Core 模块的测试链接。

---

## 5. 架构红线异味清单 (必须重构)

若在代码中发现以下模式，必须立即重构：

```text
UI/ViewModel → Domain::GameValidator
UI/ViewModel → Core::FileSystem
UI/ViewModel → Core::KeyValues
UI/ViewModel → Core::Process
UI/ViewModel → Core::Logging（include `Core/Logging/*` 或直连 `LogManager`/日志文件）
UI/ViewModel → Steam 注册表 / 库扫描

Application → 直接操作 QML 控件
Domain → Application
Domain → UI
Domain → QMessageBox / QWidget / QQml...
Workflow → UI
Workflow → Application

任何业务文件 → QProcess / system() / Shell
任何业务文件 → 全局 Logger::info/error/warning
任何业务文件 → 硬编码 QStringLiteral 用户可见文案（应经 tr() / QCoreApplication::translate）

tests/ 常驻测试目标 → 链接 cs2importer_domain / cs2importer_workflow / cs2importer_application / cs2importer_ui
非 Core 临时任务测试 → 任务结束后残留于代码库中

承担众多杂项职责的庞大静态 Application 服务
无明确移除计划的临时跨层 include
```
