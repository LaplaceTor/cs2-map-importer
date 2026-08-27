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

用于将 Source 1 游戏资产（地图、模型、粒子等）导入至 Counter-Strike 2 的 Windows 桌面 GUI 应用程序。

* **开发语言：** C++17
* **技术框架：** Qt 6.8+
* **构建系统：** 现代 CMake
* **目标平台：** 仅限 Windows（程序仅供 Windows 构建与运行）
* **UI 技术：** QML / Qt Quick Controls 2
* **QML 样式：** Fusion

当前 Core 层已完全解耦提取，Domain 基础与 Application 环境服务正逐步迁移，Workflow 及剩余遗留模块仍处于过渡阶段。

---

## 2. 目标分层架构

```text
┌──────────────────────────────────────────────────────────────┐
│ Presentation / UI (表现层)                                   │
│ QML 视图 <-> ViewModels / Controllers                        │
│                                                              │
│ 允许: Qt/QML + Application 契约                              │
│ 禁止: 直接编排 Domain/Core                                   │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用 / 信号槽连接
┌─────────────────────────────▼────────────────────────────────┐
│ Application (应用层)                                         │
│ 服务 / 任务编排 / 配置 / 自动更新 / 环境感知                  │
│                                                              │
│ 将 UI 契约转换为 Domain/Workflow 输入                        │
│ 负责异步调度、生命周期控制及面向 UI 的结果封装               │
└─────────────────────────────┬────────────────────────────────┘
                              │ 触发 / 组装
┌─────────────────────────────▼────────────────────────────────┐
│ Workflow (工作流层)                                          │
│ 具体导入用例 / 流水线编排 / 取消响应                         │
│                                                              │
│ 可使用 Domain + Core                                         │
│ 严禁依赖 UI 或 Application                                   │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用
┌─────────────────────────────▼────────────────────────────────┐
│ Domain (领域层)                                              │
│ Valve / Source 1/2 业务规则、领域模型、解析器、处理器、工具  │
│                                                              │
│ 仅可调用 Core                                                │
│ 无 UI、Application、Workflow 依赖                            │
└─────────────────────────────┬────────────────────────────────┘
                              │ 调用底层设施
┌─────────────────────────────▼────────────────────────────────┐
│ Core (基础设施层)                                            │
│ 路径 / 文件系统 / KeyValues / 进程 / 日志 / 临时文件 / 错误 │
│                                                              │
│ 无业务逻辑 / 无 Valve 专用策略 / 无 UI                       │
└──────────────────────────────────────────────────────────────┘
```

### 规范依赖流向

```text
Presentation → Application → Workflow → Domain → Core
```

Application 亦可直接调用 Domain/Core 提供的非工作流服务，但 **UI 层严禁跨层直接调用**。

严禁出现**逆向依赖**：

```text
Core        ✗→ Domain / Workflow / Application / UI
Domain      ✗→ Workflow / Application / UI
Workflow    ✗→ Application / UI
Application ✗→ UI
```

### Workflow 与 Application 分离原因

* **Application** 回答：*在何种应用策略、执行上下文与时机下运行？*
* **Workflow** 回答：*具体资产导入用例如何从输入推进到输出？*
* **Domain** 回答：*Valve/Source 专有规则与转换逻辑代表什么？*
* **Core** 回答：*如何安全执行通用的底层系统/基础设施操作？*

切勿因为旧代码只有一个 `Ui::Start()` 函数就将具体导入流水线塞进 `Application`。

---

## 3. Agent 强制架构规则

### 3.1 表现层 / UI 规则

`src/UI/` 与 `src/qml/` 仅负责界面呈现。

UI **允许**：

* 暴露 `Q_PROPERTY`、Qt 信号/槽、命令与界面展示状态；
* 校验基础界面输入（判空、Tab 切换、必要控件状态）；
* 调用 Application 服务/门面；
* 将 UI 本地数据转换为 **Application 层定义的契约 DTO**；
* 展示 Application 返回的错误、进度、交互弹窗与结果。

UI **严禁**：

* 直接 include `Domain/*`；
* 直接 include `Core/*` 用于执行业务或文件系统/进程操作；
* 调用 `Domain::*` 校验器、注册表、解析器或处理器；
* 调用 `Core::Process::*`、`Core::FileSystem::*`、`Core::KeyValues::*` 等执行应用任务；
* 扫描游戏目录、解析 `gameinfo`、检索 Steam 库、解压资产包、调用外部工具或修改导入资产；
* 决定调用哪个 Domain 校验器/处理器/工具；
* 拥有工作流线程、Worker 线程池、取消令牌或导入生命周期；
* 创建 `QProcess` 或执行 Shell 脚本/命令；
* 在当前导入中使用全局/静态业务状态。

### 3.2 Application 规则

`src/Application/` 包含应用层服务与业务编排。

Application **允许**：

* 暴露面向 UI 的服务/门面 API；
* 将 Application 契约转换为 Domain/Workflow 输入；
* 编排 `WorkflowRunner` 与异步任务执行；
* 管理异步调度与 Worker 线程池策略；
* 协调 Steam 探测与游戏安装识别服务；
* 协调应用配置持久化、更新检测与生命周期策略；
* 将日志、进度、结果通道连接至 UI 适配器；
* 实现下层依赖中立接口（如用于弹窗交互的抽象接口）。

Application **严禁**：

* 包含 QML 或直接操作 UI 控件；
* 实现属于 Domain 层的 Valve 格式解析或资产转换；
* 包含属于 Workflow 层的具体地图/模型/粒子导入流水线；
* 为图省事绕过 Workflow 直接调用旧版导入入口；
* 在已有稳定 Application 契约时向 QML 暴露底层实现细节。

### 3.3 Workflow 规则

`src/Workflow/` 包含具体资产导入用例。

Workflow **允许**：

* 定义 `IImporter`、`ImportContext`、取消机制、任务级进度与流水线结果类型；
* 按序调用 Domain 处理器与工具；
* 管理导入用例专属的执行步骤与错误处理；
* 从 Application 接收 `TaskLoggingContext` 与取消令牌；
* 在需要时直接使用 Core 基础设施；
* 返回 `Core::Result<T>` 或 `Result<void>` 明确表达 `Success`、`Failure`、`Cancelled` 与 `Skipped` 业务状态。

Workflow **严禁**：

