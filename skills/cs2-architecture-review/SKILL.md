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
| `Miscellaneous::RunCommandSync`, `PROGRAM_*` | `Domain::Tool` | `src/Domain/Tool/` | 基于 `Core::Process` 的 Valve 官方工具强类型封装（`ResourceCompilerTool` [自适应 `-filelist` 清单批量编译], `Source1ImportTool`，配备结构化日志解析器 `*LogParser` [支持资源级容错与部分成功统计] 与 `ToolErrors`）。[已落地] |
| `VmfBspProcess` | `Domain::Vmf` / `Domain::Bsp` | `src/Domain/Vmf/`, `src/Domain/Bsp/` | VMF 处理与 BSP 反编译行为。 |
| `MaterialFix`, `3dskyboxfix` | `Domain::Material` | `src/Domain/Material/` | VMT/VMAT 材质转换与修正；VTF 解码（`VtfCodec` / `VtfConverter`）与纹理处理管线（`TextureProcess` PBR 贴图生成、通道打包，`TextureIO` 宽读取 / 仅 PNG 导出与 `readImage`/`writeImage` 原生读写）；3D 天空盒立体十字构建器（`SkyboxCubeBuilder` / `SkyboxTypes`：FT 为绝对中心的 4x3 纯内视视角十字拓扑、水平环优先与 UP/DN 联合多邻接面 SAD 打分防平滑天空熔断、12 物理接缝闭环容错与孤立、Step 2B 假性一致性仲裁）。[已落地] |
| `SoundscapeImport` | `Domain::Audio` + `Application::Soundscape` | 对应目录 | Source 1 Soundscape 脚本解析、KV3 Soundevents 转换与批量服务（无状态静态纯函数，`convertMapSoundscapesAsync` 返回有效 `TaskHandle`）。[已落地] |
| `FileExtractFromVPK`, 跨包盲猜撞库与解包 | `Domain::Package` + `Workflow::Common` + `Application::Package` | 对应目录 | 基于 `sourcepp` 的内嵌包解析与提取（`PackArchive`, `BspPackExtractor` [resolveBelow + 目标句柄校验], `PackArchivePool` 细粒度 `OpeningEntry` 条件变量并发同步与防重入缓存）；VPK 全量树持久化二进制索引（`VpkIndex`, `VpkIndexBuilder`, `VpkIndexService` [PassKey 强制共享所有权、后台预热与 SHA-256 校验、UI 主线程信号隔离 `dispatch*`]）及 `AssetExtractor`（统一 `AssetExtractOptions` 配置、松散文件优先、索引点查直接命中、CS2 原生规整去重、解压与复制透传 `expectedBaseDir` 目录防穿越与伴生资源取消感知），完全移除外部 VPKEdit CLI 依赖并根除盲目撞库试探开销。[已落地] |
| 资产定位与来源探测 | `Domain::Asset` + `Workflow::Common` | `src/Domain/Asset/`, `src/Workflow/Common/` | 资产定位三层抽象：Domain 纯策略解耦（`IAssetSourceProber`, `ArchiveAssetSourceProber`, `AssetLocateStrategy`, `AssetLocation`）；Workflow 消费层（`AssetLocator` 提供高保真 API `exists()` 返回 `Result<bool>`，CS2 原生存在即视为 `true`，`locate()` 精确区分松散/归档/原生/未找到/取消/失败）。[已落地] |
| 异常转译与上下文边界 | `Core::Error` | `src/Core/Error/` | 通用异常边界防护（`Core::Error::ExecutionGuard` 与 `ExecutionContext`），零业务关键字猜测，直接保留强类型 `Core::Error::Exception` 错误码并归一化标准异常转译。[已落地] |
| 路径安全与边界判定 | `Core::Path` + `Core::FileSystem` | `src/Core/Path/`, `src/Core/FileSystem/` | `FilesystemPath` 双层安全体系（Tier 1 `resolveBelow` / `isSubpathOf` 逻辑前缀与弱规范化判定 + Tier 2 `verifyHandleWithinBase` / `verifyFileWithinBase` 对 `QFileDevice` 物理内核句柄校验），以及 `Core::FileSystem::FileSystem::copy/move` 自动联动 `expectedBaseDir` 执行内核边界验证与越界清理，彻底消除符号链接与 Junction 逃逸漏洞与 TOCTOU 竞态。[已落地] |
| `Miscellaneous::ParseGameInfo`, `SearchTarget` | `Domain::Game` | `src/Domain/Game/` | GameInfo 解析、校验与搜索路径解析；S2 插件扫描严格收口至 `content/csgo_addons`。[已落地] |
| `ModelImporter` | `Workflow::Model` | `src/Workflow/Model/` | `.mdl → .vmdl` 导入流水线。 |
| `ParticleImporter` | `Workflow::Particle` + `Application::Particle` | 对应目录 | `.pcf → .vpcf` 导入流水线（`ParticleImportWorkflow` + `ParticleImportService` [PassKey 强制 `std::shared_ptr` 所有权契约、前置 `weak_from_this().lock()` 预检与 `ImportScopeGuard` 状态回滚]），支持多 PCF 批量导入、暂存防重名隔离保护、调用官方资源编译器自适应清单编译及半成品清理。[已落地] |
| `MapImporter` | `Workflow::Map` | `src/Workflow/Map/` | BSP → VMF → 编译/资产提取流水线。 |
| `Ui::AutoDetectPaths`, `IsValid*` | `Application::Environment` + `Domain::Game` | 对应目录 | Application 编排 + Domain 校验。[已落地] |
| `vpk.signatures` 锁定 / 导入前置保障 | `Application::Common` + `Application::Environment` + `Application::Package` | 对应目录 | `ImportPrerequisiteService` 统一校验基础参数、独占获取 CS2 文件租约，并双重保障 VPK 索引处于可用就绪状态。[已落地] |
| `Ui::CheckForUpdate` | `Application::Update` | `src/Application/Update/` | 自动更新检测。 |
| `Ui::LoadFromCfg`, `SaveToCfg` | `Application::Config` | `src/Application/Config/` | 配置持久化。 |
| `Ui::Start`, 工作线程, `CancelAll` | `Application::Async`（任务服务 `Application::Task`【规划】） | 对应目录 | `AsyncTaskRunner`（`runTask` / `runWorkflowTask` / `runSystemTask`）、`TaskHandle`、`SystemTaskLog`（系统任务平面）与协作式取消；内置异常终态兜底（`fallbackTerminalStateOnFatalException`）与回调异常安全隔离（`invokeCallbackSafely`）。[已落地] |
| `LogViewModel` 直连 `Core::Logging`（`registerWithLogManager` 时代） | `Application::Logging` | `src/Application/Logging/` | `TaskLogService` 日志投递门面 + `TaskLogDTOs` UI 侧值类型；UI 消费日志唯一通道（订阅制投递、陈旧批次抑制、`SinkBridge` 双阶段 `detach()` 协议防 UAF），`src/UI/` 严禁 include `Core/Logging/*`。[已落地] |
| `Ui.h/.cpp` Q_PROPERTY/slots | `UI` | `src/UI/` | 极薄的表现层适配器（`MainController`, `LogViewModel`, `GameViewModel` 联动 `VpkIndexService` 预热），通用 `SourceFileListBox` 组件与左右并排 Tab 布局重构，滚轮防冒泡与智能自动滚动暂停机制。[重构中] |

