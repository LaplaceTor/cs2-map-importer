---
name: cs2-async-error-handling
description: >-
  Use this skill when implementing asynchronous tasks, error handling, process execution, logging, or state transitions in cs2-map-importer. Covers the three task planes (workflow/tool tasks, system tasks, Result<T> outcomes), the Application logging facade, Result<T> semantics, tripartite diagnostic contracts, exception boundaries, and state conflict arbitration.
---

# CS2 Map Importer — 异步任务与错误诊断规范指南

本指南汇集了项目在异步任务调度、业务执行结果表示、分层诊断日志与错误仲裁方面的核心模式、规则及完整决策矩阵。

---

## 1. 三平面任务体系：任务执行生命周期、系统任务与业务执行结果

为保证异步任务与工作流操作的概念严密性，架构定义了三个正交平面：

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. 任务执行生命周期平面 (Task Execution Lifecycle Plane: TaskState)        │
│    由 LogManager / TaskLoggingContext 管理 (工作流/阶段/工具任务)          │
│    状态流转: Pending → Running → Completed | Failed | Cancelled | Skipped    │
│    在 UI 日志模型中跟踪展示 (LogViewModel / LogTaskModel)                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ 2. 系统任务平面 (System Task Plane: SystemTaskLog)                          │
│    由 AsyncTaskRunner::runSystemTask 创建, taskId 恒为 0                    │
│    无 LogManager 任务 / 无 TaskState / 不进 UI 任务树                       │
│    日志并入 application_<timestamp>.log ("[TaskName]" 前缀)                 │
├─────────────────────────────────────────────────────────────────────────────┤
│ 3. 业务执行结果平面 (Business Outcome Plane: Result<T>)                     │
│    Workflow 与 Application API 的标准单层返回契约                           │
│    结果状态: Success | Failure | Cancelled | Skipped + 业务负载 T 与说明文本 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 `AsyncTaskRunner` 标准 API 体系

`AsyncTaskRunner` 是连接三个平面的核心桥梁：

* `AsyncTaskRunner::runTask<T>(taskName, context, worker, callback)`: 用于产出业务负载 `T` 的异步任务。
* `AsyncTaskRunner::runTask<void>(taskName, context, worker, callback)`: 用于无返回值的异步任务，保持完整的 `Result<void>` 语义。
* `AsyncTaskRunner::runChildTask<T>(parentTaskId, taskName, context, worker, callback)`: 用于层次化子任务。
* `AsyncTaskRunner::runChildTask<void>(parentTaskId, taskName, context, worker, callback)`: 用于无返回值子任务。
* `AsyncTaskRunner::runBackground(taskName, worker)`: 委派至 `runTask<void>` 的后台便捷封装，Worker 必须返回 `Core::Result<void>`，复用同一套生命周期与异常仲裁逻辑。
* `AsyncTaskRunner::runWorkflowTask<T>(workflowName, assetBaseName, context, worker, callback)`: 顶层导入工作流入口——自动创建工作流日志目录（`logs/<workflowName>_<timestamp>/`）并注册 Workflow 根任务，再走 `runTask` 全套生命周期与仲裁。
* `AsyncTaskRunner::runSystemTask<T>(taskName, context, worker, callback)`: **系统任务**入口——Worker 签名为 `(const Application::Async::SystemTaskLog&, [CancellationToken])`，必须返回 `Result<T>`。无 LogManager 任务、无 `TaskState`、不进 UI 任务树（taskId 恒为 0）；runner 在应用日志中写入一行生命周期记录（`started` / `finished` / `cancelled` / `skipped` / `failed`）；`TaskHandle::cancel()` 仅触发取消令牌，不触碰 LogManager。