* include 或调用 UI 类 / QML；
* 依赖 Application 策略或全局配置类；
* 直接弹出对话框或在 QML 中请求用户交互；
* 拥有全局应用配置状态；
* 自行发现 Steam 安装或执行全局环境判定（除非该行为作为参数由上层传入）；
* 使用 `std::optional<T>` 或 `bool` 隐式传递业务执行结果。所有新 Workflow API 必须严格使用 `Result<T>`。

### 3.4 Domain 规则

`src/Domain/` 包含 Valve/Source 专有知识与确定性领域操作。

Domain **允许**：

* 解析与校验 Valve 专属数据格式；
* 抽象 Source 1/2 领域概念；
* 解析游戏资产搜索路径（Search Paths）；
* 处理 VMF/VMT/VMAT/BSP/VPK 等领域资产；
* 在 `Domain::Tool` 下通过类型安全接口封装外部 CLI 工具。

Domain **严禁**：

* include `Application/*`、`Workflow/*`、`UI/*` 或 QML 相关头文件；
* 发送 UI 通知或直接弹出对话框；
* 访问 `QQmlApplicationEngine`、`QGuiApplication`、QWidget 或表现层类；
* 决定应用生命周期、重试策略、Steam 发现策略、配置持久化或更新策略；
* 调用全局应用日志器。

若 Domain 操作需要用户确认，应在下层边界（如 `Workflow/Common` 或通用抽象接口）定义轻量接口，由 Application 提供具体实现。Domain 层严禁感知 QML。

### 3.5 Core 规则

`src/Core/` 为通用的底层基础设施。

Core **严禁包含**：

* 游戏定义与 CS2/CSGO/HL2 专有规则；
* 导入工作流决策；
* Steam 探测逻辑；
* VPK 业务策略；
* 材质转换逻辑；
* UI / QML 代码；
* Application 服务类。

Core API 必须保持通用性，可无缝脱离本项目复用。

---

## 4. UI 与 Application 边界契约

### 推荐调用范式

```text
QML
  ↓
ViewModel / Controller
  ↓
Application service / facade
  ↓
Workflow / Domain
  ↓
Core
```

### Application 契约规范

面向 UI 的 Application API 应优先使用 **Application 自有 DTO/值对象** 或基础 Qt 值类型，避免要求 UI 代码构造 Domain/Core 内部实现类型。

推荐做法：

```cpp
Application::Environment::ValidateGameRequest request;
request.path = selectedPath;
request.gameId = selectedGameId;

auto result = gameEnvironmentService->validate(request);
```

严禁做法：

```cpp
// 错误：UI 层直接构造 Domain 实现细节
auto type = Domain::Game::GameRegistry::stringToGameType(selectedType);
auto path = Core::Path::FilesystemPath(selectedPath);
auto result = Domain::Game::GameValidator::validateDirectory(path, type);
```

Application 服务应承担数据转换职责：

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

---

## 5. 异步与多线程规范

所有潜在的阻塞操作均必须置于非 UI 线程执行。

常见耗时操作：文件系统扫描、大文件解析、Steam 库遍历、涉及磁盘 I/O 的游戏校验、VPK 解包、外部进程调用、地图/模型/粒子导入流水线。

规则：

1. UI 线程必须时刻保持高响应。
2. Application 层统一管理 Worker 线程调度。
3. Workflow 执行全部在非 UI 线程进行。
4. UI 状态刷新必须通过 Qt 排队连接（Queued Connection）或线程安全机制传递。
5. 严禁未作生命周期守卫即捕获 UI 对象至后台异步任务中。
6. Domain 代码严禁直接操控 UI 事件循环同步。
7. 取消机制必须显式、协作式传递，严禁使用临时共享全局标志。

### 双平面架构：任务执行生命周期 vs 业务执行结果

