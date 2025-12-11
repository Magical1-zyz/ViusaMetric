#pragma once
#include "Scene/Scene.h"

// 前置声明
namespace Renderer { class PBRRenderer; }
namespace Metrics { class MetricVisualizer; }
namespace Scene { class Model; struct CameraSample; }
struct GLFWwindow;

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Init();
    void Run();

private:
    enum class RenderPhase {
        PHASE_IBL_PSNR = 0,
        PHASE_SILHOUETTE = 1,
        PHASE_NORMAL = 2,
        FINISHED = 3
    };

    // --- 窗口与系统 ---
    GLFWwindow* window = nullptr;
    int scrWidth, scrHeight;
    const char* appTitle;
    Scene::Scene scene;

    // --- 子模块 ---
    std::unique_ptr<Renderer::PBRRenderer> renderer;
    std::unique_ptr<Metrics::MetricVisualizer> visualizer;

    // --- 渲染资源 ---
    struct RenderTargets {
        unsigned int texRef = 0;
        unsigned int texOpt = 0;
        unsigned int texHeatmap = 0;
        int width = 1024;
        int height = 1024;
        void Init(int w, int h);
        void Cleanup();
    } targets;

    // --- 逻辑状态 ---
    std::vector<Scene::CameraSample> views;
    int currentViewIdx = 0;
    float lastTime = 0.0f;

    RenderPhase currentPhase = RenderPhase::PHASE_IBL_PSNR;
    double accumulatorError = 0.0;      // 累加误差 (用于计算平均值)
    double currentViewError = 0.0;      // [新增] 当前视角误差 (用于写入 CSV)
    int lastSavedView = -1;             // [新增] 防止同一视角重复保存

    // --- 文件输出相关 [新增] ---
    std::ofstream csvFile;              // CSV 文件流
    std::string currentOutputDir;       // 当前阶段的输出目录 (如 output/psnr)

    void EnsureDirectories();                                    // 创建文件夹
    void InitPhaseOutput(const std::string& phaseName);          // 初始化阶段输出(打开CSV等)
    void SaveScreenshot(int viewIdx);                            // 保存截图
    void LogToCSV(int viewIdx, double error);                    // 写入数据
    void UpdateHeatmapTexture(const std::vector<unsigned char>& data);

    // --- 内部流程 ---
    void ProcessInput();
    void RenderPasses();
    void UpdateState();
};