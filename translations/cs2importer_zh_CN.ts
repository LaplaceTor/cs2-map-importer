<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en">
<context>
    <name>AssetExtractor</name>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="120"/>
        <source>Companion extraction failed for &apos;%1&apos;: %2</source>
        <translation>提取伴随文件失败：'%1'：%2</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="126"/>
        <source>Companion &apos;%1&apos; not present in target &apos;%2&apos;</source>
        <translation>目标 '%2' 中不存在伴随文件 '%1'</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="147"/>
        <source>relative asset path is empty</source>
        <translation>资源相对路径为空</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="152"/>
        <source>destination content directory is empty or invalid</source>
        <translation>目标 content 目录为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="164"/>
        <source>Asset extraction cancelled</source>
        <translation>资源提取已取消</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="174"/>
        <source>Asset extraction failed while searching &apos;%1&apos;</source>
        <translation>在 '%1' 中搜索资源时提取失败</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="178"/>
        <source>Asset &apos;%1&apos; not found in target &apos;%2&apos;</source>
        <translation>目标 '%2' 中未找到资源 '%1'</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="185"/>
        <source>Extracted &apos;%1&apos; from &apos;%2&apos;</source>
        <translation>已从 '%2' 提取 '%1'</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/AssetExtractor.cpp" line="198"/>
        <source>Asset &apos;%1&apos; was not found in any search target</source>
        <translation>在任何搜索目标中都未找到资源 '%1'</translation>
    </message>
</context>
<context>
    <name>AsyncTaskRunner</name>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="220"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="361"/>
        <source>%1 (%2)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="225"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="229"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="233"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="367"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="374"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="381"/>
        <source>Task &apos;%1&apos; failed</source>
        <translation>任务 '%1' 失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="311"/>
        <source>Failed to create task context for &apos;%1&apos; (invalid parentTaskId: %2)</source>
        <translation>无法为 '%1' 创建任务上下文（无效的 parentTaskId：%2）</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="386"/>
        <source>Task failed with uncaught exception</source>
        <translation>任务因未捕获的异常而失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="435"/>
        <source>Contract violation: worker returned Result::success after task failed</source>
        <translation>契约冲突：任务已失败，但工作函数返回了 Result::success</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="437"/>
        <source>Contract violation: Task completed with logged errors or explicit failure</source>
        <translation>契约冲突：任务存在已记录的错误或显式失败，却被标记为完成</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="439"/>
        <source>Contract violation: worker returned Result::cancelled after task failed with errors</source>
        <translation>契约冲突：任务已因错误失败，但工作函数返回了 Result::cancelled</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="440"/>
        <source>Contract violation: Task failed with errors before cancellation</source>
        <translation>契约冲突：任务在取消前已因错误失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="442"/>
        <source>Contract violation: worker returned Result::skipped after task failed with errors</source>
        <translation>契约冲突：任务已因错误失败，但工作函数返回了 Result::skipped</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="443"/>
        <source>Contract violation: Task failed with errors before skipping</source>
        <translation>契约冲突：任务在跳过前已因错误失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="447"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="460"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="483"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="512"/>
        <source>Task failed</source>
        <translation>任务失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="452"/>
        <source>Contract violation: worker returned Result::success after task was cancelled</source>
        <translation>契约冲突：任务已取消，但工作函数返回了 Result::success</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="453"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="463"/>
        <source>Contract violation: Task was cancelled</source>
        <translation>契约冲突：任务已被取消</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="455"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="465"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="469"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="488"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="504"/>
        <source>Cancelled</source>
        <translation>已取消</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="457"/>
        <source>Contract violation: worker returned Result::failure after task was cancelled</source>
        <translation>契约冲突：任务已取消，但工作函数返回了 Result::failure</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="462"/>
        <source>Contract violation: worker returned Result::skipped after task was cancelled</source>
        <translation>契约冲突：任务已取消，但工作函数返回了 Result::skipped</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="475"/>
        <source>Contract violation: worker returned Result::success after task was skipped</source>
        <translation>契约冲突：任务已跳过，但工作函数返回了 Result::success</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="476"/>
        <source>Contract violation: Task was skipped</source>
        <translation>契约冲突：任务已被跳过</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="478"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="492"/>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="508"/>
        <source>Skipped</source>
        <translation>已跳过</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="480"/>
        <source>Contract violation: worker returned Result::failure after task was skipped</source>
        <translation>契约冲突：任务已跳过，但工作函数返回了 Result::failure</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="485"/>
        <source>Contract violation: worker returned Result::cancelled after task was skipped</source>
        <translation>契约冲突：任务已跳过，但工作函数返回了 Result::cancelled</translation>
    </message>
    <message>
        <location filename="../src/Application/Async/AsyncTaskRunner.h" line="500"/>
        <source>Completed</source>
        <translation>已完成</translation>
    </message>
</context>
<context>
    <name>AtomicFile</name>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="51"/>
        <source>Cannot open AtomicFile: Already committed</source>
        <translation>无法打开 AtomicFile：已提交</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="60"/>
        <source>Cannot open AtomicFile: Target path is empty</source>
        <translation>无法打开 AtomicFile：目标路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="69"/>
        <source>Failed to create parent directory for atomic write: %1</source>
        <translation>为原子写入创建父目录失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="77"/>
        <source>Failed to open QSaveFile for target &apos;%1&apos;: %2</source>
        <translation>为 %1 打开 QSaveFile 失败：%2</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="92"/>
        <source>AtomicFile QSaveFile is not open</source>
        <translation>AtomicFile 的 QSaveFile 未打开</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="99"/>
        <source>Failed to write to QSaveFile for target &apos;%1&apos;: %2</source>
        <translation>写入 %1 的 QSaveFile 失败：%2</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="112"/>
        <source>Cannot commit AtomicFile: File was not opened or written</source>
        <translation>无法提交 AtomicFile：文件未打开或未写入</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/AtomicFile.cpp" line="118"/>
        <source>Failed to commit QSaveFile for target &apos;%1&apos;: %2</source>
        <translation>提交 %1 的 QSaveFile 失败：%2</translation>
    </message>
</context>
<context>
    <name>BspEmbeddedExtractor</name>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="18"/>
        <source>BSP file path is empty or invalid</source>
        <translation>BSP 文件路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="23"/>
        <source>BSP file not found</source>
        <translation>未找到 BSP 文件</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="29"/>
        <source>destination directory is empty or invalid</source>
        <translation>目标目录为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="34"/>
        <source>BSP embedded file extraction cancelled</source>
        <translation>BSP 内嵌文件提取已取消</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="46"/>
        <source>Extracted %1 embedded file(s) from &apos;%2&apos; to &apos;%3&apos;</source>
        <translation>已从 '%2' 提取 %1 个内嵌文件到 '%3'</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="50"/>
        <source>BSP embedded file extraction cancelled after %1 file(s)</source>
        <translation>BSP 内嵌文件提取在 %1 个文件后被取消</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/BspEmbeddedExtractor.cpp" line="53"/>
        <source>Failed to extract embedded files from &apos;%1&apos;: %2</source>
        <translation>从 '%1' 提取内嵌文件失败：%2</translation>
    </message>
</context>
<context>
    <name>BspPackExtractor</name>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="35"/>
        <source>BSP file path is empty or invalid</source>
        <translation>BSP 文件路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="40"/>
        <source>BSP file not found</source>
        <translation>未找到 BSP 文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="46"/>
        <source>destination directory is empty or invalid</source>
        <translation>目标目录为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="51"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="140"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="250"/>
        <source>BSP embedded file extraction cancelled</source>
        <translation>BSP 内嵌文件提取已取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="58"/>
        <source>BSP file failed to load or has an invalid signature</source>
        <translation>BSP 文件加载失败或签名无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="64"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="70"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="81"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="90"/>
        <location filename="../src/Domain/Package/BspPackExtractor.cpp" line="135"/>
        <source>BSP contains no embedded files</source>
        <translation>BSP 不包含任何内嵌文件</translation>
    </message>
