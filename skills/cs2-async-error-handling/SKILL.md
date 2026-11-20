---
name: cs2-async-error-handling
description: >-
  Three-Plane Task scheduling, Result<T> error contracts, tripartite diagnostics, Single-Point Exception Boundary, state conflict arbitration, and external tool sub-task streaming for cs2-map-importer. Trigger when implementing async tasks, error handling, logging, or CLI process execution.
---

# CS2 Map Importer — 异步任务与错误诊断规范指南

本指南定义项目的 **Three-Plane Task（三平面任务）** 体系、业务结果与三层诊断契约、**Single-Point Exception Boundary（单点异常屏障）**、终态冲突仲裁矩阵及外部工具流式绑定范式。

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

`AsyncTaskRunner` 是连接三个平面的核心桥梁，同时承担异步任务的**顶层异常屏障**：

* `AsyncTaskRunner::runTask<T>(taskName, context, worker, callback)`：产出业务负载 `T`（或 `void`）的异步任务。
* `AsyncTaskRunner::runChildTask<T>(parentTaskId, taskName, context, worker, callback)`：层次化子任务。
* `AsyncTaskRunner::runBackground(taskName, worker)`：委派至 `runTask<void>` 的后台封装，Worker 返回 `Core::Result<void>`。
* `AsyncTaskRunner::runWorkflowTask<T>(workflowName, assetBaseName, context, worker, callback)`：顶层导入工作流入口——创建独立日志目录（`logs/<workflowName>_<timestamp>/`）并注册 Workflow 根任务。
* `AsyncTaskRunner::runSystemTask<T>(taskName, context, worker, callback)`：**系统任务**入口——Worker 签名为 `(const Application::Async::SystemTaskLog&, [CancellationToken])`，返回 `Result<T>`；无 LogManager 任务、不进 UI 任务树（`taskId == 0`），`TaskHandle::cancel()` 仅触发取消令牌。

规则：
1. `T` 为**业务负载类型**（如 `GameInstallationInfo`、`DetectionResult`、`void`），严禁嵌套 `Result<Result<T>>`。
2. **平面归属规则**：用户可见导入/转换流程（粒子导入、音景转换等）走 `runWorkflowTask` / `runTask`；环境检测、安装校验、插件列举、VPK 索引预热等非导入后台任务走 `runSystemTask`。
3. **致命异常终态回退 (`fallbackTerminalStateOnFatalException`)**：Worker 逃逸未捕获异常且任务未达终态时，强制流转至 `TaskState::Failed` 并刷新进度至 1.0；对已达终态严格幂等保留。
4. **回调异常隔离 (`invokeCallbackSafely`)**：向调用方线程派发回调时捕获并记录至应用日志，防止 UI 回调异常破坏 Worker 线程池。
5. **异步服务所有权与状态回滚**：继承 `std::enable_shared_from_this` 的服务（如 `ParticleImportService`、`VpkIndexService`）必须使用 PassKey + 静态 `create()` 工厂约束 `std::shared_ptr`；异步入口变更状态前先校验 `weak_from_this().lock()`；活动计数与状态变更使用 RAII 守卫（如 `ImportScopeGuard`）确保派发失败时回滚。
6. **主线程信号亲和性 (`dispatch*`)**：后台任务产出结果后，面向 UI 的 Qt 信号必须通过 `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` 在主线程发射。
7. **无状态门面与 `TaskHandle`**：无成员状态的转换门面（如 `SoundscapeConvertService`）声明为静态纯函数并返回有效 `Async::TaskHandle`。

### 1.2 层级化任务日志树与外部工具子任务 (Workflow Task vs Tool Sub-Task)

```text
Workflow Task (Root: createWorkflowTask) -> logs/<workflow>_<timestamp>/workflow.log
 └── Stage Child Task (Stage: createChildTask)
      ├── [EXEC] commandLine (携带 toolTaskId)
      └── Tool Sub-Task (taskCtx->createToolSubTask, UI主树隐藏) -> <父任务目录>/<asset>_<tool>_<time>.log
```

