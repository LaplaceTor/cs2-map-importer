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
* **国际化：** Qt Linguist（英文为源语言，zh_CN 翻译经 `translations/cs2importer_zh_CN.ts` 提取；`src/Main.cpp` 启动时按 `QLocale::system()` 自动载入 `.qm`，非中文语系回退英文，无手动切换与配置持久化）

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

* **允许：** 暴露面向 UI 的门面/服务 API；将 UI 契约转换为 Domain/Workflow 输入；编排 `AsyncTaskRunner`、Worker 线程池与工作流；管理持有生命周期的异步服务（如 `ParticleImportService`, `VpkIndexService`：强制 `std::shared_ptr` 共享所有权契约，通过 PassKey 模式约束仅可经 `create()` 静态工厂构造，禁止栈分配与 `std::make_unique`；在异步派发前置阶段必须验证 `weak_from_this().lock()`，杜绝未纳管实例抛出 `bad_weak_ptr`；跨线程 Qt 信号发射必须通过主线程亲和性调度 [如 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 或 `dispatch*` 调度器] 隔离至 UI 主线程，严禁在后台线程直接触发 UI 绑定的信号；状态变更与活动任务计数必须通过 RAII 作用域守护保障，在任务调度抛出异常或启动失败时安全幂等回滚，避免 UI 状态永久挂死）；纯无状态转换服务（如 `SoundscapeConvertService`）统一声明为静态纯函数并返回有效 `Async::TaskHandle`，杜绝后台线程裸 `this` 捕获；持有 `Application::Logging::TaskLogService` 日志门面（订阅制 DTO 分发与日志路径查询，UI 消费日志的唯一通道；其桥接 Sink 必须实现双阶段解绑 `detach()` 协议，防止 `LogManager` 锁外分发并发析构 UAF）；统一管理应用配置、更新与环境检测（Steam 探测、文件租约）；提供弹窗交互抽象接口的具体实现。
* **严禁：** 包含 QML 或直接操作 UI 控件；实现属于 Domain 的数据格式解析或转换；包含属于 Workflow 的具体导入流水线；在已有契约时向 QML 暴露底层 AST/指针细节。

### 3.3 Workflow 规则 (`src/Workflow/`)

* **允许：** 定义具体导入流水线（如 `ParticleImportWorkflow`）；使用 `ImportContext` 处理取消与进度（`runStep` 在步骤成功后延后推进进度）；管理生成的半成品资产生命周期（失败或取消时清理半成品）；导入外部文件需暂存时通过隔离临时文件保障现有用户资产不被覆盖；资产定位与提取严格遵循“松散文件优先、VPK 索引点查直接命中”策略（`Workflow::Common::AssetExtractor` 联动 `VpkIndex`，杜绝跨 VPK 盲目撞库与多次打开），配合 CS2 原生资产规整树对原生已有的基础资源跳过重复解包与编译；资产存在性查询（`Workflow::Common::AssetLocator::exists()`）忠实返回 `Core::Result<bool>`，对 CS2 原生已有资源返回成功 `true`，真实失败/取消原样上报；资源提取采用 `AssetExtractOptions` 参数规整，针对敏感文件输出向包提取器（`PackArchive`）与文件系统辅助类（`FileSystem::copy`）传递预期根目录以触发内核句柄边界验证（`verifyFileWithinBase`），根除 TOCTOU 符号链接与 Junction 替换漏洞；伴随资源提取（`extractCompanions`）严格穿透协作式取消令牌；按序调用 Domain 处理器与工具；返回 `Core::Result<T>` 表达单层业务结果。
* **严禁：** include 或调用 UI / QML；依赖 Application 策略或全局配置；直接弹出交互对话框；自行发现 Steam 或扫描全局环境。

### 3.4 Domain 规则 (`src/Domain/`)