为保证异步任务与工作流操作的概念严密性，架构定义了两个正交平面：

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. 任务执行生命周期平面 (Task Execution Lifecycle Plane: TaskState)        │
│    由 LogManager / TaskLoggingContext 管理                                  │
│    状态流转: Pending → Running → Completed | Failed | Cancelled | Skipped    │
│    在 UI 日志模型中跟踪展示 (LogViewModel / LogTaskModel)                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. 业务执行结果平面 (Business Outcome Plane: Result<T>)                     │
│    Workflow 与 Application API 的标准单层返回契约                           │
│    结果状态: Success | Failure | Cancelled | Skipped + 业务负载 T 与说明文本 │
└─────────────────────────────────────────────────────────────────────────────┘
```

`AsyncTaskRunner` 是连接两个平面的核心桥梁：
* **标准 API 体系**：
  * `AsyncTaskRunner::runTask<T>(taskName, context, worker, callback)`: 用于产出业务负载 `T` 的异步任务。
  * `AsyncTaskRunner::runTask<void>(taskName, context, worker, callback)`: 用于无返回值的异步任务，保持完整的 `Result<void>` 语义。
  * `AsyncTaskRunner::runChildTask<T>(parentTaskId, taskName, context, worker, callback)`: 用于层次化子任务。
  * `AsyncTaskRunner::runChildTask<void>(parentTaskId, taskName, context, worker, callback)`: 用于无返回值子任务。
  * `AsyncTaskRunner::runBackground(taskName, worker)`: 委派至 `runTask<void>` 的后台便捷封装，Worker 必须返回 `Core::Result<void>`，复用同一套生命周期与异常仲裁逻辑。
* `T` 为**业务负载类型**（如 `GameInstallationInfo`、`DetectionResult`、`void`），严禁嵌套为 `Result<Result<T>>`。
* Worker 返回单层 `Result<T>`。
* `AsyncTaskRunner` 结合业务结果、日志报错与捕获的异常，驱动 `LogManager` 中的 `TaskState` 状态转移。
* 回调函数接收 `const Result<T>&`，并线程安全地投递至调用方所在线程。

#### Result 语义契约

* **`status()` / `isSuccess()` / `isFailure()` / `isCancelled()` / `isSkipped()`**：业务结果分支判定的权威来源。严禁仅凭 `errorCode()` 判定业务成败。
* **`error()`**：机器可解析的结构化错误对象（`Core::Error::Error`），包含：
  * `error().code()`：用于路由分支的标准 `ErrorCode` 枚举；
  * `error().message()`：领域/系统层面的具体失败原因（如 `"gameinfo.gi not found"`）；
  * `error().details()`：技术诊断数据（如文件路径、stderr 输出、语法行号）。
* **`message()`**：面向用户/UI 的高层操作总结（如 `"CS2 校验失败"`）。未设置时自动回退为 `error().message()`；处于 `Skipped` 状态时携带具体跳过原因。
* **`details()`**：直接代理 `error().details()`。

##### 三层诊断分层规范 (Tripartite Diagnostic Contract)

`Result::failure(error, message)` 同时保存底层 `Error` 与高层操作摘要 `message`。访问 `result.message()` 时，优先返回显式设置的操作摘要；未设置时自动回退为底层 `error().message()`。

三层字符串职责分工与调用约定如下：

| 诊断层级 | 访问接口 | 归属层级 | 语义职责 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| **操作总结 (Operation Summary)** | `Result::message()` | Workflow / Application | 面向用户/任务的全局高层概括，说明**哪个宏观操作失败或成功**。 | `"地图 'de_dust2' 导入失败"`, `"CS2 环境验证失败"`, `"Steam 探测失败"` |
| **失败原因 (Failure Reason)** | `Error::message()` | Domain / Core | 具体领域或底层系统原因，说明**为何发生失败**。 | `"gameinfo.gi 未找到"`, `"实体解析语法错误"`, `"file missing"` |
| **技术诊断 (Technical Diagnostics)** | `Error::details()` / `Result::details()` | Domain / Core / Process | 供排查问题的底层技术诊断数据（绝对路径、stderr 输出、AST 行号、CLI 参数、退出码等）。 | `"C:/Steam/steamapps/common/CS2/game/csgo/gameinfo.gi"`, 编译器 stderr 输出 |

##### 构造反模式与规范模式

* ❌ **反模式 1：将高层操作总结挤占进 `Error.message`**
  ```cpp
  // 错误：丢失了具体缺少哪个文件及底层真实原因
  return Result<void>::failure(ErrorCode::FileNotFound, "无法导入地图");
  ```
* ❌ **反模式 2：把文件绝对路径等技术细节硬编码进 `Error.message`**
  ```cpp
  // 错误：污染 UI 错误文案，破坏错误分类归纳
  return Result<void>::failure(ErrorCode::FileNotFound, "gameinfo.gi 未在 C:/Games/CS2/gameinfo.gi 找到");
  ```
* ❌ **反模式 3：三层字符串职责混乱与重复**
  ```cpp
  // 错误：error.message 塞了操作总结，result.message 塞了另一个错误原因，details 塞了重复文本
  auto err = Core::Error::Error(ErrorCode::FileNotFound, "Steam 探测失败", "找不到 gameinfo.gi 路径");
  return Result<void>::failure(err, "gameinfo.gi 文件缺失"); // 三层互相重复且颠倒
  ```
* ✅ **规范模式 1：多层结构化失败封装（推荐）**
  ```cpp
  // 正确：Error 记录具体原因与技术细节，Result 包装宏观操作总结
  auto err = Domain::Game::GameErrors::gameInfoNotFound("file missing", gamePath.toQString());
  return Result<void>::failure(
      err,
      QStringLiteral("Steam 游戏探测失败") // Result.message: 操作总结
  );
  // 最终：
  // error.message()  = "file missing" (具体失败原因)
  // result.message() = "Steam 游戏探测失败" (宏观操作总结)
  // result.details() = "C:/Steam/..." (技术诊断细节)
  ```
* ✅ **规范模式 2：带技术细节的便捷重载**
  ```cpp
  // 正确：ErrorCode + 明确原因 + 技术路径
  return Result<void>::failure(
      Core::Error::ErrorCode::FileNotFound,
      QStringLiteral("gameinfo.gi not found"), // Error.message
      gamePath.toQString()                     // Error.details
  );
  ```

##### 值访问规范 (Value Access Contract)

* **`hasValue()` / `isSuccess()` 前置检查**：访问业务负载 `result.value()`、`operator*`、`operator->` 前，**必须**显式检查。在无值状态下调用 `.value()` 会抛出 `std::bad_optional_access`。
* **禁止基于异常的业务控制流**：
  * ❌ 严禁使用 `try { auto v = res.value(); } catch (...) {}` 进行控制流分支。
  * ✅ 规范做法：`if (res.isSuccess()) { ... } else { ... }` 或使用 `res.valueOr(...)`。

##### 异常处理与转译分层规范 (Exception Handling & Translation)

业务主干使用 `Result<T>` 显式单层传递，底层异常在系统边界统一转译：
* **`Core::Error::Exception`**：项目专用的结构化异常传输类型（派生自 `std::exception` / `QException`），携带强类型 `Core::Error::Error`。用于深层调用栈快速跳出。
* **`std::exception`**：标准库与第三方库异常兜底。
* **`catch (...)`**：未知系统异常最终防线。

在异步调度入口（`AsyncTaskRunner`）或服务边界处通过 `ExecutionGuard` 统一转译为 `Result<T>::failure`：
```cpp
try {
    return executeOperation();
} catch (const Core::Error::Exception& ex) {
    return Result<T>::failure(ex.error());
} catch (const std::exception& ex) {
    return Result<T>::failure(Core::Error::ErrorCode::OperationFailed, QString::fromUtf8(ex.what()));
} catch (...) {
    return Result<T>::failure(Core::Error::ErrorCode::Unknown, QStringLiteral("Unknown runtime exception caught"));
}
```

##### 异常捕获与转译边界契约 (Exception Boundary & Handling Contract)

> **异常只能在明确的异常边界被捕获。Application API 边界必须将异常转换为 `Core::Result<T>`；Application 内部 helper 默认不得通过 `catch (...)` 将异常静默转换为空值、空容器、`false` 或 `nullptr`。若异常确实代表合法的 best-effort fallback，必须在注释中说明该 fallback 语义，并确保不会掩盖业务失败。**

* **Application Public Operation**：
  * 必须返回 `Core::Result<T>`；
  * 必须通过 `Application::Execution::ExecutionGuard` 或 `AsyncTaskRunner` 统一守卫异常边界，将 `Core::Error::Exception`、`std::exception` 与未知异常转译为单层结构化 `Result<T>::failure`。
* **Application Internal Helper**：
  * 可以返回普通值类型（如 `FilesystemPath`、`std::vector<T>`、`QString`、`std::optional<T>` 等）；
  * 默认**不得**通过 `catch (...)` 静默吞没异常并返回空值；
  * 异常应当自然向上冒泡到调用它的 Public Operation 或 Task Worker 统一转译处理，避免掩盖底层真实错误与技术诊断。

##### 强类型 Domain 错误码扩展规范

`Error::domain`、`Error::is` 与 `Error::domainCodeAs` 接口受 `static_assert(std::is_enum_v<EnumT>)` 约束，必须传入强类型 `enum` / `enum class`（如 `Domain::Game::GameErrorCode`），严禁使用整型魔法数。

#### 终态严重性级联规则 (Terminal Severity Hierarchy)

跨终态仲裁遵循单一确定性原则：

> **Failure 拥有最高优先级，压倒其他所有终态（无论由生命周期还是业务结果产生）。**

优先级顺序：
1. **存在任何 Failure**（`TaskState::Failed`、日志报错或 `Result::failure`）→ 终态为 **`Failed`**，最终结果为 **`Failure`**。
2. **否则，存在任何 Cancelled**（`TaskState::Cancelled` 或 `Result::cancelled`）→ 终态为 **`Cancelled`**，最终结果为 **`Cancelled`**。
3. **否则，存在任何 Skipped**（`TaskState::Skipped` 或 `Result::skipped`）→ 终态为 **`Skipped`**，最终结果为 **`Skipped`**。
4. **否则** → 终态为 **`Completed`**，最终结果为 **`Success`**。

关系：
- `Cancelled` + `Failure` → **`Failed`**
- `Skipped` + `Failure` → **`Failed`**
- `Skipped` + `Cancelled` → **`Cancelled`**

#### 跨终态冲突仲裁矩阵 (State Conflict Arbitration Matrix)

| Context `TaskState` | Worker `Result` | 是否记录契约违规日志 | LogManager 最终 `TaskState` | 回调接收的最终 `Result<T>` |
| :--- | :--- | :--- | :--- | :--- |
| **Failed** (或有错误日志) | **Success** | `error()` ("... 任务失败后返回了 success") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Failed** (或有错误日志) | **Failure** | 否（达成一致） | `Failed` | `Failure`（保留原错误） |
| **Failed** (或有错误日志) | **Cancelled** | `error()` ("... 任务失败后返回了 cancelled") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Failed** (或有错误日志) | **Skipped** | `error()` ("... 任务失败后返回了 skipped") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Cancelled** (无错误) | **Success** | `warning()` ("... 任务取消后返回了 success") | `Cancelled` | 强制转为 `Cancelled` |
| **Cancelled** (无错误) | **Failure** | `warning()` ("... 任务取消后返回了 failure") | `Failed` | `Failure`（保留原错误） |
| **Cancelled** (无错误) | **Cancelled** | 否（达成一致） | `Cancelled` | `Cancelled`（保留原状态） |
| **Cancelled** (无错误) | **Skipped** | `warning()` ("... 任务取消后返回了 skipped") | `Cancelled` | 强制转为 `Cancelled` |
| **Skipped** (无错误) | **Success** | `warning()` ("... 任务跳过后返回了 success") | `Skipped` | 强制转为 `Skipped` |
| **Skipped** (无错误) | **Failure** | `warning()` ("... 任务跳过后返回了 failure") | `Failed` | `Failure`（保留原错误） |
| **Skipped** (无错误) | **Cancelled** | `warning()` ("... 任务跳过后返回了 cancelled") | `Cancelled` | `Cancelled`（保留原状态） |
| **Skipped** (无错误) | **Skipped** | 否（达成一致） | `Skipped` | `Skipped`（保留原状态） |
| **Completed / Running** | **Success** | 否（达成一致） | `Completed` | `Success`（保留原结果） |
| **Completed / Running** | **Failure** | 否（状态顺推） | `Failed` | `Failure`（保留原错误） |
| **Completed / Running** | **Cancelled** | 否（状态顺推） | `Cancelled` | `Cancelled`（保留原状态） |
| **Completed / Running** | **Skipped** | 否（状态顺推） | `Skipped` | `Skipped`（保留原状态） |

*负载保全规则 (Value Preservation Rule)*：当 `Result<T>` 因契约冲突转换状态时，已有的部分数据负载（`result.value()`）严格予以保留。
*原始错误保留规则 (Original Error Preservation Rule)*：因契约违规转换为 `Failure` 时，若原 `Result` 中已含有非成功错误信息，完整保留其错误码与诊断细节；仅当原结果无有效错误（如原为 `Success`）时才合成 `OperationFailed`。违规描述由 `taskContext->error()` 记入日志并在 `Result::message()` 操作总结中呈现。

#### 异常边界 vs 业务错误数据模型

* **`Core::Error::Error`**：**稳定的业务错误数据模型**，跨 Domain、Workflow、Application 与 UI 的标准传递值对象。
* **`Core::Error::Exception`**：**控制流异常边界**，Qt 跨线程异常传输载体（`QException`）。

##### 1. 返回 `Result` 与抛出 `Exception` 的界定规则

* **正常业务失败路径**：所有可预测的操作失败、解析错误、校验未通过、I/O 失败等，**必须严格返回** `Core::Result<T>::failure(error)`。严禁就常规业务条件抛出 `Exception`。
* **抛出 `Exception` 的合法场景**：
  1. **不可恢复的内部断言/状态损坏**；
  2. **第三方/外部 C++ 异常包装**（在集成边界捕获并转译）；
  3. **深层跨栈帧打断**（中继栈帧无法传递 `Result<T>` 的旧有非领域代码）。
* **严禁混用控制流**：禁止在常规业务分支中交叉混用 `throw Exception` 与 `return Result`。

##### 2. 三层上报职责与 UI 去重规范

| 上报通道 | 目标接收方 | 语义职责 | 内容格式与策略 |
| :--- | :--- | :--- | :--- |
| **任务日志消息 (Task Log)** | `TaskLoggingContext` (`taskContext->error(...)`) | **技术诊断细节** | 完整技术调用栈、`ErrorCode`、具体失败原因、`details()`（绝对路径、CLI 参数、stderr）或 `ex.what()`。展开日志时查看。 |
| **任务终态总结 (Task Summary)** | `LogManager` / `TaskSnapshot.currentMessage` | **简明宏观状态** | 简短状态文本（如 `"任务异常终止"`, `"校验失败"`）。**严禁**塞入长字符串、路径或堆栈。 |
| **业务结果 (Result)** | 调用方 / UI 回调 (`Result<T>`) | **结构化业务结果** | 包含 `status()`、`message()`（操作总结）与结构化 `Error`，用于程序分支、UI 徽章或 Toast 提示。 |

#### 错误码分层：Core vs Domain 错误域

* **`Core::Error::ErrorCode`**：通用基础设施/系统错误（`InvalidArgument`, `InvalidPath`, `DirectoryNotFound`, `FileNotFound`, `ProcessFailed` 等），无 Valve/游戏专有概念。
* **Domain 错误码（如 `Domain::Game::GameErrorCode`）**：细粒度业务错误（`UnsupportedGame`, `GameInfoNotFound`, `GameTypeMismatch`, `SteamAppMismatch`, `InvalidGameInstallation` 等）。
* **禁止领域错误侵入底层事实**：
  * 低层路径语法错误（`!path.isValid()`）必须返回 `Core::ErrorCode::InvalidPath`。
  * 磁盘目录不存在（`!dir.exists()`）必须返回 `Core::ErrorCode::DirectoryNotFound`。
  * 严禁将底层文件/路径错误吞没并伪装为领域错误（如 `GameInfoNotFound`）。
* **领域错误工厂唯一入口契约**：
  * `Domain::Game::GameErrors` 是构建领域业务错误对象的**唯一权威入口**。
  * 上层（Application/Workflow/UI）严禁随意凭空构造 Domain 业务错误，必须消费 Domain 产生的错误并包装宏观操作总结。
* **错误检测契约**：
  * 基础设施状态判定：`err.is(Core::Error::ErrorCode::InvalidPath)`
  * 领域业务判定：`err.is(Domain::Game::GameErrorCode::GameInfoNotFound)` 或 `err.domainCodeAs<GameErrorCode>()`

#### 启发式推导 (`try*` -> `std::optional<T>`) vs 确定性校验 (`validate*` -> `Result<T>`)

* **确定性校验 (`validate*` -> `Result<T>`)**：用于对目标进行严格的前置契约校验。失败代表操作受阻，需通过 `Result<T>::failure(Error)` 阐明原因。
* **启发式识别 (`tryIdentify*` -> `std::optional<T>`)**：用于尽力而为的模式推导（如 `tryIdentifyGameType`）。返回 `std::nullopt` 表示常规未匹配（如回退至手动选择），并非操作错误。

---

## 6. 日志规范

本项目采用**任务导向（Task-Oriented）日志系统**。

### 严禁使用

```cpp
Core::Logging::Logger::info(...);
Core::Logging::Logger::warning(...);
Core::Logging::Logger::error(...);
```

严禁引入全局静态日志 API、全局日志器指针或模块级全局日志状态。

### 标准流向

Application/Workflow 创建或接收任务日志上下文并向下传递：

```text
Application
  ↓