规则：
1. `T` 为**业务负载类型**（如 `GameInstallationInfo`、`DetectionResult`、`void`），严禁嵌套为 `Result<Result<T>>`。
2. Worker 返回单层 `Result<T>`。
3. `AsyncTaskRunner` 结合业务结果、日志报错与捕获的异常，驱动 `LogManager` 中的 `TaskState` 状态转移。
4. 回调函数接收 `const Result<T>&`，并线程安全地投递至调用方所在线程。
5. **平面归属规则**：面向用户的导入工作流（粒子导入、音景转换等）一律走 `runWorkflowTask` / `runTask` 平面（进任务树、有独立日志）；环境检测、安装校验、插件列举等非导入后台任务必须走 `runSystemTask`，严禁占用可见任务树。

### 1.2 层级化任务日志树与外部工具任务 (Workflow Task vs Tool Task)

任务执行生命周期支持严格的层次化树状结构：

```text
Workflow Task (Root: createWorkflowTask) -> logs/<workflow>_<timestamp>/workflow.log
 └── Stage Child Task (Stage: createChildTask)
      ├── [EXEC] commandLine (携带 toolTaskId)
      └── Tool Task (createToolTask, UI平铺树中隐藏) -> <父任务日志目录>/<asset>_<tool>_<time>.log
```

* **Workflow 根任务 (`LogManager::createWorkflowTask`)**：
  * 面向高层宏观导入流程（如地图、粒子导入工作流）。
  * 自动在 `logs/` 目录下生成独立时间戳目录 `logs/<workflowName>_<yyyyMMdd_HHmmss_zzz>/`，并将主日志输出至该目录下的 `workflow.log`。
* **阶段子任务 (`createChildTask`)**：
  * 表示工作流内部阶段里程碑（如解包、材质转码、几何体提取）。
  * 继承父任务的工作流目录上下文，UI 端支持按深度平铺投影与同级互斥折叠。
* **外部工具任务 (`createToolTask`)**：
  * 专门承接外部 CLI 进程（`resourcecompiler`, `source1import`, `bspsrc`）。
  * **主界面噪音屏蔽**：标记 `isToolTask() == true`，在 UI 主任务树（`LogViewModel`）中静默隐藏，避免大量编译日志刷屏。
  * **命令透传与关联**：自动在父阶段任务注入 `[EXEC] <commandLine>` 日志条目，并绑定 `toolTaskId`。
  * **独立落盘与专项视窗**：日志单独保存为 `<asset>_<tool>_<time>.log`，**写入父任务的日志目录**（父为阶段任务时即 `logs/<workflow>_<ts>/<stage>/` 子目录；父为工作流根时直接落在工作流目录下）；同毫秒命名冲突自动追加 `_2`、`_3` 序号。用户点击父任务中的命令条目时，UI 通过独立的 `ToolLogWindow` 调取该工具的完整输出流。

### 1.3 日志投递门面 (Application::Logging)

UI 层消费日志的**唯一通道**是 Application 层门面 `Application::Logging::TaskLogService`（完整 API 见 `skills/cs2-api-reference/SKILL.md`）：

* `TaskLogService` 以私有 `SinkBridge`（`Core::Logging::ILogSink`）桥接 `LogManager`，将密封 `LogBlock` 转换为 UI DTO（`TaskLogMessage` / `TaskInfo`，定义于 `TaskLogDTOs.h`），经队列信号 `logBatchReceived(subscriptionId, taskId, taskName, messages)` 投递；
* **订阅制陈旧批次抑制**：`subscribe()` / `unsubscribe()`；旧订阅 id 的在途批次由接收方按 id 丢弃（取代旧的 `viewGeneration()` 代数模型）；无订阅者时跳过 DTO 转换；
* **红线**：`src/UI/` 严禁 include `Core/Logging/*`。`LogViewModel` 通过 `attachToLogService()` / `detachFromLogService()` 挂接门面（旧的 `registerWithLogManager()` 直连模式已删除）。

---

## 2. Result 语义契约与值访问

### 2.1 契约属性
* **`status()` / `isSuccess()` / `isFailure()` / `isCancelled()` / `isSkipped()`**：业务结果分支判定的权威来源。严禁仅凭 `errorCode()` 判定业务成败。
* **`error()`**：机器可解析的结构化错误对象（`Core::Error::Error`），包含：
  * `error().code()`：用于路由分支的标准 `ErrorCode` 枚举；
  * `error().message()`：领域/系统层面的具体失败原因（如 `"gameinfo.gi not found"`）；
  * `error().details()`：技术诊断数据（如文件路径、stderr 输出、语法行号）。
