#pragma once
#include "App/Config.h"
#include "Scene/Scene.h"

// 前置声明
namespace Renderer { class PBRRenderer; }
namespace Metrics { class MetricVisualizer; }
namespace Scene { class Model; struct CameraSample; }
struct GLFWwindow;

class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    // 禁止拷贝
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // 只初始化窗口和 OpenGL 上下文，不加载模型
    bool InitSystem();
    // 处理单个模型的全流程 (加载 -> 渲染循环 -> 保存 -> 卸载)
    void ProcessSingleModel(const std::string & refPath, const std::string &optPath, const std::string& modelName);

private:
    enum class RenderPhase {
        PHASE_IBL_PSNR = 0,
        PHASE_SILHOUETTE = 1,
        PHASE_NORMAL = 2,
        FINISHED = 3
    };

    // --- 配置与状态 ---
    AppConfig config;
    std::string currentModelName;   // 当前处理的模型名
    std::string currentOutputDir;   // 当前输出目录

    // --- 窗口与系统 ---
    GLFWwindow* window = nullptr;
    Scene::Scene scene;

    // --- 子模块 ---
    std::unique_ptr<Renderer::PBRRenderer> renderer;
    std::unique_ptr<Metrics::MetricVisualizer> visualizer;

    // --- 渲染资源 ---
    struct RenderTargets {
        unsigned int texRef = 0;
        unsigned int texOpt = 0;
        unsigned int texHeatmap = 0;
        int width = 0;
        int height = 0;
        void Init(int w, int h);
        void Cleanup();
    } targets;

    // --- 逻辑状态 ---
    std::vector<Scene::CameraSample> views;
    int currentViewIdx = 0;
    float lastTime = 0.0f;

    RenderPhase currentPhase = RenderPhase::PHASE_IBL_PSNR;
    double accumulatorError = 0.0;      // 累加误差 (用于计算平均值)
    double currentViewError = 0.0;      // 当前视角误差 (用于写入 CSV)
    int lastSavedView = -1;             // 防止同一视角重复保存

    // --- 辅助函数 ---
    void SetupOutputDirectories(const std::string& modelName);
    void AppendToGlobalCSV(const std::string& metricType, int viewIdx, double error);
    void SaveScreenshot(int viewIdx);

    // --- 纹理读取 ---
    std::vector<float> ReadTextureFloat(unsigned int texID, int w, int h);
    std::vector<unsigned char> ReadTextureByte(unsigned int texID, int w, int h);
    std::vector<float> ReadTextureDepth(unsigned int texID, int w, int h);
    void UploadGrayscaleToTexture(unsigned int texID, const std::vector<unsigned char>& data, int w, int h);
    void UpdateHeatmapTexture(const std::vector<unsigned char>& data);

    // --- 渲染流程 ---
    void ProcessInput();
    void UpdateState();  // 状态机流转 (PSNR->Sil->Normal->Finished)
    void RenderPasses(); // 渲染、计算误差、更新热力图
};