TaskLoggingContext
  ↓
Workflow / Domain
```

### 日志级别与任务生命周期契约

* **`error()` / `reportFault()`**：**当前任务发生不可恢复的业务失败**。触发后任务将被 `AsyncTaskRunner` 自动置为 `TaskState::Failed`。只有任务真正失败时才可调用；若故障已被降级/重试处理，严禁调用 `error()`。
* **`warning()`**：**可恢复问题、降级处理或跳过**。不影响任务成功状态。
* **`info()`**：**面向用户的宏观阶段里程碑**（如任务开始、完成、发现关键资产）。切勿在 info 级别输出冗长技术细节。
* **`debug()`**：**技术诊断与内部步骤追踪**（搜索路径、解析器细节、CLI 参数等）。

### UI 日志

ViewModel 可实现 `ILogSink` 或订阅日志事件，但适配层必须将更新调度至 UI 线程。Core 层日志接收器可将日志落地为文件，但 Core 绝对不得感知 UI 的存在。

---

## 7. 用户交互与弹窗确认规范

下层严禁直接调用模态对话框。

严禁做法：

```cpp
QMessageBox::question(...);
QQmlApplicationEngine ...;
从 Domain/Workflow 中调用 QML 弹窗;
```

标准调用流：

```text
Workflow/Domain 需要确认
        ↓