* **允许：** 解析与校验 Valve 专属数据格式；抽象 Source 1/2 领域资产与相对路径；资产检索三层解耦（`Domain::Asset::IAssetSourceProber` 抽象探测接口、`ArchiveAssetSourceProber` 归档池探测适配、`AssetLocateStrategy` 纯领域优先级策略与 `AssetLocation` 定位载体）；归档池并发硬化（`Domain::Package::PackArchivePool` 基于键粒度 `OpeningEntry` 条件变量同步，根除同一归档并发重复打开并响应取消）；包解包安全（`PackArchive::extractEntryToFile` 接收可选基目录，`BspPackExtractor` 结合 `destDir.resolveBelow` 进行目录预创建与文件规划，且均在打开句柄后执行 `verifyFileWithinBase` 真实物理路径校验）；S2 插件扫描严格从 `content/csgo_addons` 目录探测（禁止混入 `game/` 目录）；VPK 资产包索引机制（`Domain::Package::VpkIndex` / `VpkIndexBuilder`，基于 `sourcepp` API 快速构建全量文件树，序列化为紧凑二进制 `.idx`，集成文件大小/修改时间与 `_dir.vpk` SHA-256 完整性快速校验，生成绝对路径 $O(1)$ 路由映射与 CS2 资产主干名集合）；材质纹理图像处理（VTF/TGA 解码、PBR 贴图生成、通道打包等确定性纯计算，见 `Domain::Material::TextureProcess`）；封装官方 CLI 工具（`Domain::Tool`，多资产编译自适应生成 `-filelist` 清单，日志解析器支持资产粒度容错与部分成功统计）；保持确定性与无状态纯计算。
* **严禁：** include `Application/*`、`Workflow/*`、`UI/*` 或 QML 头文件；发送 UI 通知或弹窗；访问应用全局配置或日志器；自行启动线程。

### 3.5 Core 规则 (`src/Core/`)

* **允许：** 提供通用跨平台路径抽象（`FilesystemPath`：严格确立 **Tier 1 逻辑路径包含性** 与 **Tier 2 物理内核句柄验证** 两层安全保证——Tier 1 基于 `std::filesystem::weakly_canonical` 的 `isSubpathOf` 与 `resolveBelow` 逻辑边界解析，拦截盘符冒号 `:`、NTFS ADS 流与跨目录穿越，支持未创建目录的穿透判别；Tier 2 提供 `verifyHandleWithinBase` 与支持 `QFileDevice` [覆盖 `QFile`/`QSaveFile`/`QTemporaryFile`] 的 `verifyFileWithinBase`，基于 Win32 `GetFinalPathNameByHandleW` 校验打开文件对象的真实内核物理路径，彻底免疫 TOCTOU 目录替换与 Junction 逃逸）；通用异常屏障与诊断富化（`Core::Error::ExecutionGuard` 与 `ExecutionContext`，统一捕获并转译异常为结构化 `Result<T>`，禁止在 Core 中硬编码 Valve 业务关键词推测错误）；通用文件系统与散列取消穿透（`Core::FileSystem::FileSystem` 复制/移动全面支持 `CancellationToken` 协作取消与 `expectedBaseDir` 内核句柄安全边界自动校验，越界时安全清除并报错；`Core::Hash::Sha256` 64KB 流式分块计算全面支持 `CancellationToken`）；通用 KeyValues 增强（`KeyValuesNode` 支持位置感知 CRUD 与有序遍历，`KeyValuesDocument` 保持头部注释与格式）；统一临时资源生命周期管理（`Core::Temp::TempFile` 兼具系统临时文件创建与现有路径 RAII 清理托管双模，支持 `dismiss()`/`release()`/`cleanup()` 控制）、进程抽象、日志与结构化错误。
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
5. **任务导向日志与精简格式**：严禁使用全局静态 Logger（如 `Logger::info(...)`）。工作流/工具任务必须通过 `TaskLoggingContext` 显式向下传递；系统任务使用 `SystemTaskLog`（见第 2 条）。
   - **层级化与工作流任务**：顶层导入流程通过 `LogManager::createWorkflowTask` 创建 Workflow 根任务，在 `logs/<workflowName>_<timestamp>/` 下生成独立目录与主工作流日志 `workflow.log`；任务终态为 `Completed` 时强制将进度刷新至 100%；
   - **外部工具隐藏任务（Tool Task）**：外部 CLI 工具（如 `resourcecompiler`, `source1import`, `bspsrc`）必须通过 `LogManager::createToolTask` 创建。Tool 任务从主 UI 任务树中隐蔽（避免日志噪音），父任务接收携带 `toolTaskId` 的 `[EXEC]` 启动通知；工具输出实时流式写入独立文件（`<asset>_<tool>_<timestamp>.log`，位于父任务目录下，同毫秒冲突自动追加 `_2` 序号），经 `logExternalToolOutput` 直通原始行（不加人工等级前缀，保留天然换行），UI 表现层通过独立 `ToolLogWindow` 按需查看；
   - **日志落盘精简规范**：`TaskFileSink` / `FileSink` 统一单任务日志行格式为 `[LEVEL] %2`，去除冗余时间戳与块序列号；新任务日志首行记录结构化元数据头（`=== Task: %1 (ID: %2) | Started: %3 ===`）。