---

## 2. 重构演进路线图

重构按阶段逐步推进，**严禁为了让临时代码通过编译而跨阶段混杂实现**。

1. **Stage 1 — Core 基础设施解耦提取**（已完成：错误体系与 `Core::Error::ExecutionGuard` 边界防护、文件系统与流式散列取消支持、KeyValues 节点操作、任务导向日志与格式精简、异步取消令牌 `CancellationToken`、`Core::Hash::Sha256` 64KB 流式分块散列校验、`Core::Temp::TempFile` 统一生命周期管理、`Core::Path::FilesystemPath` 路径归属判别与 Win32 目录句柄/Junction 越界防护）
2. **Stage 2 — Domain 领域基础迁移**（已完成：游戏模型/注册表/校验器/插件目录扫描、`Domain::Asset` 资产定位策略解耦 [IAssetSourceProber, ArchiveAssetSourceProber, AssetLocateStrategy]、`Domain::Package` [PackArchive 目录句柄校验, BspPackExtractor, PackArchivePool 细粒度并发同步与防重入缓存, VpkIndex/VpkIndexBuilder]、`Domain::Material` [VtfConverter, VtfCodec/TextureIO/TgaCodec 纹理 IO 与 `TextureProcess` 纹理处理后端，SkyboxCubeBuilder / SkyboxTypes 3D 天空盒立体十字构建器与接缝容错判定]、`Domain::Audio` [Soundscape 解析与转换]、`Domain::Tool` [CS2 官方工具自适应清单与日志解析器]）
3. **Stage 3 — 导入器与领域逻辑迁移**（进行中：`Workflow::Particle` 已落地 [支持多 PCF 批量导入、暂存防重名保护、自适应 `-filelist` 资源编译器批量编译]；`Workflow::Common` 资产定位与提取 [AssetLocator 高保真 exists() 语义、AssetExtractor 统一提取选项 AssetExtractOptions、目标句柄校验与伴生资源取消感知、VpkIndex 点查直接命中与 CS2 原生规整去重] 与 `ImportContext` [延后进度递进] 已就绪；待推进：ModelImporter → `Workflow::Model`、VmfBspProcess → `Domain::Vmf` + `Domain::Bsp`）
4. **Stage 4 — Application 应用编排重构**（进行中：`AsyncTaskRunner` [runWorkflowTask / runSystemTask / SystemTaskLog、致命异常终态回退防护 fallbackTerminalStateOnFatalException、回调异常安全隔离 invokeCallbackSafely]、`TaskHandle`、`TaskLogService` 日志门面、`ImportPrerequisiteService`、`VpkIndexService`、`ParticleImportService`、`SoundscapeConvertService`、`GameEnvironmentService`、`GameInstallationValidator` 已落地；待补齐：统一 ConfigService、UpdateService）
5. **Stage 5 — MapImporter 重构与 UI 瘦身**（进行中：Map/Model/Particle Tab 统一采用 `SourceFileListBox` 与左右并排布局；待推进：MapImporter → `Workflow::Map`、UI 彻底收敛为纯展示与 Application 调用）

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
* [ ] 外部 CLI 工具必须通过 `LogManager::createToolTask` 封装为隐藏任务，多文件编译采用自适应 `-filelist` 临时清单文件，并经 `ProcessOptions` 回调实现流式日志直通重定向（`logExternalToolOutput`）与取消令牌绑定。
* [ ] VPK 资产提取严格基于 `VpkIndex` $O(1)$ 点查或松散文件优先策略，严禁遍历全部 VPK 盲目撞库；CS2 原生资源仅索引 `gameinfo.gi` 定义的 `SearchPaths -> Game` 目录 VPK。
* [ ] VPK 索引后台构建与校验走 `AsyncTaskRunner::runSystemTask` 系统任务平面，不占用可见 UI 任务树。
* [ ] 敏感文件输出（包提取、文件复制与外部文件暂存）必须显式传入 `expectedBaseDir`，以触发底层物理内核句柄验证（`verifyFileWithinBase`），防范符号链接与 Junction 逃逸漏洞。
* [ ] Application / UI 弹窗桥接之外无直接模态对话框调用。
* [ ] 未引入全局静态日志器。
* [ ] 未引入新的全局可变状态。