抽象确认接口 (Prompt Interface)
        ↓
Application 实现策略/桥接
        ↓
UI/QML 弹出实际对话框
        ↓
结果异步返回至 Application/Worker 上下文
```

---

## 8. 外部工具调用规范

严禁在业务代码中直接创建外部进程。

严禁做法：

```cpp
QProcess process;
process.start(...);

std::system(...);
popen(...);
WinExec(...);
```

所有外部 CLI 工具（`bspsrc`, `source1import`, `resourcecompiler`, `vpkeditcli`, `vtfcmd` 等）必须统一封装于：

```text
Domain::Tool
    ↓
Core::Process::ProcessRunner
```

### 8.1 进程机械结果 (`ProcessResult`) 与业务错误模型契约

`Core::Process::ProcessResult` 描述的是外部进程执行的**底层机械结果**（包括 `ProcessStatus`、退出码、`stdOut`、`stdErr` 与系统级报错），并非抽象的领域业务结果。

因此：
1. **保留机械结果结构体**：`ProcessResult` 保留为纯粹的机械状态载体，不直接重构为 `Result<T>`。
2. **严禁上层直接消费 `ProcessStatus` 分支**：Workflow 与 Domain 工具包装器严禁随处自行 `switch (procResult.status)` 或各自手写分支判断，避免形成第三套散落的业务错误体系。
3. **统一转译为 `Core::Error` / `Result<T>`**：外部工具包装层或消费方必须通过统一转译规则将 `ProcessResult` 转换为 `Core::Error::Error` 或 `Core::Result<T>`。

#### 标准转换映射规则

| 机械状态 (`ProcessStatus`) | 标准错误码 (`ErrorCode`) | 说明 |
| :--- | :--- | :--- |
| `ProcessStatus::Success` | `ErrorCode::Success` | 进程正常退出且退出码为 0 |
| `ProcessStatus::FailedToStart` | `ErrorCode::ProcessFailed` (或 `ProcessNotFound`) | 可执行文件缺失、权限不足或启动失败 |
| `ProcessStatus::TimedOut` | `ErrorCode::ProcessTimeout` | 进程执行超时被主动终止 |
| `ProcessStatus::Crashed` | `ErrorCode::ProcessFailed` (底层映射为 `ProcessCrashed`) | 进程异常崩溃或收到致命信号 |
| `ProcessStatus::NonZeroExit` | `ErrorCode::ProcessFailed` | 进程非零异常退出 |

#### 诊断字段组装规范

* **`Error.message()`**：机械错误说明（如 `"Process execution failed with exit code 1"` 或 `procResult.errorMessage`）。
* **`Error.details()`**：技术诊断输出（优先包含 `stdErr.trimmed()`，为空时使用 `stdOut.trimmed()`）。
* **`Result.message()`**：当前上层宏观操作摘要（如 `"BSPSRC 反编译地图失败"`）。

#### 规范调用示例

```cpp
// 外部工具封装层或 Workflow 执行外部进程
Core::Process::ProcessResult procResult = processRunner.run(cmd, args, options);