6. **异常边界转译与终态兜底**：Application 服务边界统一通过 `Core::Error::ExecutionGuard`（或 `Application::Execution::ExecutionGuard` 门面）或 `AsyncTaskRunner` 将异常转译为 `Result<T>::failure`，严禁在内部 helper 中静默使用 `catch (...)` 吞没异常。`AsyncTaskRunner` 在顶层工作线程建立致命异常兜底守护（`fallbackTerminalStateOnFatalException`）：未终结的任务遭遇逃逸异常时强制流转至 `TaskState::Failed` 终态（避免任务树挂起死锁），对已终结状态（如 `Completed`）严格幂等保留；回调异常经 `invokeCallbackSafely` 隔离记录至 `ApplicationLogger::error`，防止破坏 Worker 线程。使用 `shared_from_this()` 的异步服务必须通过 PassKey 模式约束 `std::shared_ptr` 所有权，在状态变更前前置校验 `weak_from_this().lock()`，并通过 RAII 作用域守卫在任务启动异常时安全回滚活动计数。
7. **消息创建处翻译 (i18n)**：面向用户的消息（`Result::message()`、`Error::message()`、任务日志摘要、对话框文案、QML `qsTr()`）必须在**创建处**翻译——QObject 类用成员 `tr()`，非 QObject 类用 `QCoreApplication::translate("<类名上下文>", "...")`，字符串表用 `QT_TRANSLATE_NOOP` 标记。日志文件内容随界面语言变化。**不翻译**：`Error::details()` 技术诊断、外部工具原始输出、`debug()`/系统日志行、日志等级与导出格式串、游戏产品名。
8. **线程亲和性发射与 Sink 解绑安全**：
   - **信号发射亲和性**：异步服务（如 `VpkIndexService`）在 Worker 线程执行完成后，面向 UI 的 Qt 信号必须通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 投递回对象宿主线程（主 UI 线程）发射，严禁在后台工作线程直接触发 UI 绑定的信号；
   - **Sink 桥接解绑协议**：跨模块 Sink 桥接（如 `TaskLogService::SinkBridge`）必须实现双阶段解绑（原子标记 `std::atomic<bool> m_detached` 快速跳过 + 互斥锁保护指针置空），确保门面析构与日志器无锁写入无竞争与悬空解引用（UAF）；
   - **无状态服务规范**：纯数据/文件格式转换服务（如 `SoundscapeConvertService`）统一声明为静态纯函数并暴露有效 `Async::TaskHandle`，彻底杜绝后台线程裸 `this` 捕获。