* **`message()`**：面向用户/UI 的高层操作总结（如 `"CS2 校验失败"`）。未设置时自动回退为 `error().message()`；处于 `Skipped` 状态时携带具体跳过原因。
* **`details()`**：直接代理 `error().details()`。
* **i18n 契约**：`message()` 与 `error().message()` 属用户可见文案，必须在**创建处翻译**——QObject 类用 `tr()`，非 QObject 类用 `QCoreApplication::translate("<类名上下文>", "...")`；`details()` 与外部工具原始输出保持英文技术原文。

### 2.2 值访问规范 (Value Access Contract)
* **`hasValue()` / `isSuccess()` 前置检查**：访问业务负载 `result.value()`、`operator*`、`operator->` 前，**必须**显式检查。在无值状态下调用 `.value()` 会抛出 `std::bad_optional_access`。
* **禁止基于异常的业务控制流**：
  * ❌ 严禁使用 `try { auto v = res.value(); } catch (...) {}` 进行控制流分支。
  * ✅ 规范做法：`if (res.isSuccess()) { ... } else { ... }` 或使用 `res.valueOr(...)`。

---

## 3. 三层诊断分层规范 (Tripartite Diagnostic Contract)

`Result::failure(error, message)` 同时保存底层 `Error` 与高层操作摘要 `message`。访问 `result.message()` 时，优先返回显式设置的操作摘要；未设置时自动回退为底层 `error().message()`。

三层字符串职责分工与调用约定如下：

| 诊断层级 | 访问接口 | 归属层级 | 语义职责 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| **操作总结 (Operation Summary)** | `Result::message()` | Workflow / Application | 面向用户/任务的全局高层概括，说明**哪个宏观操作失败或成功**。 | `"地图 'de_dust2' 导入失败"`, `"CS2 环境验证失败"`, `"Steam 探测失败"` |
| **失败原因 (Failure Reason)** | `Error::message()` | Domain / Core | 具体领域或底层系统原因，说明**为何发生失败**。 | `"gameinfo.gi 未找到"`, `"实体解析语法错误"`, `"file missing"` |
| **技术诊断 (Technical Diagnostics)** | `Error::details()` / `Result::details()` | Domain / Core / Process | 供排查问题的底层技术诊断数据（绝对路径、stderr 输出、AST 行号、CLI 参数、退出码等）。**恒为英文原文，不翻译**。 | `"C:/Steam/steamapps/common/CS2/game/csgo/gameinfo.gi"`, 编译器 stderr 输出 |

> **i18n 规则**：`Result::message()` 与 `Error::message()` 属于用户可见平面，必须在消息创建处经 `tr()` / `QCoreApplication::translate("<类名上下文>", ...)` 翻译（非 QObject 类无法使用成员 `tr()`）；日志文件与 UI 因此同语言。`details()`、外部工具原始输出、`debug()` 日志行不翻译。

### 3.1 构造反模式与规范模式

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
      QCoreApplication::translate("GameEnvironmentService", "Steam game detection failed") // Result.message: 操作总结（创建处翻译）
  );
  // 最终：
  // error.message()  = "file missing" (具体失败原因)
  // result.message() = "Steam 游戏探测失败" (宏观操作总结，中文环境下的翻译结果)
  // result.details() = "C:/Steam/..." (技术诊断细节)
  ```
* ✅ **规范模式 2：带技术细节的便捷重载**
  ```cpp
  // 正确：ErrorCode + 明确原因 + 技术路径
  return Result<void>::failure(
      Core::Error::ErrorCode::FileNotFound,
      QCoreApplication::translate("GameInfoParser", "Failed to parse GameInfo file"), // Error.message（创建处翻译）
      gamePath.toQString()                     // Error.details（英文技术原文，不翻译）
  );
  ```

---

## 4. 异常处理与转译边界契约

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
    return Result<T>::failure(Core::Error::ErrorCode::Unknown,
        QCoreApplication::translate("ExecutionGuard", "Unhandled unknown exception"));
}
```

