#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <vector>

#include "Core/Logging/FileSink.h"
#include "Core/Logging/LogManager.h"

using namespace Core::Logging;

namespace {

struct RecordedLog {
    QString message;
    bool accepted = false;
};

[[noreturn]] void fail(const QString& message)
{
    std::cerr << message.toStdString() << std::endl;
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const QString& message)
{
    if (!condition) {
        fail(message);
    }
}

void verifyTaskLogs(const std::shared_ptr<TaskLoggingContext>& task,
                    const std::vector<RecordedLog>& records)
{
    const auto blocks = task->allBlocks();
    std::vector<LogEntry> entries;
    for (const auto& block : blocks) {
        require(block.taskId() == task->taskId(), "block crossed task boundary");
        for (const auto& entry : block.entries()) {
            require(entry.taskId == task->taskId(), "entry crossed task boundary");
            entries.push_back(entry);
        }
    }

    std::vector<QString> acceptedMessages;
    for (const auto& record : records) {
        if (record.accepted) {
            acceptedMessages.push_back(record.message);
        }
    }
    require(static_cast<qsizetype>(entries.size()) == static_cast<qsizetype>(acceptedMessages.size()),
            QString("entry count mismatch for task %1").arg(task->taskId()));

    std::sort(entries.begin(), entries.end(), [](const LogEntry& left, const LogEntry& right) {
        return left.sequence < right.sequence;
    });
    for (qsizetype i = 0; i < static_cast<qsizetype>(entries.size()); ++i) {
        require(entries[i].sequence == static_cast<quint64>(i + 1), "task sequence has a gap or duplicate");
        require(entries[i].message == acceptedMessages[static_cast<size_t>(i)],
                QString("message corruption in task %1").arg(task->taskId()));
    }
}

void runLoggingStress()
{
    auto& manager = LogManager::instance();
    manager.clear();
    manager.setDefaultBlockSizeThreshold(128);

    constexpr int taskCount = 12;
    constexpr int messagesPerTask = 350;
    std::vector<std::shared_ptr<TaskLoggingContext>> tasks;
    std::vector<std::vector<RecordedLog>> records(taskCount);
    tasks.reserve(taskCount);

    for (int taskIndex = 0; taskIndex < taskCount; ++taskIndex) {
        auto task = manager.createTask(QString("StressTask_%1").arg(taskIndex));
        require(task != nullptr, "failed to create stress task");
        task->setBlockSizeThreshold(128);
        tasks.push_back(task);
        records[taskIndex].reserve(messagesPerTask);
    }

    std::vector<std::unique_ptr<QThread>> workers;
    for (int taskIndex = 0; taskIndex < taskCount; ++taskIndex) {
        workers.emplace_back(QThread::create([&, taskIndex]() {
            std::mt19937 random(0xC0FFEEu + static_cast<unsigned>(taskIndex));
            std::uniform_int_distribution<int> payloadLength(1, 180);
            std::uniform_int_distribution<int> level(0, 3);
            std::uniform_int_distribution<int> burst(1, 7);
            for (int messageIndex = 0; messageIndex < messagesPerTask; ++messageIndex) {
                if (messageIndex % burst(random) == 0) {
                    QThread::usleep(static_cast<unsigned long>(random() % 300));
                }
                const int length = payloadLength(random);
                QString payload(length, QChar('a' + (messageIndex % 26)));
                const QString message = QString("task=%1 message=%2 payload=%3")
                    .arg(tasks[taskIndex]->taskId()).arg(messageIndex).arg(payload);
                bool accepted = false;
                switch (level(random)) {
                case 0: accepted = tasks[taskIndex]->debug(message); break;
                case 1: accepted = tasks[taskIndex]->info(message); break;
                case 2: accepted = tasks[taskIndex]->warning(message); break;
                default: accepted = tasks[taskIndex]->error(message); break;
                }
                records[taskIndex].push_back({message, accepted});
            }
        }));
    }
    for (auto& worker : workers) worker->start();
    for (auto& worker : workers) {
        worker->wait();
    }

    bool multipleBlocks = false;
    for (int i = 0; i < taskCount; ++i) {
        require(tasks[i]->sealedBlockCount() > 1, "stress task did not produce multiple blocks");
        multipleBlocks = multipleBlocks || tasks[i]->sealedBlockCount() > 1;
        verifyTaskLogs(tasks[i], records[i]);
    }
    require(multipleBlocks, "stress test did not exercise block flush");
}

void runFaultStress()
{
    auto& manager = LogManager::instance();
    manager.clear();
    manager.setDefaultBlockSizeThreshold(96);

    QTemporaryDir tempDir;
    require(tempDir.isValid(), "failed to create fault stress temporary directory");
    auto sink = std::make_shared<FileSink>(tempDir.filePath("fault-stress.log"));
    require(sink->isOpen(), "failed to open fault stress sink");
    manager.addSink(sink);

    constexpr int taskCount = 8;
    constexpr int messagesPerTask = 250;
    std::vector<std::shared_ptr<TaskLoggingContext>> tasks;
    std::vector<std::vector<RecordedLog>> records(taskCount);
    for (int i = 0; i < taskCount; ++i) {
        auto task = manager.createTask(QString("FaultTask_%1").arg(i));
        require(task != nullptr, "failed to create fault task");
        task->setBlockSizeThreshold(96);
        tasks.push_back(task);
        records[i].reserve(messagesPerTask);
    }

    std::atomic<bool> startFault{false};
    std::atomic<int> acceptedFaults{0};
    std::mutex faultMessageMutex;
    QString acceptedFaultMessage;
    std::vector<std::unique_ptr<QThread>> workers;
    for (int taskIndex = 0; taskIndex < taskCount; ++taskIndex) {
        workers.emplace_back(QThread::create([&, taskIndex]() {
            std::mt19937 random(0xBADC0DEu + static_cast<unsigned>(taskIndex));
            for (int messageIndex = 0; messageIndex < messagesPerTask; ++messageIndex) {
                if (taskIndex == 0 && messageIndex == 25) {
                    startFault.store(true, std::memory_order_release);
                }
                while (taskIndex != 0 && !startFault.load(std::memory_order_acquire)) {
                    QThread::yieldCurrentThread();
                }
                const QString message = QString("normal task=%1 message=%2 payload=%3")
                    .arg(tasks[taskIndex]->taskId()).arg(messageIndex)
                    .arg(QString(static_cast<int>(random() % 80) + 1, QChar('x')));
                const bool accepted = tasks[taskIndex]->info(message);
                records[taskIndex].push_back({message, accepted});
            }
        }));
    }

    std::vector<std::unique_ptr<QThread>> faultReporters;
    for (int i = 0; i < 16; ++i) {
        faultReporters.emplace_back(QThread::create([&, i]() {
            while (!startFault.load(std::memory_order_acquire)) {
                QThread::yieldCurrentThread();
            }
            const auto result = tasks[0]->reportFault(QString("FAULT reporter=%1").arg(i));
            if (result.accepted()) {
                acceptedFaults.fetch_add(1, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(faultMessageMutex);
                acceptedFaultMessage = QString("FAULT reporter=%1").arg(i);
            }
        }));
    }
    for (auto& worker : workers) worker->start();
    for (auto& reporter : faultReporters) reporter->start();
    for (auto& reporter : faultReporters) reporter->wait();
    for (auto& worker : workers) worker->wait();

    const auto barrier = manager.faultBarrier();
    require(acceptedFaults.load() == 1, "more than one fault transition was accepted");
    const FaultContext context = barrier->faultContext();
    require(context.boundary > 0 && context.submissionSequence > context.boundary,
            "invalid fault boundary");

    // The task may have been writing when the fault won the race. Inspect the accepted fault
    // entry directly, then drain all blocks through the manager's normal output path.
    manager.beginFaultDraining();
    manager.terminateAfterFault();
    manager.flushAll();

    int faultEntries = 0;
    for (const auto& task : tasks) {
        for (const auto& block : task->allBlocks()) {
            for (const auto& entry : block.entries()) {
                const bool acceptedBeforeFault = entry.submissionSequence <= context.boundary;
                const bool acceptedFault = entry.submissionSequence == context.submissionSequence;
                require(acceptedBeforeFault || acceptedFault,
                        "ordinary log appeared after fault boundary");
                if (acceptedFault) {
                    ++faultEntries;
                    require(entry.taskId == context.taskId && entry.level == LogLevel::Critical,
                            "fault entry has incorrect context");
                }
            }
        }
    }
    require(faultEntries == 1, "fault entry missing or duplicated");
    require(!acceptedFaultMessage.isEmpty(), "accepted fault identity was not recorded");

    bool foundFaultMessage = false;
    for (const auto& task : tasks) {
        for (const auto& block : task->allBlocks()) {
            for (const auto& entry : block.entries()) {
                if (entry.submissionSequence == context.submissionSequence) {
                    foundFaultMessage = entry.message == acceptedFaultMessage;
                }
            }
        }
    }
    require(foundFaultMessage, "fault message identity was not preserved");
    sink->close();
    QFile output(tempDir.filePath("fault-stress.log"));
    require(output.open(QIODevice::ReadOnly | QIODevice::Text), "failed to read fault stress output");
    const QString outputText = QString::fromUtf8(output.readAll());

    std::set<QString> emittedMessages;
    for (const auto& task : tasks) {
        for (const auto& block : task->allBlocks()) {
            for (const auto& entry : block.entries()) {
                emittedMessages.insert(entry.message);
            }
        }
    }
    for (const auto& task : tasks) {
        for (const auto& block : task->allBlocks()) {
            for (const auto& entry : block.entries()) {
                require(outputText.contains(entry.message), "accepted entry missing from fault output");
            }
        }
    }
    for (const auto& taskRecords : records) {
        for (const auto& record : taskRecords) {
            if (!record.accepted) {
                require(!emittedMessages.count(record.message), "rejected entry entered final output");
                require(!outputText.contains(record.message), "rejected entry was written to the sink");
            }
        }
    }

    for (int i = 0; i < taskCount; ++i) {
        if (i != 0) {
            verifyTaskLogs(tasks[i], records[i]);
        } else {
            qsizetype expectedEntries = 0;
            for (const auto& record : records[i]) {
                if (record.accepted) {
                    ++expectedEntries;
                }
            }
            require(tasks[i]->allBlocks().size() > 1, "fault task did not produce multiple blocks");
            qsizetype actualEntries = 0;
            for (const auto& block : tasks[i]->allBlocks()) {
                actualEntries += block.entryCount();
            }
            require(actualEntries == expectedEntries + 1, "fault task entry count mismatch");
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    runLoggingStress();
    runFaultStress();
    std::cout << "logging stress tests passed" << std::endl;
    return EXIT_SUCCESS;
}
