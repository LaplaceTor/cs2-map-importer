# CS2 Map Importer — Agent 开发契约

将 Source 1 游戏资产（地图、模型、粒子、音频等）导入 Counter-Strike 2 的 **Windows 桌面 GUI** 应用程序。

## 1. 上下文索引 (Context Pointers)

处理对应分支任务前，先读取权威参考文档：

| 触发分支 / 任务场景 | 目标文档 | 核心内容 |
| :--- | :--- | :--- |
| **架构分层、CMake 隔离、模块职责划分、单点防御位置** | [`ARCHITECTURE.md`](ARCHITECTURE.md) | 五层垂直架构、CMake `PRIVATE` 隔离、**Deep Module** 与 **Deletion Test**、**Single-Point Defense** 对照表、跨层数据流。 |
| **异步任务、错误处理、进程执行、任务日志与状态仲裁** | [`skills/cs2-async-error-handling/SKILL.md`](skills/cs2-async-error-handling/SKILL.md) | **Three-Plane Task** 体系、三层诊断、`ExecutionGuard` 单点边界、终态冲突仲裁矩阵、`createToolSubTask` 绑定。 |
| **查阅已有类、头文件、方法签名与服务契约** | [`skills/cs2-api-reference/SKILL.md`](skills/cs2-api-reference/SKILL.md) | Core / Domain / Workflow / Application / UI 五层已实现 API 字典。 |
| **架构审查、重构演进路线与红线异味自查** | [`skills/cs2-architecture-review/SKILL.md`](skills/cs2-architecture-review/SKILL.md) | 模块迁移表、演进路线图、Deep Module & Single-Point Defense 审查清单与红线异味。 |
| **Qt C++ / QML / UI 通用规范** | `third_party/agent-skills/skills/` | Qt/QML 编码与审查通用技能。 |

---

## 2. 技术栈与平台不变式

* **语言与框架**：C++20、Qt 6.8+、现代 CMake 3.28+、QML / Qt Quick Controls 2 (Fusion)。
* **仅限 Windows**：仅支持 Windows 构建与运行（根 `CMakeLists.txt` 强制 `WIN32` 并全局注入 `NOMINMAX WIN32_LEAN_AND_MEAN`）；保持纯 Windows 实现，不引入非 Windows 平台宏或条件分支。
* **国际化 (i18n)**：英文为源语言，`translations/cs2importer_zh_CN.ts` 由主目标 `cs2importer` 的 `qt_add_translations` 提取；`src/Main.cpp` 按 `QLocale::system()` 自动加载 `.qm`（非中文回退英文）。

---

## 3. 五层单向依赖规则

依赖流向严格单向，CMake `PRIVATE` 链接从编译期强制隔离：

```text
Presentation / UI  ──▶  Application  ──▶  Workflow  ──▶  Domain  ──▶  Core
                             └───────────────┴─────────────────────────┘
```

* **UI (`src/UI/`, `src/qml/`)** 仅依赖 **Application** 门面与 DTO（日志唯一通道为 `Application::Logging::TaskLogService`）。
* **Application (`src/Application/`)** 编排 Workflow、Domain 与 Core；`cs2importer_application` 将 `Workflow` 与 `Domain` 设为 `PRIVATE` 链接，防止底层头文件泄漏至 UI。
* **Workflow (`src/Workflow/`)** 编排多步骤导入/转换流水线，仅依赖 Domain 与 Core。
* **Domain (`src/Domain/`)** 承载 Valve/Source 业务规则与格式解析，仅依赖 Core，保持确定性且不自行裸建线程。
* **Core (`src/Core/`)** 为通用无业务基础设施，可脱离本项目独立复用。
* `src/Legacy/` 未接入构建，仅作迁移参照，新代码不依赖 Legacy。

---

## 4. 三大核心设计准则 (Leading Words)

