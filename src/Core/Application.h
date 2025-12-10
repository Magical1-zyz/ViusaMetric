#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <string>

#include "PBRRenderer.h"
#include "MetricVisualizer.h"
#include "Model.h"
#include "CameraSampler.h" // [新增] 引入采样器

class Application {
public:
    Application(int width, int height, const char* title);
    ~Application();

    bool Init();
    void Run();

private:
    GLFWwindow* window;
    int scrWidth, scrHeight;
    const char* appTitle;

    std::unique_ptr<Core::PBRRenderer> renderer;
    std::unique_ptr<Core::MetricVisualizer> visualizer;
    std::unique_ptr<Core::Model> refModel;
    std::unique_ptr<Core::Model> optModel;

    unsigned int texRef = 0;
    unsigned int texOpt = 0;
    int renderW = 1024;
    int renderH = 1024;

    // [修改] 使用 CameraSampler 生成的 Sample 结构
    std::vector<Core::CameraSample> views;
    int currentViewIdx = 0;
    float lastTime = 0.0f;

    void InitIntermediateTextures();
    // GenerateFibonacciViews 函数被移除，逻辑移至 CameraSampler

    // [修改] 参数类型变更为 CameraSample
    void RenderPasses(const Core::CameraSample& cam);

    bool SetupWorkingDirectory();
    std::string FindFirstModel(const std::string& folder);
    std::string FindFirstHDR(const std::string& folder);
};