</context>
<context>
    <name>DirectorySnapshot</name>
    <message>
        <location filename="../src/Core/FileSystem/DirectorySnapshot.cpp" line="23"/>
        <source>Cannot capture directory snapshot: Path is empty</source>
        <translation>无法捕获目录快照：路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/DirectorySnapshot.cpp" line="30"/>
        <source>Directory does not exist: %1</source>
        <translation>目录不存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/DirectorySnapshot.cpp" line="56"/>
        <source>Cannot compare DirectorySnapshots from different root directories: &apos;%1&apos; vs &apos;%2&apos;</source>
        <translation>无法比较来自不同根目录的目录快照：'%1' 与 '%2'</translation>
    </message>
</context>
<context>
    <name>DspPreset</name>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="9"/>
        <source>Normal (off)</source>
        <translation>无（关闭）</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="10"/>
        <source>Generic</source>
        <translation>通用</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="11"/>
        <source>Metal Small</source>
        <translation>金属 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="12"/>
        <source>Metal Medium</source>
        <translation>金属 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="13"/>
        <source>Metal Large</source>
        <translation>金属 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="14"/>
        <source>Tunnel Small</source>
        <translation>隧道 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="15"/>
        <source>Tunnel Medium</source>
        <translation>隧道 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="16"/>
        <source>Tunnel Large</source>
        <translation>隧道 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="17"/>
        <source>Chamber Small</source>
        <translation>密室 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="18"/>
        <source>Chamber Medium</source>
        <translation>密室 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="19"/>
        <source>Chamber Large</source>
        <translation>密室 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="20"/>
        <source>Bright Small</source>
        <translation>明亮 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="21"/>
        <source>Bright Medium</source>
        <translation>明亮 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="22"/>
        <source>Bright Large</source>
        <translation>明亮 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="23"/>
        <source>Water 1</source>
        <translation>水声 1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="24"/>
        <source>Water 2</source>
        <translation>水声 2</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="25"/>
        <source>Water 3</source>
        <translation>水声 3</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="26"/>
        <source>Concrete Small</source>
        <translation>混凝土 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="27"/>
        <source>Concrete Medium</source>
        <translation>混凝土 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="28"/>
        <source>Concrete Large</source>
        <translation>混凝土 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="29"/>
        <source>Outside Alley</source>
        <translation>室外 小巷</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="30"/>
        <source>Outside Street</source>
        <translation>室外 街道</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="31"/>
        <source>Outside Open</source>
        <translation>室外 开阔</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="32"/>
        <source>Cavern Small</source>
        <translation>洞穴 小</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="33"/>
        <source>Cavern Medium</source>
        <translation>洞穴 中</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="34"/>
        <source>Cavern Large</source>
        <translation>洞穴 大</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="35"/>
        <source>Weirdo 1</source>
        <translation>怪异 1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="36"/>
        <source>Weirdo 2</source>
        <translation>怪异 2</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/DspPresetRegistry.cpp" line="37"/>
        <source>Weirdo 3</source>
        <translation>怪异 3</translation>
    </message>
</context>
<context>
    <name>ExecutionGuard</name>
    <message>
        <location filename="../src/Application/Execution/ExecutionGuard.h" line="49"/>
        <source>Unhandled standard exception</source>
        <translation>未处理的标准异常</translation>
    </message>
    <message>
        <location filename="../src/Application/Execution/ExecutionGuard.h" line="62"/>
        <location filename="../src/Application/Execution/ExecutionGuard.h" line="63"/>
        <source>Unhandled unknown exception</source>
        <translation>未处理的未知异常</translation>
    </message>
</context>
<context>
    <name>FileLease</name>
    <message>
        <location filename="../src/Core/FileSystem/FileLease.cpp" line="46"/>
        <source>File path is empty.</source>
        <translation>文件路径为空。</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileLease.cpp" line="51"/>
        <source>File does not exist: %1</source>
        <translation>文件不存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileLease.cpp" line="55"/>
        <source>Path is not a regular file: %1</source>
        <translation>路径不是常规文件：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileLease.cpp" line="98"/>
        <source>Failed to acquire exclusive handle on &apos;%1&apos; (Windows Error %2): %3</source>
        <translation>获取 '%1' 的独占句柄失败（Windows 错误 %2）：%3</translation>
    </message>
</context>
<context>
    <name>FileSystem</name>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="64"/>
        <source>Cannot create directory: Path is empty</source>
        <translation>无法创建目录：路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="75"/>
        <source>Failed to create directory: %1</source>
        <translation>创建目录失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="83"/>
        <source>Cannot remove: Path is empty</source>
        <translation>无法删除：路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="96"/>
        <source>Failed to remove directory recursively: %1</source>
        <translation>递归删除目录失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="103"/>
        <source>Failed to remove file: %1 (%2)</source>
        <translation>删除文件失败：%1（%2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="112"/>
        <source>Cannot copy: Source or destination path is empty</source>
        <translation>无法复制：源路径或目标路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="119"/>
        <source>Cannot copy: Source path does not exist: %1</source>
        <translation>无法复制：源路径不存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="131"/>
        <source>Cannot copy directory: Destination is inside source directory (%1 -&gt; %2)</source>
        <translation>无法复制目录：目标位于源目录内部（%1 -> %2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="136"/>
        <source>Cannot copy directory: Source is inside destination directory (%1 -&gt; %2)</source>
        <translation>无法复制目录：源位于目标目录内部（%1 -> %2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="148"/>
        <source>Cannot copy: Destination file already exists: %1</source>
        <translation>无法复制：目标文件已存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="154"/>
        <source>Cannot copy: Failed to overwrite existing destination file: %1</source>
        <translation>无法复制：覆盖已有目标文件失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="166"/>
        <source>Failed to copy file from %1 to %2</source>
        <translation>从 %1 复制文件到 %2 失败</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="193"/>
        <source>Cannot move: Source or destination path is empty</source>
        <translation>无法移动：源路径或目标路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="200"/>
        <source>Cannot move: Source path does not exist: %1</source>
        <translation>无法移动：源路径不存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="212"/>
        <source>Cannot move directory: Destination is inside source directory (%1 -&gt; %2)</source>
        <translation>无法移动目录：目标位于源目录内部（%1 -> %2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="217"/>
        <source>Cannot move directory: Source is inside destination directory (%1 -&gt; %2)</source>
        <translation>无法移动目录：源位于目标目录内部（%1 -> %2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="227"/>
        <source>Cannot move: Destination path already exists: %1</source>
        <translation>无法移动：目标路径已存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="239"/>
        <source>Cannot move: Failed to create temporary backup for existing destination: %1</source>
        <translation>无法移动：为已有目标创建临时备份失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="279"/>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="288"/>
        <source>Move partially completed: destination copy kept, source removal failed</source>
        <translation>移动部分完成：已保留目标副本，但删除源失败</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="305"/>
        <source>Cannot read file: Path is empty</source>
        <translation>无法读取文件：路径为空</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="312"/>
        <source>Cannot read file: File does not exist: %1</source>
        <translation>无法读取文件：文件不存在：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="319"/>
        <source>Cannot open file for reading: %1 (%2)</source>
        <translation>无法打开文件进行读取：%1（%2）</translation>
    </message>
    <message>
        <location filename="../src/Core/FileSystem/FileSystem.cpp" line="329"/>
        <source>Cannot write file: Path is empty</source>
        <translation>无法写入文件：路径为空</translation>
    </message>