* **Workflow 根任务 (`LogManager::createWorkflowTask`)**：面向顶层导入流程，生成 `logs/<workflowName>_<yyyyMMdd_HHmmss_zzz>/workflow.log`。
* **阶段子任务 (`createChildTask`)**：工作流内部阶段里程碑，继承父工作流目录。
* **外部工具子任务 (`TaskLoggingContext::createToolSubTask`)**：
  * `Domain::Tool`（`ResourceCompilerTool`、`Source1ImportTool`）**通过传入的 `taskCtx->createToolSubTask(commandLine, assetBaseName)` 派生**（内部委托 `LogManager::createToolTask` 并启动），并经 `finishToolSubTask` / `failToolSubTask` / `cancelToolSubTask` 结束——`Domain::Tool` 不直接依赖 `LogManager::instance()`。
  * **UI 噪音屏蔽**：`isToolTask() == true`，在 `LogViewModel` 主任务树中隐藏；父任务自动记录携带 `toolTaskId` 的 `[EXEC]` 条目，用户按需在 `ToolLogWindow` 查看。
  * **独立落盘与原始流直通**：写入父任务目录下的 `<asset>_<tool>_<time>.log`（同毫秒冲突追加 `_2`）；经 `logExternalToolOutput` 保持原始行输出，工具报错与父阶段任务隔离。

### 1.3 日志投递门面 (`Application::Logging::TaskLogService`)

UI 消费日志的**唯一通道**是 `Application::Logging::TaskLogService`（完整 API 见 [`skills/cs2-api-reference/SKILL.md`](../cs2-api-reference/SKILL.md)）：
* 私有 `SinkBridge` 实现双阶段 `detach()` 协议（原子标志 `std::atomic<bool> m_detached` 快速跳过 + 互斥锁指针置空），防止门面析构与日志器锁外分发并发 UAF。
* 订阅制投递 `logBatchReceived(subscriptionId, taskId, taskName, messages)`，按 `subscriptionId` 抑制陈旧批次；`src/UI/` 严禁 include `Core/Logging/*`。

---

## 2. Result 语义契约与值访问

### 2.1 契约属性
* **`status()` / `isSuccess()` / `isFailure()` / `isCancelled()` / `isSkipped()`**：分支判定的唯一权威来源，不以 `errorCode()` 替代状态判定。
* **`error()`**：结构化 `Core::Error::Error`，含 `code()`、`message()`、`details()`；领域枚举使用带域匹配 `error.is(domainName, code)`。
* **`message()`**：面向用户的高层操作总结（未设置时回退 `error().message()`）；`details()` 代理 `error().details()`。
* **i18n 契约**：`message()` 与 `error().message()` 必须在**创建处翻译**（QObject 用 `tr()`，非 QObject 用 `QCoreApplication::translate("<类名>", "...")`）；`details()` 与工具原始输出保持英文。

### 2.2 值访问规范
* 访问 `result.value()`、`operator*`、`operator->` 前必须先检查 `hasValue()` / `isSuccess()`（或使用 `valueOr(...)`）；不以 `try/catch` 驱动常规业务分支。

---

## 3. 三层诊断分层规范 (Tripartite Diagnostic Contract)

| 诊断层级 | 访问接口 | 归属层级 | 语义职责 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| **操作总结 (Operation Summary)** | `Result::message()` | Workflow / Application | 说明**哪个宏观操作失败或成功**（创建处翻译）。 | `"Map 'de_dust2' import failed"` / `"地图 'de_dust2' 导入失败"` |
| **失败原因 (Failure Reason)** | `Error::message()` | Domain / Core | 说明**为何发生失败**（创建处翻译）。 | `"gameinfo.gi not found"` / `"未找到 gameinfo.gi"` |
| **技术诊断 (Technical Diagnostics)** | `Error::details()` / `Result::details()` | Domain / Core / Process | 绝对路径、stderr、行号、CLI 参数、退出码等，**恒为英文原文**。 | `"C:/Steam/.../csgo/gameinfo.gi"`, 编译器 stderr |

### 3.1 构造规范模式与反模式

* ❌ 反模式：将操作总结塞进 `Error.message`、把绝对路径拼进 `Error.message`，或三层字符串互相重复。
* ✅ 规范模式 1（多层结构化失败封装）：
  ```cpp
  auto err = Domain::Game::GameErrors::gameInfoNotFound(
      QCoreApplication::translate("GameInfoParser", "gameinfo.gi not found"),
      gamePath.toQString());
  return Result<void>::failure(
      err,
      QCoreApplication::translate("GameEnvironmentService", "Steam game detection failed"));
  ```
* ✅ 规范模式 2（带技术细节的便捷重载）：
  ```cpp
  return Result<void>::failure(
      Core::Error::ErrorCode::FileNotFound,
      QCoreApplication::translate("GameInfoParser", "Failed to parse GameInfo file"),
      gamePath.toQString());
  ```

---

## 4. 单点异常边界契约 (Single-Point Exception Boundary)

遵循 **Single-Point Defense**，异常捕获与转译统一使用 **`Core::Error::ExecutionGuard`**，且**仅驻守两类权威边界**：