> **边界准则**：异常只能在明确的异常边界被捕获。Application API 边界必须将异常转换为 `Core::Result<T>`；Application 内部 helper 默认不得通过 `catch (...)` 将异常静默转换为空值、空容器、`false` 或 `nullptr`。若异常确实代表合法的 best-effort fallback，必须在注释中说明该 fallback 语义，并确保不会掩盖业务失败。

---

## 5. 跨终态严重性级联与冲突仲裁矩阵

### 5.1 终态严重性级联规则 (Terminal Severity Hierarchy)

单一确定性原则：**Failure 拥有最高优先级，压倒其他所有终态（无论由生命周期还是业务结果产生）。**

优先级顺序：
1. **存在任何 Failure**（`TaskState::Failed`、日志报错或 `Result::failure`）→ 终态为 **`Failed`**，最终结果为 **`Failure`**。
2. **否则，存在任何 Cancelled**（`TaskState::Cancelled` 或 `Result::cancelled`）→ 终态为 **`Cancelled`**，最终结果为 **`Cancelled`**。
3. **否则，存在任何 Skipped**（`TaskState::Skipped` 或 `Result::skipped`）→ 终态为 **`Skipped`**，最终结果为 **`Skipped`**。
4. **否则** → 终态为 **`Completed`**，最终结果为 **`Success`**。

### 5.2 跨终态冲突仲裁矩阵 (State Conflict Arbitration Matrix)

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

* **负载保全规则**：当 `Result<T>` 因契约冲突转换状态时，已有的部分数据负载（`result.value()`）严格予以保留（`Result::failure/cancelled/skipped` 均提供带 `partialValue` 的重载）。
* **原始错误保留规则**：因契约违规转换为 `Failure` 时，若原 `Result` 中已含有非成功错误信息，完整保留其错误码与诊断细节；仅当原结果无有效错误（如原为 `Success`）时才合成 `OperationFailed`。
* **仲裁执行机制**：矩阵由 `AsyncTaskRunner::runTaskInternal` 结合 `LogManager::forceTaskState(taskId, state, message)` 落地（幂等：重复强制同一终态不再重复 flush/通知）。
* **终态不可逆规则**：`TaskLoggingContext::forceTerminalState` 是执行仲裁覆盖入口（可重判终态，如 `Cancelled → Failed`），但已处于失败类终态的任务**永远不可被重判为 Completed**；终态永不回退为非终态。
* **取消级联**：`LogManager::cancelTask` 会级联取消全部子孙任务（BFS 遍历，环防护），保证日志树不出现"父已取消而子仍在运行"。
* **故障屏障（FaultBarrier）**：致命故障路径走 `LogManager::reportFault(taskId, msg)` → `beginFaultDraining()` → `terminateAfterFault()`；任务上下文侧对应 `TaskLoggingContext::reportFault`（返回 `LogSubmissionResult`）。

---

## 6. 进程机械结果 (`ProcessResult`) 与业务错误模型转换

`Core::Process::ProcessResult` 描述的是外部进程执行的底层机械状态（退出码、`stdOut`、`stdErr`、系统报错）。外部工具包装层（`Domain::Tool`）统一将其实例化并转译为业务错误。

### 6.1 标准转换映射规则

| 机械状态 (`ProcessStatus`) | 标准错误码 (`ErrorCode`) | 说明 |
| :--- | :--- | :--- |
| `ProcessStatus::Success` | `ErrorCode::Success` | 进程正常退出且退出码为 0 |
| `ProcessStatus::FailedToStart` | `ErrorCode::ProcessFailed` (或 `ProcessNotFound`) | 可执行文件缺失、权限不足或启动失败 |
| `ProcessStatus::TimedOut` | `ErrorCode::ProcessTimeout` | 进程执行超时被主动终止 |
| `ProcessStatus::Crashed` | `ErrorCode::ProcessCrashed` | 进程异常崩溃或收到致命信号（`toErrorCode()` 直接返回） |
| `ProcessStatus::NonZeroExit` | `ErrorCode::ProcessFailed` | 进程非零异常退出 |
| `ProcessStatus::Cancelled` | `ErrorCode::Cancelled` | 进程因取消令牌触发而被终止 |