</context>
<context>
    <name>GameDetectService</name>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="23"/>
        <source>Detect Environment</source>
        <translation>检测环境</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="63"/>
        <source>Invalid custom Steam path</source>
        <translation>自定义 Steam 路径无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="77"/>
        <source>No Steam installation detected on this system.</source>
        <translation>未在此系统上检测到 Steam 安装。</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="97"/>
        <source>No Steam libraries found.</source>
        <translation>未找到 Steam 库。</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="113"/>
        <source>Invalid Steam library path: %1</source>
        <translation>Steam 库路径无效：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="159"/>
        <source>Environment detection failed</source>
        <translation>环境检测失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="179"/>
        <source>Cannot detect games with Unknown or Custom type in Steam libraries</source>
        <translation>无法在 Steam 库中检测“未知”或“自定义”类型的游戏</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="193"/>
        <source>No Steam libraries detected on this host</source>
        <translation>此主机上未检测到 Steam 库</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="223"/>
        <source>Game not found in detected Steam libraries</source>
        <translation>在检测到的 Steam 库中未找到该游戏</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="225"/>
        <location filename="../src/Application/Environment/GameDetectService.cpp" line="226"/>
        <source>Steam game detection failed</source>
        <translation>Steam 游戏检测失败</translation>
    </message>
</context>
<context>
    <name>GameEnvironmentService</name>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="182"/>
        <source>Validate %1</source>
        <translation>验证 %1</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="220"/>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="274"/>
        <source>Target path is empty</source>
        <translation>目标路径为空</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="233"/>
        <source>Source 1 validation failed</source>
        <translation>Source 1 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="246"/>
        <source>Validate Source 2</source>
        <translation>验证 Source 2</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="280"/>
        <source>Source 2 validation failed</source>
        <translation>Source 2 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="313"/>
        <source>List Source 2 Addons</source>
        <translation>列出 Source 2 附加内容</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="332"/>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="341"/>
        <location filename="../src/Application/Environment/GameEnvironmentService.cpp" line="350"/>
        <source>VPK signature lease service is unavailable</source>
        <translation>VPK 签名租约服务不可用</translation>
    </message>
</context>
<context>
    <name>GameErrors</name>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="45"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="49"/>
        <source>Unsupported or unrecognised game type</source>
        <translation>不支持或无法识别的游戏类型</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="55"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="59"/>
        <source>GameInfo file was not found</source>
        <translation>未找到 GameInfo 文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="65"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="69"/>
        <source>Game configuration does not match expected game type</source>
        <translation>游戏配置与预期的游戏类型不匹配</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="75"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="79"/>
        <source>Steam AppID does not match expected game</source>
        <translation>Steam AppID 与预期游戏不符</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="85"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="89"/>
        <source>Invalid game installation structure</source>
        <translation>游戏安装结构无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameErrors.h" line="95"/>
        <location filename="../src/Domain/Game/GameErrors.h" line="99"/>
        <source>Custom GameInfo is empty and has no valid gameinfo file path</source>
        <translation>自定义 GameInfo 为空且没有有效的 gameinfo 文件路径</translation>
    </message>
</context>
<context>
    <name>GameInfoParser</name>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="139"/>
        <source>GameInfo file path is invalid or empty</source>
        <translation>GameInfo 文件路径无效或为空</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="141"/>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="148"/>
        <source>GameInfo parsing failed</source>
        <translation>GameInfo 解析失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="146"/>
        <source>GameInfo file does not exist</source>
        <translation>GameInfo 文件不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="156"/>
        <source>Failed to load GameInfo</source>
        <translation>加载 GameInfo 失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="170"/>
        <source>GameInfo path hint is invalid</source>
        <translation>GameInfo 路径提示无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInfoParser.cpp" line="179"/>
        <source>Failed to parse GameInfo content</source>
        <translation>解析 GameInfo 内容失败</translation>
    </message>
</context>
<context>
    <name>GameInstallationResolver</name>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="100"/>
        <source>Source 1 directory path is empty or invalid</source>
        <translation>Source 1 目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="102"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="109"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="116"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="123"/>
        <source>Source 1 directory resolution failed</source>
        <translation>解析 Source 1 目录失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="107"/>
        <source>Source 1 directory does not exist</source>
        <translation>Source 1 目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="114"/>
        <source>Source 1 path is not a directory</source>
        <translation>Source 1 路径不是目录</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="136"/>
        <source>Source 2 directory path is empty or invalid</source>
        <translation>Source 2 目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="138"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="145"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="166"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="217"/>
        <source>Source 2 directory resolution failed</source>
        <translation>解析 Source 2 目录失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="143"/>
        <source>Source 2 directory does not exist</source>
        <translation>Source 2 目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="164"/>
        <source>Target file is not a Source 2 gameinfo.gi</source>
        <translation>目标文件不是 Source 2 的 gameinfo.gi</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="192"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="277"/>
        <source>/gameinfo.gi</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="215"/>
        <source>Could not locate gameinfo.gi in Source 2 structure</source>
        <translation>在 Source 2 目录结构中找不到 gameinfo.gi</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="224"/>
        <source>Failed to parse Source 2 gameinfo.gi</source>
        <translation>解析 Source 2 gameinfo.gi 失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="236"/>
        <source>Source 2 gameinfo validation failed</source>
        <translation>Source 2 gameinfo 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="250"/>
        <source>GameInfo path is empty or invalid</source>
        <translation>GameInfo 路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="252"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="259"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="301"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="311"/>
        <source>GameInfo inspection failed</source>
        <translation>检查 GameInfo 失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="257"/>
        <source>GameInfo path does not exist</source>
        <translation>GameInfo 路径不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="289"/>
        <source>/gameinfo.txt</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="299"/>
        <source>No gameinfo.txt or gameinfo.gi found in directory</source>
        <translation>目录中未找到 gameinfo.txt 或 gameinfo.gi</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="309"/>
        <source>GameInfo file not found</source>
        <translation>未找到 GameInfo 文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="321"/>
        <source>Failed to parse GameInfo file</source>
        <translation>解析 GameInfo 文件失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="339"/>
        <source>Directory path is empty or invalid</source>
        <translation>目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="341"/>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="348"/>
        <source>Game directory resolution failed</source>
        <translation>解析游戏目录失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameInstallationResolver.cpp" line="346"/>
        <source>Directory does not exist</source>
        <translation>目录不存在</translation>
    </message>
</context>
<context>
    <name>GameInstallationValidator</name>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="69"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="129"/>
        <source>Target directory path is empty or invalid</source>
        <translation>目标目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="71"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="81"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="91"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="109"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="110"/>
        <source>Source 1 validation failed</source>
        <translation>Source 1 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="79"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="139"/>
        <source>Target directory does not exist</source>
        <translation>目标目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="107"/>
        <source>Failed to create installation from resolved Source 1 path</source>
        <translation>从解析出的 Source 1 路径创建游戏安装信息失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="131"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="141"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="151"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="169"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="170"/>
        <source>Source 2 validation failed</source>
        <translation>Source 2 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="167"/>
        <source>Failed to create installation from resolved Source 2 path</source>
        <translation>从解析出的 Source 2 路径创建游戏安装信息失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="188"/>
        <source>GameInfo path is empty or invalid</source>
        <translation>GameInfo 路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="190"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="200"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="210"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="228"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="229"/>
        <source>GameInfo inspection failed</source>
        <translation>检查 GameInfo 失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="198"/>
        <source>GameInfo path does not exist</source>
        <translation>GameInfo 路径不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="226"/>
        <source>Failed to create installation from inspected GameInfo</source>
        <translation>从检查的 GameInfo 创建游戏安装信息失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="250"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="262"/>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="263"/>
        <source>Game directory validation failed</source>
        <translation>游戏目录校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/GameInstallationValidator.cpp" line="260"/>
        <source>Failed to create installation from resolved game directory</source>
        <translation>从解析出的游戏目录创建游戏安装信息失败</translation>
    </message>
</context>
<context>
    <name>GameSelectorBox</name>
    <message>
        <location filename="../src/qml/cs2importer/components/GameSelectorBox.qml" line="8"/>
        <source>Game</source>
        <translation>游戏</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/GameSelectorBox.qml" line="94"/>
        <source>Press to Select gameinfo.txt</source>
        <translation>点击选择 gameinfo.txt</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/GameSelectorBox.qml" line="94"/>
        <source>Press to Select Game Folder</source>
        <translation>点击选择游戏目录</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/GameSelectorBox.qml" line="115"/>
        <source>Validate Game File</source>
        <translation>校验游戏文件</translation>
    </message>