1. **任务调度顶层与外部同步入口**：`AsyncTaskRunner`（已内置）及直接向外部暴露的同步顶层接口；
2. **会抛异常的底层 / 第三方库调用点**：`Core::FileSystem` 调用点、`sourcepp`（`vpkpp`/`bsppp`）、`vtfpp` 与会抛异常的 STL 原语封装内部。

**严禁层层嵌套**：项目内部返回 `Core::Result<T>` 的 `Workflow` / `Domain` / `Application` 纯函数之间直接透传 `Core::Result<T>`，**不在中间层重复包裹 `ExecutionGuard::guard`**，也不编写自定义 `runGuarded` 模板。

### 4.1 底层抛异常边界的标准用法 (`Core::Error::ExecutionGuard` & `ExecutionContext`)

```cpp
Core::Error::ExecutionContext ctx{
    .stage = QStringLiteral("Decoding VTF image"),
    .resourcePath = vtfPath.toQString(),
    .targetPath = destPath.toQString()
};

return Core::Error::ExecutionGuard::guard([&]() -> Core::Result<void> {
    // 仅包裹可能抛异常的 vtfpp / Core::FileSystem 调用
    return decodeAndWrite();
}, ctx, QCoreApplication::translate("VtfConverter", "Failed to convert VTF image"));
```

### 4.2 Core 通用性与零领域猜测契约

* **零领域关键词猜词**：`Core::Error::ExecutionGuard` 保持通用，严禁根据异常文本关键词（如 `msg.contains("vpk")`）猜测 Valve 领域错误码。
* **标准异常归一化**：`classifyStdException` 仅归类标准异常（`std::filesystem::filesystem_error`, `std::invalid_argument`, `std::out_of_range`, `std::bad_alloc`）。
* **领域错误强类型透传**：Domain 层返回带领域 `ErrorCode` 的 `Result<T>::failure` 或抛出携带 `Core::Error::Error` 的 `Core::Error::Exception`，`ExecutionGuard` 原样透传错误码并追加 `ExecutionContext` 诊断细节。
* **严禁静默吞异常**：内部 helper 不得用 `catch (...)` 静默返回空值、`false` 或 `nullptr`；确属合法 best-effort fallback 时必须以注释明示语义且不掩盖业务失败。

---

## 5. 跨终态严重性级联与冲突仲裁矩阵

### 5.1 终态严重性级联规则

**Failure 优先级最高，压倒其他所有终态**：
1. 存在任何 `Failure`（`TaskState::Failed`、日志报错或 `Result::failure`）→ **`Failed` / `Failure`**。
2. 否则存在 `Cancelled` → **`Cancelled` / `Cancelled`**。
3. 否则存在 `Skipped` → **`Skipped` / `Skipped`**。
4. 否则 → **`Completed` / `Success`**。

### 5.2 跨终态冲突仲裁矩阵

| Context `TaskState` | Worker `Result` | 契约违规日志 | LogManager 最终 `TaskState` | 回调接收的最终 `Result<T>` |
| :--- | :--- | :--- | :--- | :--- |
| **Failed** (或有错误日志) | **Success** | `error()` ("... 任务失败后返回了 success") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Failed** (或有错误日志) | **Failure** | 否 | `Failed` | `Failure`（保留原错误） |
| **Failed** (或有错误日志) | **Cancelled** | `error()` ("... 任务失败后返回了 cancelled") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Failed** (或有错误日志) | **Skipped** | `error()` ("... 任务失败后返回了 skipped") | `Failed` | 强制转为 `Failure` (`OperationFailed`) |
| **Cancelled** (无错误) | **Success** | `warning()` ("... 任务取消后返回了 success") | `Cancelled` | 强制转为 `Cancelled` |
| **Cancelled** (无错误) | **Failure** | `warning()` ("... 任务取消后返回了 failure") | `Failed` | `Failure`（保留原错误） |
| **Cancelled** (无错误) | **Cancelled** | 否 | `Cancelled` | `Cancelled`（保留原状态） |
| **Cancelled** (无错误) | **Skipped** | `warning()` ("... 任务取消后返回了 skipped") | `Cancelled` | 强制转为 `Cancelled` |
| **Skipped** (无错误) | **Success** | `warning()` ("... 任务跳过后返回了 success") | `Skipped` | 强制转为 `Skipped` |
| **Skipped** (无错误) | **Failure** | `warning()` ("... 任务跳过后返回了 failure") | `Failed` | `Failure`（保留原错误） |
| **Skipped** (无错误) | **Cancelled** | `warning()` ("... 任务跳过后返回了 cancelled") | `Cancelled` | `Cancelled`（保留原状态） |
| **Skipped** (无错误) | **Skipped** | 否 | `Skipped` | `Skipped`（保留原状态） |
| **Completed / Running** | **Success** | 否 | `Completed` | `Success`（保留原结果） |
| **Completed / Running** | **Failure** | 否 | `Failed` | `Failure`（保留原错误） |
| **Completed / Running** | **Cancelled** | 否 | `Cancelled` | `Cancelled`（保留原状态） |
| **Completed / Running** | **Skipped** | 否 | `Skipped` | `Skipped`（保留原状态） |