### 6.2 诊断字段组装与调用示例

* **`Error.message()`**：机械错误说明（如 `"Process execution failed with exit code 1"`）。
* **`Error.details()`**：技术诊断输出（优先包含 `stdErr.trimmed()`，为空时使用 `stdOut.trimmed()`）。
* **`Result.message()`**：当前上层宏观操作摘要（如 `"BSPSRC 反编译地图失败"`）。

```cpp
Core::Process::ProcessResult procResult = processRunner.run(cmd, args, options);

if (!procResult.isSuccess()) {
    return Result<void>::failure(
        procResult.toError(),
        QCoreApplication::translate("MapImportWorkflow", "BSP decompilation failed") // Result.message: 操作总结（创建处翻译）
    );
}
```

### 6.3 实时流式捕获与 Tool Task 绑定模式

外部编译/导入工具执行周期通常较长，严禁进行静默无反馈的黑盒调用。必须通过 `ProcessOptions` 的回调将 stdout 与 stderr 实时接入任务上下文：

```cpp
// 1. 创建隐藏 Tool 任务（LogManager 是唯一创建入口；父阶段任务自动记录 [EXEC]）
//    规范调用方参见 Domain::Tool::ResourceCompilerTool / Source1ImportTool
auto toolTask = Core::Logging::LogManager::instance().createToolTask(
    taskCtx->taskId(),      // 父阶段任务 id
    fullCommandLine,        // 完整命令行（工具名据此解析，支持带引号的含空格路径）
    assetBaseName);
toolTask->start();

// 2. 配置流式捕获回调与协作式取消令牌
Core::Process::ProcessOptions options;
options.cancellationToken = cancelToken;
options.timeout = 180000; // 3 分钟超时
options.onStdOutLine = [toolTask](const QString& line) {
    toolTask->info(line);
    toolTask->flush(); // 实时落盘与通知
};
options.onStdErrLine = [toolTask](const QString& line) {
    toolTask->warning(line);
    toolTask->flush();
};

// 3. 执行进程
auto procResult = Core::Process::ProcessRunner::run(executable, options);

// 4. 根据结果结束 Tool 任务（终态摘要为用户可见文案，创建处翻译）
if (procResult.isSuccess()) {
    toolTask->complete(QCoreApplication::translate("ToolWorkflow", "Tool execution completed"));
} else if (procResult.isCancelled()) {
    toolTask->cancel(QCoreApplication::translate("ToolWorkflow", "Tool execution cancelled by user"));
} else {
    toolTask->fail(procResult.errorMessage);
}
```

---

## 7. 测试生命周期契约 (Testing Lifecycle Policy)

测试代码必须严格遵守仓库的依赖拓扑与生命周期契约：

1. **Core 层测试（长期常驻维护）**：
   - 纯 Core 单元测试（`test_core_*`）长期驻留于 `tests/`；
   - 必须使用纯 Core 的基础设施（如在测试内实现轻量 `ILogSink` 或使用 `TaskFileSink`）进行断言；
   - **绝对禁止反向依赖**：常驻测试严禁 include 或 link `Application`、`UI` 或 `Domain`。
2. **非 Core 层测试（面向单任务，用完即删）**：
   - 针对 Domain、Workflow、Application、UI 层的验证测试，一律定义为**临时单任务测试（Task-Scoped / Ephemeral Tests）**；
   - 仅在开发相应功能或排查特定缺陷时编写用于自测验证；
   - **任务完成后必须在合入代码库前立即删除**，严禁将包含上层复杂依赖（特别是 UI、QML、ViewModel）的测试长期滞留在 `tests/` 中。