### 4.5 API 规范
* [ ] UI 接收 Application 契约对象，而非 Domain AST / 底层设施对象。
* [ ] Domain API 使用强领域类型。
* [ ] 纯属性访问器遵循 Qt 风格，严禁添加 `get` 前缀（如 `taskName()`、`index()` 而非 `getTaskName()`、`getIndex()`）。
* [ ] Qt Model 的 `roleNames()` 必须使用局部静态常量缓存（`static const QHash<int, QByteArray>`），容器只读访问器返回常量引用（`const QVector<T>&`）。
* [ ] 枚举声明显式底层类型（`: int`）并保留末项尾随逗号；包含 `Q_ASSERT` 断言的函数严禁声明为 `noexcept`。
* [ ] 跨线程异步服务（如 `ParticleImportService`, `VpkIndexService`）继承 `std::enable_shared_from_this` 时必须通过 PassKey 模式约束 `std::shared_ptr` 所有权，在状态变更前前置校验 `weak_from_this().lock()`；面向 UI 的信号发射必须通过主线程亲和性调度（`dispatch*` 经 `invokeMethod` `QueuedConnection`）隔离。
* [ ] 跨模块 Sink 桥接（如 `TaskLogService::SinkBridge`）必须实现双阶段 `detach()` 协议（原子标记快速跳过 + 互斥锁保护指针置空），杜绝无锁并发日志分发时的并发 UAF。
* [ ] 无状态转换服务（如 `SoundscapeConvertService`）统一声明为静态纯函数并返回有效 `Async::TaskHandle`，杜绝后台线程裸 `this` 捕获。
* [ ] 错误处理结构化并保留诊断上下文；`Core::Error::ExecutionGuard` 保持通用基础设施特性，严禁猜词推断领域错误码。
* [ ] 底层探测器（如 `ArchiveAssetSourceProber`）严禁将失败或取消压制吞没为 `false`；工作流资产定位契约（`AssetLocator::exists()`）高保真反映存在性（CS2 原生存在返回 `true`）。
* [ ] 未重复编写已有 Core 基础设施的功能。
* [ ] 用户可见文案已包裹 `tr()` / `QCoreApplication::translate` 且上下文为类名（i18n 契约，见 AGENTS.md §5.7）；`details()` 与外部工具原始输出保持英文原文。