if (!procResult.isSuccess()) {
    // 转换为标准 Core::Error 并附加宏观操作总结
    return Result<void>::failure(
        procResult.toError(),
        QStringLiteral("BSPSRC 地图反编译执行失败") // Result.message: 操作总结
    );
}
```

---

## 9. 文件系统与 I/O 规范

* 优先使用 `Core::Path::FilesystemPath` 与 `Core::FileSystem`。
* 严禁在 Domain/Application/UI 中重复实现路径标准化、原子写入、文件锁或基础文件工具函数。
* 需原子写入时使用 `Core::FileSystem::AtomicFile`。
* 避免重复读取/解析同一文件，优先在内存 AST 中完成转换。
* 对 Valve KeyValues/VDF/VMF 格式文件，必须使用 `Core::KeyValues`，严禁使用临时正则表达式解析。
* Domain 决定 Valve 专有相对路径/搜索规则；Core 只处理通用宿主文件系统路径。

---

## 10. 目标目录结构

```text
src/
├── Core/
│   ├── Error/
│   ├── FileSystem/
│   ├── KeyValues/
│   ├── Logging/
│   ├── Path/
│   ├── Process/
│   ├── Temp/
│   └── CMakeLists.txt
│
├── Domain/
│   ├── Asset/
│   ├── Audio/
│   ├── Bsp/
│   ├── Game/
│   ├── Material/
│   ├── Package/
│   ├── Tool/
│   ├── Vmf/
│   └── CMakeLists.txt
│
├── Workflow/
│   ├── Common/
│   ├── Map/
│   ├── Model/
│   ├── Particle/
│   └── CMakeLists.txt
│
├── Application/
│   ├── Config/
│   ├── Environment/
│   ├── Task/
│   ├── Update/
│   └── CMakeLists.txt
│
├── UI/
│   ├── Controllers/
│   ├── ViewModels/
│   └── CMakeLists.txt
│
└── qml/
    └── cs2importer/
        ├── CMakeLists.txt
        └── Main.qml
```

`src/Legacy/` 仅用于过渡。新代码严禁依赖 Legacy。

---

## 11. 模块迁移对照表

| 既有 / 遗留组件                                | 目标分层                                        | 目标路径                             | 核心职责                                                  |
| :--------------------------------------------- | :---------------------------------------------- | :----------------------------------- | :-------------------------------------------------------- |
| `Miscellaneous::RunCommandSync`, `PROGRAM_*`   | `Domain::Tool`                                  | `src/Domain/Tool/`                   | 基于 `Core::Process` 的外部工具强类型封装。               |
| `VmfBspProcess`                                | `Domain::Vmf` / `Domain::Bsp`                   | `src/Domain/Vmf/`, `src/Domain/Bsp/` | VMF 处理与 BSP 反编译行为。                               |
| `MaterialFix`                                  | `Domain::Material`                              | `src/Domain/Material/`               | VMT/VMAT 材质转换与修正。                                 |
| `SoundscapeImport`                             | `Domain::Audio`                                 | `src/Domain/Audio/`                  | Soundscape 提取与 VMF 音效关联。                          |
| `FileExtractFromVPK`                           | `Domain::Package`                               | `src/Domain/Package/`                | 类型安全 VPK/包提取。                                     |
| `Miscellaneous::ParseGameInfo`, `SearchTarget` | `Domain::Game`                                  | `src/Domain/Game/`                   | GameInfo 解析、校验与搜索路径解析。                       |
| `ModelImporter`                                | `Workflow::Model`                               | `src/Workflow/Model/`                | `.mdl → .vmdl` 导入流水线。                               |
| `ParticleImporter`                             | `Workflow::Particle`                            | `src/Workflow/Particle/`             | `.pcf → .vpcf` 导入流水线。                               |
| `MapImporter`                                  | `Workflow::Map`                                 | `src/Workflow/Map/`                  | BSP → VMF → 编译/资产提取流水线。                         |
| `Ui::AutoDetectPaths`, `IsValid*`              | `Application::Environment` + `Domain::Game`     | 对应目录                             | Application 编排 + Domain 校验。                          |
| `vpk.signatures` 锁定                          | `Application::Environment` + `Core::FileSystem` | 对应目录                             | Application 策略 + 通用文件租约（File Lease）。           |
| `Ui::CheckForUpdate`                           | `Application::Update`                           | `src/Application/Update/`            | 自动更新检测。                                            |
| `Ui::LoadFromCfg`, `SaveToCfg`                 | `Application::Config`                           | `src/Application/Config/`            | 配置持久化。                                              |
| `Ui::Start`, 工作线程, `CancelAll`             | `Application::Task`                             | `src/Application/Task/`              | WorkflowRunner / 任务生命周期管理。                       |
| `Ui.h/.cpp` Q_PROPERTY/slots                   | `UI`                                            | `src/UI/`                            | 极薄的表现层适配器。                                      |

---

## 12. CMake 依赖强制规范

CMake 构建配置必须如实映射架构依赖图。

### 模块依赖拓扑

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
cs2importer 主执行程序 / QML 集成
```

### 规则