> 💡 **详细规范与完整决策表**：请查阅专用技能 [`skills/cs2-async-error-handling/SKILL.md`](file:///c:/Users/KEY/Documents/GitHub/cs2-map-importer/skills/cs2-async-error-handling/SKILL.md) 获取三平面任务体系、终态冲突仲裁矩阵、构造正反模式代码及进程机械结果转译规则。

---

## 6. 系统交互与外部工具规范

1. **禁止直接操作外部进程**：业务代码严禁使用 `QProcess`、`system()`、`popen()` 或 `WinExec()`。
2. **外部工具强类型封装**：所有外部 CLI 工具（`bspsrc`，官方 `resourcecompiler`, `source1import`）必须封装于 `Domain::Tool` 并通过 `Core::Process::ProcessRunner` 执行。
3. **流式输出与取消绑定**：`ProcessRunner` 必须支持基于 `onStdOutLine` / `onStdErrLine` 的逐行实时流式日志捕获，并与 `CancellationToken` 强绑定，严禁无超时的静默阻塞式黑盒调用。
4. **批量参数清单与长度防护**：调用外部 CLI 编译或处理批量资源时，当文件数量 > 1，必须自适应采用由 `Core::Temp::TempFile` 管理的临时清单文件（如 `-filelist <path>`），严禁将海量文件路径直接拼接入命令行以防超出 Windows 命令行长度上限。
5. **内嵌原生库与持久化索引替代**：严禁再引入或调用外部 `vpkeditcli` 与 `vtfcmd`，归档解包与 VTF 解码/转码已全量由内嵌原生库在进程内完成——`Domain::Package` 基于 `sourcepp`（vpkpp/bsppp）解包 VPK 与 BSP 嵌入包；严禁使用盲目打开 VPK 碰撞试探文件是否存在的方式，必须通过 `Domain::Package::VpkIndex` / `Application::Package::VpkIndexService` 建立持久化二进制索引并基于 `AssetExtractor` 执行 $O(1)$ 点查直接命中；CS2 原生资源仅检索 `gameinfo.gi` 中定义的 `SearchPaths -> Game` 目录 VPK，规避海量无用扫描；`Domain::Material` 基于 `vtfpp` 解码 VTF（`VtfCodec` / `VtfConverter`），`TgaCodec` 为自包含 TGA 编解码实现，纹理读写统一收口于 `TextureIO`（宽读取、导出仅 PNG）。
6. **用户交互解耦**：Domain / Workflow 严禁直接弹出模态对话框。必须通过抽象 Prompt 接口定义契约，由 Application 实现并调度 UI 呈现。

---

## 7. 目标目录结构

```text
src/
├── Core/             # 通用基础设施 (Async, Error, FileSystem, Hash, KeyValues, Logging, Path, Process, Result, Temp)【全部已有】
├── Domain/           # Valve/Source 专有领域模型 (Asset, Audio, Game, Material（含 TextureProcess 纹理处理后端）, Package（含 VpkIndex/VpkIndexBuilder）, Tool【已有】; Bsp, Vmf【规划】)
├── Workflow/         # 具体导入流水线 (Common, Particle【已有】; Map, Model【规划】)
├── Application/      # 应用服务与任务调度 (Async, Common, Environment, Execution, Logging, Package, Particle, Soundscape【已有】; Config, Task, Update【规划】)
├── UI/               # 表现层 ViewModel 与控制器 (Controllers, ViewModels)【全部已有】
└── qml/              # QML 界面视图与组件 (cs2importer/components, cs2importer/tabs, Main.qml)【全部已有】
```

`src/Legacy/` 仅用于过渡，新代码严禁依赖 Legacy。当前 `src/Legacy/` 未接入默认构建（`src/CMakeLists.txt` 未对其执行 `add_subdirectory`），仅作迁移参照保留。

仓库根目录 `translations/` 存放 Qt Linguist 翻译源文件（`cs2importer_zh_CN.ts`）；新增语言仅需追加对应 `.ts` 并在 `src/CMakeLists.txt` 的 `qt_add_translations` 中登记。

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
* `cs2importer_domain` 仅链接 Core（指项目层目标）；
* **Qt 模块与第三方库不属于项目分层**：各层目标可按需链接 Qt 模块（如 `cs2importer_domain` 因 Material 纹理 IO 公共链接 `Qt6::Gui`），第三方库由最底层实际消费模块 `PRIVATE` 链接（见 §9），均不构成跨层违规；
* `cs2importer_workflow` 链接 Domain + Core；
* `cs2importer_application` 链接 Workflow + Domain + Core；
* `cs2importer_ui` 链接 Application 及 Qt 模块。**严禁在 `src/UI/CMakeLists.txt` 中添加对 `cs2importer_domain` 或 `cs2importer_core` 的直接链接。**
* **临时测试红线**：代码库无常驻单元测试工程（`tests/` 目录与 CTest 集成已全量移除）；单任务临时测试必须用完即删，**严禁将测试目标或测试依赖残留于 CMakeLists.txt 中**。
* **翻译构建红线**：`LinguistTools` 仅在根 `CMakeLists.txt` 引入；`qt_add_translations` 仅挂载于主程序目标 `cs2importer`（`SOURCE_TARGETS` 显式列出六个自有层目标，严禁扫描 `third_party` / FetchContent 产物）；部署脚本严禁恢复 `NO_TRANSLATIONS`。新增字符串后通过 `update_translations` 目标运行 lupdate 提取。

---

## 9. 编码与构建规范

* **C++ 标准**：C++20，适度使用 Qt 类型，严格遵循 RAII、值传递/移动语义与 `const` 正确性。
* **平台限制**：**仅限 Windows**。根目录 `CMakeLists.txt` 统一执行 `if(NOT WIN32)` 报错守卫；严禁添加或保留非 Windows 条件编译（`#ifdef Q_OS_WIN` 等）；全局注入 `NOMINMAX WIN32_LEAN_AND_MEAN` 宏守卫。
* **命名与 API 规范**：类名与枚举采用 `PascalCase`；函数、变量采用 `camelCase`；**纯属性访问器遵循 Qt 风格，严禁添加 `get` 前缀**（统一使用 `taskName()`、`index()`，禁止 `getTaskName()`、`getIndex()`）。
* **Qt 模型与类型安全**：Qt 模型 `roleNames()` 必须使用局部静态常量缓存（`static const QHash<int, QByteArray>`），避免频繁动态构建；容器只读访问器返回常量引用（如 `const QVector<T>&`）；枚举必须声明显式底层类型（如 `enum Role : int`）并保留末项尾随逗号；包含 `Q_ASSERT` 的函数严禁声明为 `noexcept`（防止断言触发 `std::terminate` 绕过异常处理与测试）。
* **国际化规范**：严禁硬编码面向用户的 `QStringLiteral` 文案——QObject 类用 `tr()`，非 QObject 类用 `QCoreApplication::translate("<类名上下文>", "...")`，字符串表用 `QT_TRANSLATE_NOOP`；`details()`、路径、CLI 参数与外部工具原始输出保持英文原文。
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

* **无常驻单元测试**：仓库当前不维护长期常驻的单元测试目录或工程配置（`tests/` 目录已移除）；
* **单任务临时验证（用完即删）**：针对 Core、Domain、Workflow、Application、UI 层的验证测试，均严格定义为**临时单任务测试（Task-Scoped / Ephemeral Tests）**。仅用于在研发、重构或定位缺陷的单个任务期间进行即时验证。**一旦任务完成，必须立即清理或删除，严禁将包含上层复杂依赖或临时配置的测试留存在代码库中**。临时测试代码不得合入主分支。

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
* 严禁将任何临时任务测试残留于主干代码库。
* 严禁为配置、服务、取消标志或日志器添加全局静态变量。
* 严禁在业务代码中直接调用 `QProcess`、`system()` 或 Shell 命令。
* 严禁从 Domain / Workflow 中弹出模态对话框。
* 严禁将“通过编译”等同于“架构设计正确”。

> **终极思考准则**：
> 正确的思考出发点不是：“这段代码写在哪里能让当前的构建通过？”
> 而是：“**哪个分层拥有该职责？跨越该边界的公开契约是什么？如何在不让任何层感知其上层的前提下优雅实现它？**”

