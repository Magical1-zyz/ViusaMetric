#include "BatchProcessor.h"
#include "Utils/FileSystemUtils.h"


namespace fs = std::filesystem;

BatchProcessor::BatchProcessor(const AppConfig& cfg, Application& application)
        : config(cfg), app(application) {}

void BatchProcessor::InitSingleCSV(const fs::path& path) {
    std::ofstream f(path);
    if (f.is_open()) {
        f << "ModelName,ViewIndex,ErrorValue\n";
        f.close();
    }
}

void BatchProcessor::InitReportTables() {
    fs::path outRoot = config.paths.outputRoot;
    if (!fs::exists(outRoot)) fs::create_directories(outRoot);

    InitSingleCSV(outRoot / "metrics_psnr.csv");
    InitSingleCSV(outRoot / "metrics_silhouette.csv");
    InitSingleCSV(outRoot / "metrics_normal.csv");

    std::cout << "[Batch] Report tables initialized." << std::endl;
}

void BatchProcessor::RunBatch() {
    // 1. 准备环境
    InitReportTables();

    fs::path refRoot = fs::path(config.paths.assetsRoot) / config.paths.refDir;
    fs::path optRoot = fs::path(config.paths.assetsRoot) / config.paths.optDir;

    std::cout << "==================================================" << std::endl;
    std::cout << "[BatchProcessor] Start Processing..." << std::endl;
    std::cout << "  Ref Dir: " << refRoot << std::endl;
    std::cout << "==================================================\n" << std::endl;

    if (!fs::exists(refRoot) || !fs::is_directory(refRoot)) {
        std::cerr << "[Error] Ref directory not found!" << std::endl;
        return;
    }

    // 2. 遍历逻辑
    for (const auto& entry : fs::directory_iterator(refRoot)) {
        if (entry.is_directory()) {
            std::string modelName = entry.path().filename().string();

            // 使用 Utils 中的查找逻辑
            fs::path refFile = Utils::FindFirstModelFile(entry.path());

            fs::path optDirForModel = optRoot / modelName;
            fs::path optFile = Utils::FindFirstModelFile(optDirForModel);

            // 3. 调度 Application
            if (!refFile.empty() && !optFile.empty()) {
                std::cout << ">>> Processing: " << modelName << std::endl;
                app.ProcessSingleModel(refFile.string(), optFile.string(), modelName);
                std::cout << ">>> Done: " << modelName << "\n" << std::endl;
            } else {
                std::cout << "[Skip] " << modelName << " - Incomplete files." << std::endl;
            }
        }
    }

    std::cout << "[BatchProcessor] All tasks finished." << std::endl;
}