* `cs2importer_core` 严禁链接 Domain / Application / UI。
* `cs2importer_domain` 仅链接 Core。
* `cs2importer_workflow` 链接 Domain + Core。
* `cs2importer_application` 链接 Workflow + Domain + Core。
* `cs2importer_ui` 链接 Application 及 Qt UI 模块。**严禁为了方便而在 UI 中直接链接 Domain/Core。**
* 优先使用 `PRIVATE` 链接，仅在属于公共 API 时使用 `PUBLIC`。
* 避免无节制暴露全局 `${CMAKE_SOURCE_DIR}` 头文件路径。

### CMake 架构红线

若补丁试图在 `src/UI/CMakeLists.txt` 中添加 `cs2importer_domain` 或 `cs2importer_core` 以便 ViewModel 直接调用，**必须立即停止并重新设计架构边界**。

---

## 13. Application 服务设计

Application 服务默认应当是**基于对象实例（Instance-based）**的，而非巨大的 `static` 静态函数集合。

推荐范式：

```cpp
class GameEnvironmentService {
public:
    ValidationResult validate(const ValidateGameRequest& request);

    void detectAsync(
        const DetectGamesRequest& request,
        std::function<void(DetectionResult)> callback);
};
```

避免将 API 设计成庞大的静态工具类集合。当一个服务承担了过多不相关职责时，应及时拆分。

---

## 14. Domain API 设计

Domain API 必须保持确定性（Deterministic）和易测性（Testable）。

推荐范式：

```text
输入数据
    → 领域操作
    → 返回值 / 结果 / 错误
```

Domain API **严禁**：
* 访问 UI 状态；
* 访问应用全局状态；
* 弹出对话框；
* 仅为掩盖设计缺陷而自行启动 Worker 线程；
* 隐式修改无关的全局配置；
* 返回面向展示的特定结构。

跨入 Domain 层后，统一使用强领域类型（如 `GameType`、`AssetPath`、`SearchTarget` 等）。

---

## 15. 错误处理规范

* 优先使用结构化 `Core::Error` 与类型安全结果对象，避免字符串型控制流。
* 保留足够的诊断上下文，以便向 Application 与 UI 层清晰反馈失败原因。
* Domain 错误应准确描述领域/系统故障事实，而非 UI 文案。
* 严禁在 Application 内部 helper 或业务逻辑中无理由使用 `catch (...) {}` 吞没异常并伪造空值、空容器或成功状态。
* 严禁仅因操作跳过而返回 `true`（除非契约明确将跳过定义为成功/空操作）。

---

## 16. 日志 API 参考

任务导向日志标准范式：

```cpp
auto task =
    Core::Logging::LogManager::instance()
        .createTask(QStringLiteral("导入模型"));

task->start();
task->info(QStringLiteral("开始处理"));
task->updateProgress(0.5, QStringLiteral("转换中"));
task->complete(QStringLiteral("处理完成"));
```

迁移后的 Workflow/Domain 代码应由调用方显式注入 task 上下文。UI 日志接收器必须线程安全，并在跨线程时通过排队机制更新 QObject 状态。

---

## 17. Core API 参考

`src/Core/CMakeLists.txt` 将 `cs2importer_core` 编译为静态库，包含根路径为 `src/`。

* **`Core::Path`**：`FilesystemPath` 提供标准化的宿主文件系统路径操作；`PathUtils` 提供通用路径规范化、扩展名提取与安全文件名过滤。
* **`Core::KeyValues`**：通用 Valve KeyValues/VDF AST 解析与序列化器，支持无引号 Token、嵌套节点、同名兄弟节点、保序输出及原子写入。
* **`Core::FileSystem`**：提供通用文件系统辅助类、`AtomicFile`、`DirectorySnapshot` 以及 RAII 移动语义的 `FileLease` 文件租约。
* **`Core::Process`**：`ProcessRunner` 提供结构化外部进程调用与超时控制（`ProcessOptions`, `ProcessResult`）。
* **`Core::Temp`**：`TempFile` 与 `TempDirectory` 提供 RAII 临时资源生命周期管理。
* **`Core::Error`**：`ErrorCode`, `Error`, `Exception` 通用错误原语。`Result<T>` 原生集成 `Core::Error::Error`。

---

## 18. Domain API 参考

`src/Domain/CMakeLists.txt` 构建 `cs2importer_domain`，依赖 Core。

* **`Domain::Asset`**：`AssetPath`（资产相对路径）、`AssetTypeDetector`（资产类型判别）。
* **`Domain::Game`**：`GameType`, `EngineType`, `GameDefinition`, `GameRegistry`, `GameInfo`, `GameInfoParser`, `SearchTarget`, `SearchPathResolver`, `GameValidator`。封装 Source/Valve 核心业务语义，彻底与 Application/UI 解耦。新增支持的游戏应通过 `GameDefinition` / `GameRegistry` 元数据驱动。

---

## 19. Application API 参考

核心环境服务：
* `SteamService`：Steam 安装目录/库探测与 App Manifest 读取；
* `GameInstallation`：探测到的游戏安装应用层数据表示；
* `GameDetectService`：游戏探测与校验编排；
* `VpkSignatureLeaseService`：CS2 `vpk.signatures` 排他性租约策略。

面向 UI 的结果应封装为 DTO（如 `ValidationResult`），避免向 QML 暴露底层 AST 或内部设施指针。

---

## 20. 重构演进路线图

重构按阶段逐步推进，**严禁为了让临时代码通过编译而跨阶段混杂实现**。

1. **Stage 1 — Core 基础设施解耦提取**（已完成）
2. **Stage 2 — Domain 领域基础迁移**（游戏模型/解析器/注册表/校验器、`Domain::Tool`、`Domain::Package`）
3. **Stage 3 — 导入器与领域逻辑迁移**（ModelImporter → `Workflow::Model`、ParticleImporter → `Workflow::Particle`、MaterialFix → `Domain::Material`、VmfBspProcess → `Domain::Vmf` + `Domain::Bsp`）
4. **Stage 4 — Application 应用编排重构**（WorkflowRunner、ConfigService、UpdateService、任务/取消/日志统一路由）
5. **Stage 5 — MapImporter 重构与 UI 瘦身**（MapImporter → `Workflow::Map`、UI 彻底收敛为纯展示与 Application 调用）

---

## 21. 架构变更必须执行的准则

在修改代码前，Agent 必须明确回答以下问题：

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

## 22. 强制架构审查清单 (Architecture Review Checklist)

