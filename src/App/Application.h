#pragma once
#include "Scene/Scene.h"

namespace Renderer {
    class PBRRenderer;
}

namespace Metrics{
    class MetricVisualizer;
}

namespace Scene{
    class Model;
    struct CameraSample;
}

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    // 禁止拷贝复制，App 实例应该是唯一的
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Init();
    void Run();

private:
    // --- 窗口与系统 ---
    GLFWwindow* window = nullptr;
    int scrWidth, scrHeight;
    const char* appTitle;
    Scene::Scene scene;

    // --- 子模块 (使用智能指针管理生命周期) ---
    std::unique_ptr<Renderer::PBRRenderer> renderer;
    std::unique_ptr<Metrics::MetricVisualizer> visualizer;

    // --- 场景数据 (可以进一步封装成 Scene 类) ---
    std::unique_ptr<Scene::Model> refModel;
    std::unique_ptr<Scene::Model> optModel;

    // --- 渲染资源 ---
    struct RenderTargets {
        unsigned int texRef = 0;
        unsigned int texOpt = 0;
        int width = 1024;
        int height = 1024;
        void Init(int w, int h); // 将纹理创建逻辑移入这里
        void Cleanup();
    } targets;

    // --- 逻辑状态 ---
    std::vector<Scene::CameraSample> views;
    int currentViewIdx = 0;
    float lastTime = 0.0f;

    // --- 内部流程 ---
    void ProcessInput();
    void RenderPasses(); // 不再需要传参，使用成员变量 views[currentViewIdx]
    void UpdateState();  // 处理时间、视图切换逻辑
};