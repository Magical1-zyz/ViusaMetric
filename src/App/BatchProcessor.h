#pragma once
#include "Config.h"
#include "Application.h"

// 专门负责批量任务的调度、文件匹配和结果表初始化
class BatchProcessor {
public:
    BatchProcessor(const AppConfig& config, Application& app);

    // 执行批量处理的主入口
    void RunBatch();

private:
    const AppConfig& config;
    Application& app;

    // 辅助：初始化三个 CSV 表格
    void InitReportTables();

    // 辅助：初始化单个 CSV
    void InitSingleCSV(const std::filesystem::path& path);
};