</context>
<context>
    <name>GameValidator</name>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="16"/>
        <source>Cannot validate against GameType::Unknown</source>
        <translation>无法针对 GameType::Unknown 进行校验</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="25"/>
        <source>Custom GameInfo is empty and has no valid gameinfo file path</source>
        <translation>自定义 GameInfo 为空且没有有效的 gameinfo 文件路径</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="32"/>
        <source>Game definition not found for type</source>
        <translation>未找到该类型的游戏定义</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="79"/>
        <source>GameInfo AppID belongs to another game</source>
        <translation>GameInfo 的 AppID 属于另一款游戏</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="132"/>
        <source>GameInfo does not match expected game type</source>
        <translation>GameInfo 与预期的游戏类型不匹配</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="267"/>
        <source>Game directory path is empty or invalid</source>
        <translation>游戏目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="269"/>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="276"/>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="290"/>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="298"/>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="316"/>
        <source>Game directory validation failed</source>
        <translation>游戏目录校验失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="274"/>
        <source>Game directory does not exist</source>
        <translation>游戏目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="288"/>
        <source>Path is neither a directory nor a valid custom gameinfo file</source>
        <translation>路径既不是目录也不是有效的自定义 gameinfo 文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Game/GameValidator.cpp" line="296"/>
        <source>GameInfo file was not found</source>
        <translation>未找到 GameInfo 文件</translation>
    </message>
</context>
<context>
    <name>GameViewModel</name>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="99"/>
        <source>Counter-Strike 2 is Running</source>
        <translation>Counter-Strike 2 正在运行</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="100"/>
        <source>vpk.signatures is currently in use by Counter-Strike 2 or another application.

Please close the occupying application and click Retry, or Exit to quit.</source>
        <translation>vpk.signatures 正被 Counter-Strike 2 或其他程序占用。

请关闭占用该文件的程序后点击“重试”，或点击“退出”。</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="105"/>
        <source>Access Denied</source>
        <translation>访问被拒绝</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="106"/>
        <source>Permission denied when trying to access vpk.signatures:
%1

Please check file permissions or run as administrator.</source>
        <translation>访问 vpk.signatures 时权限被拒绝：
%1

请检查文件权限或以管理员身份运行。</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="111"/>
        <source>File Lease Failed</source>
        <translation>文件租约失败</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="112"/>
        <source>Failed to acquire exclusive lease on vpk.signatures:
%1</source>
        <translation>获取 vpk.signatures 独占租约失败：
%1</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="272"/>
        <source>The selected directory is not a valid installation for the selected game.
Please verify that it contains the expected game files and gameinfo.txt.</source>
        <translation>所选目录不是所选游戏的有效安装。
请确认其中包含预期的游戏文件和 gameinfo.txt。</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="276"/>
        <source>Invalid Source 1 Installation</source>
        <translation>Source 1 安装无效</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="307"/>
        <source>The selected folder is not a valid Source 2 installation.
Please ensure it contains game/csgo/gameinfo.gi or a valid Source 2 game layout.</source>
        <translation>所选文件夹不是有效的 Source 2 安装。
请确认其中包含 game/csgo/gameinfo.gi 或有效的 Source 2 目录结构。</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="311"/>
        <source>Invalid Source 2 Installation</source>
        <translation>Source 2 安装无效</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="327"/>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="343"/>
        <source>Steam Validation Unavailable</source>
        <translation>Steam 校验不可用</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="329"/>
        <source>Could not initiate Steam validation. Make sure Steam is running and the game is installed.</source>
        <translation>无法启动 Steam 校验。请确保 Steam 正在运行且已安装该游戏。</translation>
    </message>
    <message>
        <location filename="../src/UI/ViewModels/GameViewModel.cpp" line="345"/>
        <source>Could not initiate Steam validation for Source 2. Make sure Steam is running and Counter-Strike 2 is installed.</source>
        <translation>无法为 Source 2 启动 Steam 校验。请确保 Steam 正在运行且已安装 Counter-Strike 2。</translation>
    </message>
</context>
<context>
    <name>ImportContext</name>
    <message>
        <location filename="../src/Workflow/Common/ImportContext.h" line="49"/>
        <source>Operation was cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/ImportContext.h" line="103"/>
        <source>Cancelled before step: %1</source>
        <translation>步骤 %1 开始前已取消</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/ImportContext.h" line="113"/>
        <source>Cancelled during step: %1</source>
        <translation>步骤 %1 执行期间已取消</translation>
    </message>
</context>
<context>
    <name>ImportPrerequisiteService</name>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="38"/>
        <source>Import cancelled before start</source>
        <translation>导入在开始前已取消</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="46"/>
        <source>Source 1 game directory cannot be empty</source>
        <translation>Source 1 游戏目录不能为空</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="51"/>
        <source>Source 1 game directory does not exist</source>
        <translation>Source 1 游戏目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="79"/>
        <source>CS2 base directory cannot be empty</source>
        <translation>CS2 根目录不能为空</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="84"/>
        <source>CS2 base directory does not exist</source>
        <translation>CS2 根目录不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="93"/>
        <source>Target addon name cannot be empty</source>
        <translation>目标附加内容（Addon）名称不能为空</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="105"/>
        <source>Could not acquire vpk.signatures lease at &apos;%1&apos;: %2</source>
        <translation>无法获取 '%1' 的 vpk.signatures 租约：%2</translation>
    </message>
    <message>
        <location filename="../src/Application/Common/ImportPrerequisiteService.cpp" line="108"/>
        <source>Acquired vpk.signatures exclusive lease</source>
        <translation>已获取 vpk.signatures 独占租约</translation>
    </message>
</context>
<context>
    <name>KeyValuesDocument</name>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesDocument.cpp" line="26"/>
        <source>Failed to load KeyValues document from %1: %2</source>
        <translation>从 %1 加载 KeyValues 文档失败：%2</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesDocument.cpp" line="39"/>
        <source>Failed to parse KeyValues string: %1</source>
        <translation>解析 KeyValues 字符串失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesDocument.cpp" line="53"/>
        <source>Path is empty or invalid</source>
        <translation>路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesDocument.cpp" line="59"/>
        <source>File does not exist</source>
        <translation>文件不存在</translation>
    </message>