任何重构代码提交前必须对照本清单自查：

### 职责归属
* [ ] 修改的每个函数均归属于正确的层级。
* [ ] UI 类中无 Domain 业务编排。
* [ ] Application 类中无本应属于更底层的具体 Domain 转换逻辑。
* [ ] Domain / Core 类绝不感知 Application / UI。

### 依赖关系图
* [ ] 未引入任何向上逆向 include。
* [ ] CMake 中未引入向上的反向依赖。
* [ ] UI 模块未为访问底层细节而链接 Domain / Core。
* [ ] Workflow 不依赖 Application / UI。

### 运行时表现
* [ ] 阻塞性 I/O 绝不在 UI 线程执行。
* [ ] Worker 回调具备生命周期安全防护，并通过排队连接安全回到 UI 线程。
* [ ] 取消操作显式、协作且确定。

### 集成边界
* [ ] `Domain::Tool` + `Core::Process` 之外无直接 `QProcess` / Shell 调用。
* [ ] Application / UI 弹窗桥接之外无直接模态对话框调用。
* [ ] 未引入全局静态日志器。
* [ ] 未引入新的全局可变状态。

### API 规范
* [ ] UI 接收 Application 契约对象，而非 Domain AST / 底层设施对象。
* [ ] Domain API 使用强领域类型。
* [ ] 错误处理结构化并保留诊断上下文。
* [ ] 未重复编写已有 Core 基础设施的功能。

### 测试覆盖
* [ ] 新增的 Domain / Core 逻辑具备隔离的单元测试覆盖。
* [ ] 涉及的 Application 服务有编排测试。
* [ ] UI 测试关注状态与信号，而非重复测试 Domain 内部细节。

---

## 23. 架构红线异味（必须重构）

```text
UI/ViewModel → Domain::GameValidator
UI/ViewModel → Core::FileSystem
UI/ViewModel → Core::KeyValues
UI/ViewModel → Core::Process
UI/ViewModel → Steam 注册表 / 库扫描

Application → 直接操作 QML 控件
Domain → Application
Domain → UI
Domain → QMessageBox / QWidget / QQml...
Workflow → UI
Workflow → Application

任何业务文件 → QProcess / system() / Shell
任何业务文件 → 全局 Logger::info/error/warning

承担众多杂项职责的庞大静态 Application 服务
无明确移除计划的临时跨层 include
```

---

## 24. C++ 编码规范

* 采用 C++17 标准、适度使用 Qt 类型、遵循 RAII 原则、明确所有权与 `const` 正确性。
* 类名与枚举采用 `PascalCase`；函数、方法、局部变量与成员变量采用 `camelCase`。
* 保持头文件轻量且自包含。
* 明确 include 所需的标准库头文件，严禁依赖传递性间接包含。
* 涉及策略、状态、日志或异步操作的服务优先采用依赖注入。

---

## 25. CMake 规范

* 要求 CMake 3.28+ 与 Qt 6.8+。
* 适时使用 `qt_standard_project_setup()`。
* 可执行程序使用 `qt_add_executable()`。
* 模块库按需使用 `qt_add_library()`。
* QML 模块使用 `qt_add_qml_module()`。
* 仅非 QML 资源使用 `qt_add_resources()`。
* 显式声明 `PRIVATE`、`PUBLIC` 或 `INTERFACE` 目标可见性。
* 严禁使用 Qt 5 CMake API、`Qt5::` 目标或 qmake 语法。

---

## 26. 构建与测试指令

### 主程序构建

```bash
cmake -B build -S .
cmake --build build
```

或使用预设（Preset）：

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### 运行测试

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --test-dir build/local-debug --output-on-failure
```

---

## 27. 测试架构与依赖规则

测试依赖规则与生产代码一致：
* Domain 测试可依赖 Domain + Core。
* Workflow 测试可依赖 Workflow + Domain + Core。
* Application 测试可依赖 Application 及下层模块。
* UI 测试可依赖 UI + Application 契约与服务。
* 测试代码严禁为访问内部实现细节而破坏分层原则。

---

## 28. 技能规范自动加载参照

在进行相关修改前，需查阅对应 Skill：

| 任务类型            | Skill 路径                         |
| ------------------- | ---------------------------------- |
| C++ 代码实现        | `skills/qt-cmake-project/SKILL.md` |
| CMake / 构建变更    | `skills/qt-cmake-project/SKILL.md` |
| QML 界面实现        | `skills/qt-qml/SKILL.md`           |
| C++ 代码审查        | `skills/qt-cpp-review/SKILL.md`    |
| QML 代码审查        | `skills/qt-qml-review/SKILL.md`    |
| UI / UX 设计决策    | `skills/qt-ui-design/SKILL.md`     |

---

## 29. 绝对禁止事项

* 严禁 Core 依赖 Domain / Application / Workflow / UI / QML。
* 严禁 Domain 依赖 Application / Workflow / UI / QML。
* 严禁 Workflow 依赖 Application / UI。
* 严禁 UI 为执行业务直接调用 Domain / Core。
* 严禁在 `cs2importer_ui` 中添加对 Domain / Core 的直接链接。
* 严禁将游戏检测、Steam 扫描、文件校验、导入逻辑或外部工具调用写入 ViewModel / Controller。
* 严禁为配置项、服务、取消标志或日志器添加全局静态变量。
* 严禁在 UI / Application 业务代码中直接调用 `QProcess`、`system()` 或 Shell 命令。
* 严禁从 Domain / Workflow 中弹出对话框。
* 严禁在 Application 内部 helper 中使用 `catch (...)` 吞没异常并返回空值/空容器/`false`/`nullptr`。
* 严禁将 MapImporter 迁移与其他导入器的迁移混在同一阶段。
* 严禁在专项迁移中顺带进行无关重构。
* 严禁通过 `static` 函数、便捷辅助类、友元声明或 CMake 传递链接掩盖分层违规。
* 严禁将“通过编译”等同于“架构设计正确”。

---

## 30. 终极准则

当存在疑问时，务必选择能让依赖关系图**更清晰、更单向、更易测、且更难以被意外破坏**的设计方案。

正确的思考出发点不是：

> “这段代码写在哪里能让当前的构建通过？”

而是：

> “哪个分层拥有该职责？跨越该边界的公开契约是什么？如何在不让任何层感知其上层的前提下优雅实现它？”