### 4.6 测试生命周期与分层契约
* [ ] 长期零常驻测试：代码库默认不包含任何常驻测试目标，`CMakeLists.txt` 不默认启用 `enable_testing()`。
* [ ] 临时单任务测试（Task-Scoped / Ephemeral Tests）：仅在单任务开发/修复期间创建以验证行为（`test_tmp_*` 前缀）。
* [ ] 任务完成后用完即删：临时测试及其配置必须在任务完成提交前彻底从代码库和 CMakeLists.txt 中清除，严禁残留任何测试代码。

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
任何业务文件 → 遍历或试探打开多个 VPK 检索单个文件 (盲目撞库试探)

Core 层 → 自然语言关键词猜词推断领域错误（如 msg.contains("vpk") -> ArchiveOpenFailed）
探测/定位层 → 将底层真实错误或取消静默吞没为 false，导致上层将异常误判为 NotFound
异步调度层 → 在回调调用处使用 catch (...) {} 静默吞没异常，缺乏诊断记录
异步服务层 → 继承 enable_shared_from_this 的服务允许以普通栈对象或 unique_ptr 构造并直接调用异步方法
异步服务层 → 在 Worker 线程直接 emit 面向 UI 的 Qt 信号（未通过主线程亲和性调度）
异步服务层 → 跨模块桥接 Sink 析构时未执行双阶段 detach() 解绑（导致无锁并发日志写入 UAF）
文件写入层 → 敏感文件写入（解包/复制）仅作逻辑路径前缀比对而未传递 expectedBaseDir 进行内核物理句柄边界校验
Qt 规范层 → Qt Model 在 roleNames() 中每次动态构造 QHash
Qt 规范层 → 纯属性 Getter 带有 get 前缀（应使用 property() 风格）
Qt 规范层 → 包含 Q_ASSERT 断言的函数被标记为 noexcept
测试管理 → 任务完成后将临时测试目标或测试代码长期残留于代码库中

承担众多杂项职责的庞大静态 Application 服务
无明确移除计划的临时跨层 include
```