1. **Deep Module（深模块）**：
   * 追求窄接口、厚实现；所有新增或保留的包装类、中间模型与虚接口必须通过 **Deletion Test（删除测试）**——若删除后未丢失真实不变式或边界，直接移除。
   * 职责归位：多步导入/转换流水线归 `Workflow`；Valve VDF/ACF/GameInfo 等格式解析归 `Domain`；异步调度、共享租约、索引缓存与 UI DTO 转换归 `Application`。
2. **Single-Point Defense（单点契约防御）**：
   * **异常屏障**：`Core::Error::ExecutionGuard` 仅驻守异步任务顶层（`AsyncTaskRunner`）、对外同步入口及会抛异常的底层/第三方库调用点；内部 `Core::Result<T>` 函数链直接透传结果，不嵌套守卫。
   * **路径安全**：调用方通过 `FilesystemPath::resolveBelow` 组合相对路径，并向底层 I/O 原语（`AtomicFile`、`FileSystem::copy/move`、`PackArchive::extractEntryToFile`、`BspPackExtractor`）传递 `expectedBaseDir` 执行内核句柄校验（`verifyFileWithinBase`）；中间层不重复手写 `isSubpathOf`。
   * **前置校验**：路径/参数有效性检查由实际消费层单点完成，上层直接透传 `Core::Result<T>`。
3. **Three-Plane Task（三平面任务）**：
   * 阻塞操作一律由 Application 调度至 Worker 线程执行，面向 UI 的信号通过主线程亲和性调度（`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`）发射。
   * 用户可见导入流程走 `AsyncTaskRunner::runWorkflowTask` / `runTask`（进 UI 任务树）；环境检测、校验、索引预热等非导入后台任务走 `runSystemTask`（`SystemTaskLog`，`taskId=0`）；业务结果一律返回单层 `Core::Result<T>`。
   * 外部 CLI 工具（`resourcecompiler`、`source1import`、`bspsrc`）统一封装在 `Domain::Tool` 并经 `Core::Process::ProcessRunner` 流式执行，通过 `taskCtx->createToolSubTask(...)` 派生隐藏工具子任务；多文件调用使用 `Core::Temp::TempFile` 生成 `-filelist`。
   * 归档解包、VPK 索引点查与 VTF/纹理读写全部使用进程内原生 C++20 实现（`PackArchive`、`BspPackExtractor`、`VtfCodec`、`VtfConverter`、`TextureIO`、`VpkIndex`）与 `Qt6::ZlibPrivate`（Deflate 解压）。

---

## 5. 编码、i18n 与测试契约

* **命名与 Qt 规范**：类型/枚举用 `PascalCase`（枚举显式指定 `: int` 并保留末项尾随逗号）；函数/变量用 `camelCase`；纯属性访问器遵循 Qt 风格不加 `get` 前缀（如 `taskName()`、`index()`）；Qt Model 的 `roleNames()` 使用局部 `static const QHash<int, QByteArray>`；只读容器访问器返回 `const QVector<T>&`；含 `Q_ASSERT` 的函数不标 `noexcept`；`third_party/` 依赖由最底层消费者 `PRIVATE` 链接。
* **i18n 创建处翻译**：面向用户的文案（`Result::message()`、`Error::message()`、任务日志摘要、对话框、QML `qsTr()`）在创建处翻译——QObject 类用 `tr()`，非 QObject 类用 `QCoreApplication::translate("<类名>", "...")`，字符串表用 `QT_TRANSLATE_NOOP`。`Error::details()`、路径、CLI 参数、外部工具原始输出、`debug()`/`sysLog` 与游戏产品名保持英文原文。
* **零常驻测试（Ephemeral Tests）**：仓库不维护常驻 `tests/` 目录或 CTest 配置。验证或排错时按需创建 `test_tmp_*` 临时单任务测试，**任务完成前必须彻底删除测试源码与临时 CMake 配置**。

---

## 6. 构建与翻译指令

```powershell
# 配置与调试构建 (Preset)
cmake --preset windows-debug
cmake --build --preset windows-debug

# 同步提取中文翻译 (.ts)
cmake --build build/windows-debug --target update_translations

# Release 构建
cmake -B build -S .
cmake --build build --config Release
```