* **负载与原错误保全**：状态冲突重判时，已有 `partialValue` 与原错误码/诊断细节完整保留；仅原结果为 `Success` 且转 `Failure` 时合成 `OperationFailed`。
* **终态不可逆与进度刷新**：`forceTerminalState` 永不将失败类终态回退为 `Completed`，进入 `Completed` 时进度强制刷新为 1.0；`ImportContext::runStep` 在步骤成功且未取消后才递进进度。
* **取消级联与故障屏障**：`LogManager::cancelTask` 级联取消全部子孙任务（BFS）；致命故障走 `reportFault` → `beginFaultDraining` → `terminateAfterFault`。

---

## 6. 进程机械结果 (`ProcessResult`) 与外部工具子任务绑定

### 6.1 机械状态到标准错误码映射

| 机械状态 (`ProcessStatus`) | 标准错误码 (`ErrorCode`) | 说明 |
| :--- | :--- | :--- |
| `ProcessStatus::Success` | `ErrorCode::Success` | 正常退出且退出码为 0 |
| `ProcessStatus::FailedToStart` | `ErrorCode::ProcessFailed` (或 `ProcessNotFound`) | 可执行文件缺失、权限不足或启动失败 |
| `ProcessStatus::TimedOut` | `ErrorCode::ProcessTimeout` | 执行超时被终止 |
| `ProcessStatus::Crashed` | `ErrorCode::ProcessCrashed` | 进程异常崩溃 |
| `ProcessStatus::NonZeroExit` | `ErrorCode::ProcessFailed` | 非零退出码 |
| `ProcessStatus::Cancelled` | `ErrorCode::Cancelled` | 取消令牌触发终止 |

### 6.2 诊断字段组装示例

```cpp
auto procResult = Core::Process::ProcessRunner::run(cmd, args, options);
if (!procResult.isSuccess()) {
    return Result<void>::failure(
        procResult.toError(),
        QCoreApplication::translate("MapImportWorkflow", "BSP decompilation failed"));
}
```

### 6.3 实时流式捕获与 Tool Sub-Task 绑定模式

`Domain::Tool` 严禁 include `Core/Logging/LogManager.h` 或直调 `LogManager::instance()`，一律通过传入的 `taskCtx` 派生隐藏工具子任务（多文件批处理自适应使用 `Core::Temp::TempFile` 生成 `-filelist`）：

```cpp
// 1. 通过父阶段 TaskLoggingContext 派生隐藏工具子任务（内部自动启动并在父任务记录 [EXEC]）
std::shared_ptr<Core::Logging::TaskLoggingContext> childTask;
if (taskCtx) {
    childTask = taskCtx->createToolSubTask(fullCommandLine, assetBaseName);
}

// 2. 配置流式捕获回调与协作式取消令牌
Core::Process::ProcessOptions options;
options.cancellationToken = cancelToken;
options.timeout = 180000;
options.onStdOutLine = [childTask](const QString& line) {
    if (childTask) {
        childTask->logExternalToolOutput(line, Core::Logging::LogLevel::Info);
        childTask->flush();
    }
};
options.onStdErrLine = [childTask](const QString& line) {
    if (childTask) {
        childTask->logExternalToolOutput(line, Core::Logging::LogLevel::Warning);
        childTask->flush();
    }
};

// 3. 执行外部进程
auto procResult = Core::Process::ProcessRunner::run(executable, options);

// 4. 通过子任务句柄完结终态（用户可见摘要在创建处翻译）
if (childTask) {
    if (procResult.isSuccess()) {
        childTask->finishToolSubTask(
            QCoreApplication::translate("ResourceCompilerTool", "Tool execution completed"));
    } else if (procResult.isCancelled()) {
        childTask->cancelToolSubTask(
            QCoreApplication::translate("ResourceCompilerTool", "Tool execution cancelled by user"));
    } else {
        childTask->failToolSubTask(procResult.errorMessage);
    }
}
```