</context>
<context>
    <name>KeyValuesParser</name>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="26"/>
        <source>Unexpected &apos;}&apos; at top level</source>
        <translation>顶层出现意外的 '}'</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="33"/>
        <source>Unexpected &apos;{&apos; without a preceding key</source>
        <translation>出现意外的 '{'（缺少前置键名）</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="52"/>
        <source>KeyValues parsing failed: %1</source>
        <translation>KeyValues 解析失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="63"/>
        <source>Expected string token for key</source>
        <translation>键名应为字符串标记</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="100"/>
        <source>Unexpected &apos;{&apos; inside section</source>
        <translation>节内部出现意外的 '{'</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="116"/>
        <source>Unclosed &apos;{&apos; block (reached EOF)</source>
        <translation>未闭合的 '{' 块（已到文件末尾）</translation>
    </message>
    <message>
        <location filename="../src/Core/KeyValues/KeyValuesParser.cpp" line="145"/>
        <source>Unexpected token</source>
        <translation>意外的标记</translation>
    </message>
</context>
<context>
    <name>LogTaskCard</name>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="178"/>
        <source>PENDING</source>
        <translation>等待中</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="179"/>
        <source>RUNNING</source>
        <translation>运行中</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="180"/>
        <source>COMPLETED</source>
        <translation>已完成</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="181"/>
        <source>FAILED</source>
        <translation>失败</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="182"/>
        <source>CANCELLED</source>
        <translation>已取消</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="183"/>
        <source>SKIPPED</source>
        <translation>已跳过</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="203"/>
        <source>%1 sub-task(s)</source>
        <translation>%1 个子任务</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="212"/>
        <source>%1 log(s)</source>
        <translation>%1 条日志</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/components/LogTaskCard.qml" line="373"/>
        <source>Detailed Log</source>
        <translation>详细日志</translation>
    </message>
</context>
<context>
    <name>LogViewModel</name>
    <message>
        <location filename="../src/UI/ViewModels/LogViewModel.cpp" line="792"/>
        <source>General</source>
        <translation>通用</translation>
    </message>
</context>
<context>
    <name>LogWindow</name>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="66"/>
        <source>CS2 IMPORTER - Logs</source>
        <translation>CS2 IMPORTER - 日志</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="82"/>
        <source>Open log folder</source>
        <translation>打开日志文件夹</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="91"/>
        <source>Expand All</source>
        <translation>全部展开</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="100"/>
        <source>Collapse All</source>
        <translation>全部折叠</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="109"/>
        <source>Auto-scroll</source>
        <translation>自动滚动</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/LogWindow.qml" line="123"/>
        <source>Tasks: %1 | Messages: %2</source>
        <translation>任务：%1 | 消息：%2</translation>
    </message>
</context>
<context>
    <name>Main</name>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="17"/>
        <source>CS2 IMPORTER</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="84"/>
        <source>Alert</source>
        <translation>提示</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="90"/>
        <source>Counter-Strike 2 is Running</source>
        <translation>Counter-Strike 2 正在运行</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="91"/>
        <source>vpk.signatures is currently in use by Counter-Strike 2 or another application.

Please close Counter-Strike 2 before using CS2 Importer.</source>
        <translation>vpk.signatures 正被 Counter-Strike 2 或其他程序占用。

请在使用 CS2 Importer 前关闭 Counter-Strike 2。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="110"/>
        <source>Confirm Game Validation</source>
        <translation>确认游戏校验</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="111"/>
        <source>Validating game files through Steam is only needed if you are certain there are corrupted or missing game files.

Do you want to proceed?</source>
        <translation>只有在确信游戏文件存在损坏或缺失时，才需要通过 Steam 校验游戏文件。

是否继续？</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="144"/>
        <source>Select Source 1 Game Folder</source>
        <translation>选择 Source 1 游戏目录</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="154"/>
        <source>Select gameinfo.txt</source>
        <translation>选择 gameinfo.txt</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="165"/>
        <source>Select Source 2 Game Folder</source>
        <translation>选择 Source 2 游戏目录</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="175"/>
        <source>Select VMF or BSP Map File</source>
        <translation>选择 VMF 或 BSP 地图文件</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="184"/>
        <source>Select Source 1 MDL Model File</source>
        <translation>选择 Source 1 MDL 模型文件</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="193"/>
        <source>Select Source 1 PCF Particle File</source>
        <translation>选择 Source 1 PCF 粒子文件</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="227"/>
        <source>Map</source>
        <translation>地图</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="234"/>
        <source>Model</source>
        <translation>模型</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="241"/>
        <source>Particle</source>
        <translation>粒子</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="326"/>
        <location filename="../src/qml/cs2importer/Main.qml" line="329"/>
        <source>Theme:System</source>
        <translation>主题：跟随系统</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="327"/>
        <source>Theme:Light</source>
        <translation>主题：浅色</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="328"/>
        <source>Theme:Dark</source>
        <translation>主题：深色</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="343"/>
        <source>Check Update</source>
        <translation>检查更新</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="361"/>
        <source>v%1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/Main.qml" line="375"/>
        <source>LOG</source>
        <translation>日志</translation>
    </message>
</context>
<context>
    <name>MainController</name>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="47"/>
        <source>Task in Progress</source>
        <translation>任务进行中</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="48"/>
        <source>An operation is currently executing. You cannot change tabs until the current operation completes.</source>
        <translation>当前有操作正在执行。在该操作完成前无法切换标签页。</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="102"/>
        <source>Feature in Development</source>
        <translation>功能开发中</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="103"/>
        <source>Import execution will be connected in the upcoming Workflow integration stage.</source>
        <translation>导入执行将在后续的 Workflow 集成阶段接入。</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="182"/>
        <source>Particle Import Complete</source>
        <translation>粒子导入完成</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="184"/>
        <source>All particles were successfully imported and compiled.</source>
        <translation>所有粒子已成功导入并编译。</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="189"/>
        <source>Import Cancelled</source>
        <translation>导入已取消</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="190"/>
        <source>Particle import was cancelled by user.</source>
        <translation>粒子导入已被用户取消。</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="194"/>
        <source>Particle Import Failed</source>
        <translation>粒子导入失败</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="196"/>
        <source>An error occurred during particle import.</source>
        <translation>粒子导入过程中发生错误。</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="214"/>
        <source>Check for Updates</source>
        <translation>检查更新</translation>
    </message>
    <message>
        <location filename="../src/UI/Controllers/MainController.cpp" line="215"/>
        <source>You are currently running the latest development version (v%1).</source>
        <translation>当前已是最新开发版本（v%1）。</translation>
    </message>
</context>
<context>
    <name>MapTab</name>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="30"/>
        <source>Source 1 Game</source>
        <translation>Source 1 游戏</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="58"/>
        <source>Counter-Strike 2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="84"/>
        <source>Select VMF/BSP</source>
        <translation>选择 VMF/BSP</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="168"/>
        <source>Addon Name</source>
        <translation>Addon 名称</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="195"/>
        <source>NEW</source>
        <translation>新建</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="235"/>
        <source>OPTIONS</source>
        <translation>选项</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="255"/>
        <source>Clean Unnecessary Faces (-usebsp)</source>
        <translation>清理多余面（-usebsp）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="257"/>
        <source>Runs the map through a special VBSP process to generate clean map geometry from brushes, removing hidden faces and stitching edges for easier editing in Hammer.
• Preserves world (vis) and func_detail brushes for Source 2 compatibility.
• Merges all func_instances into world geometry.
• Note: Final geometry will be triangulated.</source>
        <translation>通过特殊的 VBSP 流程从笔刷生成干净的地图几何体，移除隐藏面并缝合边缘，便于在 Hammer 中编辑。
• 为兼容 Source 2，保留 world（vis）与 func_detail 笔刷。
• 将所有 func_instance 合并进世界几何体。
• 注意：最终几何体将被三角化。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="263"/>
        <source>Preserve func_instance Sub-maps (-usebsp_nomergeinstances)</source>
        <translation>保留 func_instance 子地图（-usebsp_nomergeinstances）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="266"/>
        <source>Generates clean map geometry while preserving func_instance sub-maps as separate entities instead of merging them into world geometry.
• Takes longer as it runs through the import process twice.
• Final geometry will be triangulated.
• Requires Clean Unnecessary Faces (-usebsp) to be enabled.</source>
        <translation>生成干净的地图几何体，同时将 func_instance 子地图保留为独立实体，而不合并进世界几何体。
• 导入流程会执行两遍，耗时更长。
• 最终几何体将被三角化。
• 需要启用“清理多余面（-usebsp）”。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="272"/>
        <source>Keep func_detail as func_brush</source>
        <translation>将 func_detail 保留为 func_brush</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="274"/>
        <source>Converts Source 1 func_detail brushes into separate func_brush entities instead of baking them into static world geometry.</source>
        <translation>将 Source 1 的 func_detail 笔刷转换为独立的 func_brush 实体，而不是烘焙进静态世界几何体。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="280"/>
        <source>Skip Dependencies (Map Geometry Only)</source>
        <translation>跳过依赖（仅地图几何体）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="282"/>
        <source>Generates only the .vmap map structure, skipping model, material, texture, and sound extraction to accelerate conversion for quick testing.</source>
        <translation>仅生成 .vmap 地图结构，跳过模型、材质、贴图和声音提取，以加速转换，便于快速测试。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="302"/>
        <source>START</source>
        <translation>开始</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/MapTab.qml" line="326"/>
        <source>STOP</source>
        <translation>停止</translation>
    </message>
</context>
<context>
    <name>ModelTab</name>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="30"/>
        <source>Source 1 Game</source>
        <translation>Source 1 游戏</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="58"/>
        <source>Counter-Strike 2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="84"/>
        <source>SELECT MDL</source>
        <translation>选择 MDL</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="156"/>
        <source>OPTIONS</source>
        <translation>选项</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="175"/>
        <source>Skip Animation Import (-skipcommondmxwrite)</source>
        <translation>跳过动画导入（-skipcommondmxwrite）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="177"/>
        <source>Converts only the static 3D mesh model without extracting skeletal animations (.dmx files), significantly accelerating conversion.</source>
        <translation>仅转换静态 3D 网格模型，不提取骨骼动画（.dmx 文件），显著加快转换速度。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="182"/>
        <source>Convert Coordinate (Y-Up to Z-Up) (-YupToZup)</source>
        <translation>转换坐标（Y 轴向上转 Z 轴向上）（-YupToZup）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="184"/>
        <source>Transforms the model&apos;s base pose from Y-axis Up (Source 1/Maya) to Z-axis Up (Source 2 standard) to fix lying-down or rotated models.</source>
        <translation>将模型的基础姿态从 Y 轴向上（Source 1/Maya）转换为 Z 轴向上（Source 2 标准），修复模型躺倒或旋转的问题。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="189"/>
        <source>Override &quot;lean&quot; Sequence (-overridelean)</source>
        <translation>覆盖 lean 序列（-overridelean）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="191"/>
        <source>Overrides directional leaning animation sequences for characters or weapons with standard default poses.</source>
        <translation>用标准默认姿态覆盖角色或武器的方向倾斜动画序列。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="196"/>
        <source>Use Studiohdr Bounds (-header_hull_bounds)</source>
        <translation>使用 Studiohdr 边界（-header_hull_bounds）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="198"/>
        <source>Uses the bounding box dimensions defined in the MDL studio header directly, rather than calculating boundaries from collision physics hulls.</source>
        <translation>直接使用 MDL studio 头中定义的包围盒尺寸，而不是通过碰撞物理包络计算边界。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="203"/>
        <source>Import All LODs (-lods)</source>
        <translation>导入全部 LOD（-lods）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="205"/>
        <source>Imports all distance-based Level-of-Detail meshes (LOD 0, 1, 2...). When unchecked, only the highest detail LOD 0 is imported.</source>
        <translation>导入所有基于距离的细节层级网格（LOD 0、1、2……）。未勾选时仅导入细节最高的 LOD 0。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="210"/>
        <source>Export Weapon Anim Prefab (-write_weapon_anim_prefab)</source>
        <translation>导出武器动画 Prefab（-write_weapon_anim_prefab）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="212"/>
        <source>Writes weapon animation sequences and bone weightlists into a reusable prefab file, prefixing each entry with the weapon filename.</source>
        <translation>将武器动画序列和骨骼权重列表写入可复用的 prefab 文件，每个条目以武器文件名作为前缀。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="232"/>
        <source>START</source>
        <translation>开始</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ModelTab.qml" line="256"/>
        <source>STOP</source>
        <translation>停止</translation>
    </message>
</context>
<context>
    <name>PackArchive</name>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="60"/>
        <source>pack archive path is empty or invalid</source>
        <translation>打包归档路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="65"/>
        <source>pack archive file not found</source>
        <translation>未找到打包归档文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="75"/>
        <source>file is not a supported pack archive or failed to parse</source>
        <translation>文件不是受支持的打包归档，或解析失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="93"/>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="116"/>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="135"/>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="167"/>
        <source>pack archive is not open</source>
        <translation>打包归档未打开</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="123"/>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="147"/>
        <source>entry not found in pack archive</source>
        <translation>在打包归档中未找到该条目</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="140"/>
        <source>destination file path is empty or invalid</source>
        <translation>目标文件路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="155"/>
        <source>failed to extract entry to destination file</source>
        <translation>提取条目到目标文件失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="172"/>
        <source>destination directory path is empty or invalid</source>
        <translation>目标目录路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Package/PackArchive.cpp" line="179"/>
        <source>failed to extract one or more entries</source>
        <translation>提取一个或多个条目失败</translation>
    </message>
</context>
<context>
    <name>PackArchivePool</name>
    <message>
        <location filename="../src/Domain/Package/PackArchivePool.cpp" line="25"/>
        <source>pack archive path is empty or invalid</source>
        <translation>打包归档路径为空或无效</translation>
    </message>
</context>
<context>
    <name>ParticleImportService</name>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="77"/>
        <source>Another import operation is already in progress</source>
        <translation>已有导入操作正在进行</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="93"/>
        <source>Import Particle: %1</source>
        <translation>导入粒子：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="198"/>
        <source>Source PCF path cannot be empty</source>
        <translation>源 PCF 路径不能为空</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="203"/>
        <source>Source PCF file does not exist</source>
        <translation>源 PCF 文件不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="207"/>
        <source>Starting particle import for addon &apos;%1&apos; with PCF &apos;%2&apos;</source>
        <translation>开始为附加内容 '%1' 导入粒子（PCF：'%2'）</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="221"/>
        <source>Particle import workflow was cancelled</source>
        <translation>粒子导入工作流已取消</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="226"/>
        <source>Particle import workflow failed: %1</source>
        <translation>粒子导入工作流失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="231"/>
        <source>Particle import workflow was skipped: %1</source>
        <translation>粒子导入工作流已跳过：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="238"/>
        <source>Particle import workflow finished successfully: %1 converted, %2 compiled</source>
        <translation>粒子导入工作流成功完成：%1 个已转换，%2 个已编译</translation>
    </message>
    <message>
        <location filename="../src/Application/Particle/ParticleImportService.cpp" line="242"/>
        <source>Particle import failed</source>
        <translation>粒子导入失败</translation>
    </message>
</context>
<context>
    <name>ParticleImportWorkflow</name>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="29"/>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="38"/>
        <source>Failed to clean up generated artifact: %1 (%2)</source>
        <translation>清理生成的产物失败：%1（%2）</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="44"/>
        <source>Cleaned up %1 half-finished artifact(s) after cancelled/failed import</source>
        <translation>在导入取消/失败后清理了 %1 个未完成的产物</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="62"/>
        <source>Converting PCF with source1import</source>
        <translation>使用 source1import 转换 PCF</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="81"/>
        <source>PCF conversion failed: %1</source>
        <translation>PCF 转换失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="90"/>
        <source>No .vpcf files generated from PCF conversion</source>
        <translation>PCF 转换未生成 .vpcf 文件</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="110"/>
        <source>Compiling generated .vpcf resources</source>
        <translation>编译生成的 .vpcf 资源</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="126"/>
        <source>Resource compilation failed: %1</source>
        <translation>资源编译失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Particle/ParticleImportWorkflow.cpp" line="149"/>
        <source>Particle import and compilation completed successfully</source>
        <translation>粒子导入与编译成功完成</translation>
    </message>
</context>
<context>
    <name>ParticleTab</name>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="33"/>
        <source>Source 1 Game</source>
        <translation>Source 1 游戏</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="61"/>
        <source>Counter-Strike 2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="87"/>
        <source>SELECT PCF</source>
        <translation>选择 PCF</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="160"/>
        <source>OPTIONS</source>
        <translation>选项</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="182"/>
        <source>Allow Depth Blend (-particle_allow_depth_blend)</source>
        <translation>允许深度混合（-particle_allow_depth_blend）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="184"/>
        <source>Respects $DEPTHBLEND in particle materials to smoothly feather and blend smoke, fire, and fog edges with surrounding world geometry.</source>
        <translation>遵循粒子材质中的 $DEPTHBLEND，使烟雾、火焰和雾的边缘与周围世界几何体平滑羽化融合。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="191"/>
        <source>Disable Diffuse Lighting (-particle_disable_diffuse)</source>
        <translation>禁用漫反射光照（-particle_disable_diffuse）</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="193"/>
        <source>Prevents scene lighting from tinting or darkening particle sprites, preserving their intended self-luminous or vivid colors.</source>
        <translation>防止场景光照对粒子精灵染色或变暗，保留其自发光或鲜艳的原有色彩。</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="214"/>
        <source>START</source>
        <translation>开始</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/tabs/ParticleTab.qml" line="248"/>
        <source>STOP</source>
        <translation>停止</translation>
    </message>
</context>
<context>
    <name>ProcessRunner</name>
    <message>
        <location filename="../src/Core/Process/ProcessRunner.cpp" line="31"/>
        <source>Executable path is empty.</source>
        <translation>可执行文件路径为空。</translation>
    </message>
    <message>
        <location filename="../src/Core/Process/ProcessRunner.cpp" line="65"/>
        <source>Process startup timed out.</source>
        <translation>进程启动超时。</translation>
    </message>
    <message>
        <location filename="../src/Core/Process/ProcessRunner.cpp" line="121"/>
        <location filename="../src/Core/Process/ProcessRunner.cpp" line="163"/>
        <source>Process was cancelled by user.</source>
        <translation>进程已被用户取消。</translation>
    </message>
</context>
<context>
    <name>ResourceCompilerTool</name>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="46"/>
        <source>Resource compiler path is invalid</source>
        <translation>资源编译器路径无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="51"/>
        <source>Resource compiler does not exist</source>
        <translation>资源编译器不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="56"/>
        <source>CS2 game directory cannot be empty</source>
        <translation>CS2 游戏目录不能为空</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="61"/>
        <source>Resource list to compile cannot be empty</source>
        <translation>待编译资源列表不能为空</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="122"/>
        <source>resourcecompiler cancelled</source>
        <translation>resourcecompiler 已取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="125"/>
        <source>resourcecompiler was cancelled</source>
        <translation>resourcecompiler 已被取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="127"/>
        <source>Resource compiler was cancelled</source>
        <translation>资源编译器已取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="135"/>
        <source>resourcecompiler crashed: %1</source>
        <translation>resourcecompiler 崩溃：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="138"/>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="150"/>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="162"/>
        <source>resourcecompiler.exe</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="139"/>
        <source>Resource compiler crashed</source>
        <translation>资源编译器崩溃</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="144"/>
        <source>Timed out</source>
        <translation>已超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="147"/>
        <source>resourcecompiler timed out after %1 ms</source>
        <translation>resourcecompiler 在 %1 毫秒后超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="151"/>
        <source>Resource compiler timed out</source>
        <translation>资源编译器执行超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="159"/>
        <source>resourcecompiler failed to start: %1</source>
        <translation>resourcecompiler 启动失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="163"/>
        <source>Failed to start resource compiler</source>
        <translation>资源编译器启动失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="186"/>
        <source>Compiled VPCF_C: %1</source>
        <translation>已编译 VPCF_C：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="192"/>
        <source>resourcecompiler returned failure with exit code %1</source>
        <translation>resourcecompiler 返回失败，退出码 %1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="193"/>
        <source>; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="199"/>
        <source>Resource compilation failed</source>
        <translation>资源编译失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="204"/>
        <source>Compiled %1 asset(s)</source>
        <translation>已编译 %1 个资源</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ResourceCompilerTool.cpp" line="214"/>
        <source>Successfully compiled %1 resource(s)</source>
        <translation>成功编译 %1 个资源</translation>
    </message>
</context>
<context>
    <name>Result</name>
    <message>
        <location filename="../src/Core/Result/Result.h" line="110"/>
        <location filename="../src/Core/Result/Result.h" line="219"/>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <location filename="../src/Core/Result/Result.h" line="123"/>
        <location filename="../src/Core/Result/Result.h" line="231"/>
        <source>Operation skipped</source>
        <translation>操作已跳过</translation>
    </message>
</context>
<context>
    <name>SoundEventKv3Writer</name>
    <message>
        <location filename="../src/Domain/Audio/SoundEventKv3Writer.cpp" line="177"/>
        <source>Failed to write KV3 soundevents file</source>
        <translation>写入 KV3 soundevents 文件失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/SoundEventKv3Writer.cpp" line="181"/>
        <source>Unknown error writing KV3 soundevents file</source>
        <translation>写入 KV3 soundevents 文件时发生未知错误</translation>
    </message>
</context>
<context>
    <name>SoundscapeConvertService</name>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="25"/>
        <source>soundscape_</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="46"/>
        <source>Failed to parse soundscape content: %1</source>
        <translation>解析 soundscape 内容失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="70"/>
        <source>Failed to convert soundscape content: %1</source>
        <translation>转换 soundscape 内容失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="82"/>
        <source>Source soundscape file does not exist</source>
        <translation>源 soundscape 文件不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="90"/>
        <source>Failed to parse soundscape file: %1</source>
        <translation>解析 soundscape 文件失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="100"/>
        <source>Failed to write target .vsndevts file: %1</source>
        <translation>写入目标 .vsndevts 文件失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="123"/>
        <source>Failed to convert soundscape file: %1</source>
        <translation>转换 soundscape 文件失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="132"/>
        <source>Starting soundscape conversion for map &apos;%1&apos;</source>
        <translation>开始为地图 '%1' 转换 soundscape</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="142"/>
        <source>Scripts directory not found: %1</source>
        <translation>未找到脚本目录：%1</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="144"/>
        <source>No scripts directory found to convert</source>
        <translation>未找到可转换的脚本目录</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="154"/>
        <source>soundscapes.txt</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="155"/>
        <source>soundscapes.vsc</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="169"/>
        <source>No soundscape files found to convert for map &apos;%1&apos;</source>
        <translation>地图 '%1' 没有可转换的 soundscape 文件</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="171"/>
        <source>No soundscape files found</source>
        <translation>未找到 soundscape 文件</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="175"/>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="176"/>
        <source>soundevents</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="193"/>
        <source>Soundscape conversion cancelled by user</source>
        <translation>soundscape 转换已被用户取消</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="208"/>
        <source>Failed to convert %1: %2</source>
        <translation>转换 %1 失败：%2</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="232"/>
        <source>Soundscape conversion completed: %1 soundscapes converted, %2 soundevents generated, %3 files written, %4 unique raw sound assets referenced.</source>
        <translation>soundscape 转换完成：转换了 %1 个 soundscape，生成了 %2 个 soundevent，写入了 %3 个文件，引用了 %4 个唯一的原始声音资源。</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="241"/>
        <source>Soundscape conversion completed successfully</source>
        <translation>soundscape 转换成功完成</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="242"/>
        <source>Soundscape conversion failed for map &apos;%1&apos;</source>
        <translation>地图 '%1' 的 soundscape 转换失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="249"/>
        <source>All</source>
        <translation>全部</translation>
    </message>
    <message>
        <location filename="../src/Application/Soundscape/SoundscapeConvertService.cpp" line="250"/>
        <source>Convert Soundscapes: %1</source>
        <translation>转换 Soundscape：%1</translation>
    </message>
</context>
<context>
    <name>SoundscapeParser</name>
    <message>
        <location filename="../src/Domain/Audio/SoundscapeParser.cpp" line="29"/>
        <source>Failed to parse Soundscape VDF content</source>
        <translation>解析 Soundscape VDF 内容失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Audio/SoundscapeParser.cpp" line="39"/>
        <source>Failed to load Soundscape file</source>
        <translation>加载 Soundscape 文件失败</translation>
    </message>
</context>
<context>
    <name>Source1ImportTool</name>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="42"/>
        <source>Source 1 import tool path is invalid</source>
        <translation>Source 1 导入工具路径无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="47"/>
        <source>Source 1 import tool does not exist</source>
        <translation>Source 1 导入工具不存在</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="111"/>
        <source>source1import cancelled</source>
        <translation>source1import 已取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="114"/>
        <source>source1import was cancelled</source>
        <translation>source1import 已被取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="116"/>
        <source>Source 1 import tool was cancelled</source>
        <translation>Source 1 导入工具已取消</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="124"/>
        <source>source1import crashed: %1</source>
        <translation>source1import 崩溃：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="127"/>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="139"/>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="151"/>
        <source>source1import.exe</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="128"/>
        <source>Source 1 import tool crashed</source>
        <translation>Source 1 导入工具崩溃</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="133"/>
        <source>Timed out</source>
        <translation>已超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="136"/>
        <source>source1import timed out after %1 ms</source>
        <translation>source1import 在 %1 毫秒后超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="140"/>
        <source>Source 1 import tool timed out</source>
        <translation>Source 1 导入工具执行超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="148"/>
        <source>source1import failed to start: %1</source>
        <translation>source1import 启动失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="152"/>
        <source>Failed to start Source 1 import tool</source>
        <translation>Source 1 导入工具启动失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="180"/>
        <source>No matching files found</source>
        <translation>未找到匹配的文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="184"/>
        <source>No files found matching the specification</source>
        <translation>未找到与规格匹配的文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="190"/>
        <source>source1import returned failure with exit code %1</source>
        <translation>source1import 返回失败，退出码 %1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="191"/>
        <source>; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="196"/>
        <source>source1import failed: %1</source>
        <translation>source1import 失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="200"/>
        <source>Resource import failed: %1</source>
        <translation>资源导入失败：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="205"/>
        <source>Converted %1 asset(s)</source>
        <translation>已转换 %1 个资产</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/Source1ImportTool.cpp" line="215"/>
        <source>Successfully imported %1 asset(s)</source>
        <translation>成功导入 %1 个资产</translation>
    </message>
</context>
<context>
    <name>SteamLibraryDetector</name>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="85"/>
        <source>No Steam installation found on host system</source>
        <translation>主机系统上未找到 Steam 安装</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="101"/>
        <source>Specified Steam directory does not exist or is not a directory</source>
        <translation>指定的 Steam 目录不存在或不是目录</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="110"/>
        <source>No Steam installation found on system</source>
        <translation>系统上未找到 Steam 安装</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="115"/>
        <source>steamapps/libraryfolders.vdf</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="127"/>
        <source>Failed to parse Steam library configuration</source>
        <translation>解析 Steam 库配置失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="153"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="246"/>
        <source>steamapps</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="194"/>
        <source>Failed to parse libraryfolders.vdf</source>
        <translation>解析 libraryfolders.vdf 失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="274"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="318"/>
        <source>Invalid Steam library path</source>
        <translation>Steam 库路径无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="280"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="324"/>
        <source>Invalid Steam AppID</source>
        <translation>Steam AppID 无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="284"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="328"/>
        <source>steamapps/appmanifest_%1.acf</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="289"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="333"/>
        <source>App manifest file not found</source>
        <translation>未找到应用清单文件</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="298"/>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="342"/>
        <source>Failed to parse app manifest</source>
        <translation>解析应用清单失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="308"/>
        <source>Field &apos;installdir&apos; not found in app manifest</source>
        <translation>应用清单中未找到 'installdir' 字段</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/Internal/SteamLibraryDetector.cpp" line="352"/>
        <source>Field &apos;name&apos; not found in app manifest</source>
        <translation>应用清单中未找到 'name' 字段</translation>
    </message>
</context>
<context>
    <name>SteamService</name>
    <message>
        <location filename="../src/Application/Environment/SteamService.cpp" line="17"/>
        <location filename="../src/Application/Environment/SteamService.cpp" line="37"/>
        <source>Steam validation failed</source>
        <translation>Steam 校验失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/SteamService.cpp" line="33"/>
        <source>No primary AppID registered for game type</source>
        <translation>该游戏类型未注册主 AppID</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/SteamService.cpp" line="50"/>
        <source>Invalid Steam AppID</source>
        <translation>Steam AppID 无效</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/SteamService.cpp" line="64"/>
        <source>Failed to open Steam validation URL</source>
        <translation>打开 Steam 校验 URL 失败</translation>
    </message>
</context>
<context>
    <name>ToolErrors</name>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="52"/>
        <source>Executable not found: %1</source>
        <translation>未找到可执行文件：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="63"/>
        <source>%1 execution failed with exit code %2</source>
        <translation>%1 执行失败，退出码 %2</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="73"/>
        <source>%1 execution timed out</source>
        <translation>%1 执行超时</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="83"/>
        <source>%1 crashed during execution</source>
        <translation>%1 在执行期间崩溃</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="93"/>
        <source>Found no files matching specification: %1</source>
        <translation>未找到符合规格的文件：%1</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="99"/>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="103"/>
        <source>Source1Import failed</source>
        <translation>Source1Import 失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="109"/>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="113"/>
        <source>ResourceCompiler failed</source>
        <translation>ResourceCompiler 失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="119"/>
        <location filename="../src/Domain/Tool/ToolErrors.h" line="123"/>
        <source>Failed to parse tool output</source>
        <translation>解析工具输出失败</translation>
    </message>
</context>
<context>
    <name>ToolLogWindow</name>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="45"/>
        <source>External Tool Log - %1</source>
        <translation>外部工具日志 - %1</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="64"/>
        <source>Open log file</source>
        <translation>打开日志文件</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="74"/>
        <source>Copy all</source>
        <translation>全部复制</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="86"/>
        <source>Auto-scroll</source>
        <translation>自动滚动</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="128"/>
        <source>Lines: %1</source>
        <translation>行数：%1</translation>
    </message>
    <message>
        <location filename="../src/qml/cs2importer/ToolLogWindow.qml" line="150"/>
        <source>CMD:</source>
        <translation>命令：</translation>
    </message>
</context>
<context>
    <name>VpkSignatureLeaseService</name>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="39"/>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="46"/>
        <source>Failed to update VPK signature lease</source>
        <translation>更新 VPK 签名租约失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="53"/>
        <source>Failed to acquire VPK signature lease</source>
        <translation>获取 VPK 签名租约失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="65"/>
        <source>VPK signature lease retry failed</source>
        <translation>重试 VPK 签名租约失败</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="101"/>
        <source>No active CS2 installation to retry leasing</source>
        <translation>没有可重试租约的有效 CS2 安装</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="111"/>
        <source>CS2 base directory is invalid or does not exist</source>
        <translation>CS2 根目录无效或不存在</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="131"/>
        <source>vpk.signatures does not exist at expected path</source>
        <translation>vpk.signatures 不在预期路径上</translation>
    </message>
    <message>
        <location filename="../src/Application/Environment/VpkSignatureLeaseService.cpp" line="146"/>
        <source>Target vpk.signatures is not a regular file</source>
        <translation>目标 vpk.signatures 不是常规文件</translation>
    </message>
</context>
<context>
    <name>VtfConverter</name>
    <message>
        <location filename="../src/Domain/Material/VtfConverter.cpp" line="41"/>
        <source>VTF file path is empty or invalid</source>
        <translation>VTF 文件路径为空或无效</translation>
    </message>
    <message>
        <location filename="../src/Domain/Material/VtfConverter.cpp" line="46"/>
        <source>VTF file not found</source>
        <translation>未找到 VTF 文件</translation>
    </message>
    <message>
        <location filename="../src/Domain/Material/VtfConverter.cpp" line="104"/>
        <source>failed to decode VTF image or encode to target format</source>
        <translation>解码 VTF 图像或编码为目标格式失败</translation>
    </message>
    <message>
        <location filename="../src/Domain/Material/VtfConverter.cpp" line="118"/>
        <source>destination image path is empty or invalid</source>
        <translation>目标图像路径为空或无效</translation>
    </message>
</context>
<context>
    <name>VtfExtractor</name>
    <message>
        <location filename="../src/Workflow/Common/VtfExtractor.cpp" line="42"/>
        <source>relative VTF path is empty</source>
        <translation>VTF 相对路径为空</translation>
    </message>
    <message>
        <location filename="../src/Workflow/Common/VtfExtractor.cpp" line="47"/>
        <source>destination image directory is empty or invalid</source>
        <translation>目标图像目录为空或无效</translation>
    </message>
</context>
</TS>
