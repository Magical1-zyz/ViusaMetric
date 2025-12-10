#define _CRT_SECURE_NO_WARNINGS
#include "Application.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

Application::Application(int width, int height, const char* title)
        : scrWidth(width), scrHeight(height), appTitle(title), window(nullptr) {}

Application::~Application() {
    if (texRef) glDeleteTextures(1, &texRef);
    if (texOpt) glDeleteTextures(1, &texOpt);
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Application::Init() {
    if (!SetupWorkingDirectory()) return false;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(scrWidth, scrHeight, appTitle, NULL, NULL);
    if (!window) return false;
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

    std::string hdrPath = FindFirstHDR("assets/hdrtextures");
    if(hdrPath.empty()) hdrPath = FindFirstHDR("assets/hdrtexture");
    if (hdrPath.empty()) { std::cerr << "[Error] No HDR found!" << std::endl; return false; }

    std::string refPath = FindFirstModel("assets/refmodel");
    std::string optPath = FindFirstModel("assets/optmodel");
    if (optPath.empty()) optPath = refPath;
    if (refPath.empty()) { std::cerr << "[Error] No Models found!" << std::endl; return false; }

    renderer = std::make_unique<Core::PBRRenderer>(renderW, renderH);
    renderer->InitIBL(hdrPath);

    refModel = std::make_unique<Core::Model>(refPath);
    optModel = std::make_unique<Core::Model>(optPath);

    visualizer = std::make_unique<Core::MetricVisualizer>(renderW, renderH);

    InitIntermediateTextures();

    // [核心修改] 使用 CameraSampler 生成视角
    // 64个点，半径 2.0，无抖动
    float aspect = (float)renderW / (float)renderH;
    views = Core::CameraSampler::GenerateSamples(64, 2.0f, aspect, 0.0f);

    std::cout << "[System] Generated " << views.size() << " Fibonacci samples at Radius 2.0" << std::endl;

    return true;
}

void Application::InitIntermediateTextures() {
    auto createTex = [&](unsigned int& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderW, renderH, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    createTex(texRef);
    createTex(texOpt);
}

// [修改] 参数类型变更为 CameraSample，使用其中的 viewMatrix 和 projMatrix
void Application::RenderPasses(const Core::CameraSample& cam) {
    // --- Pass 1: RefModel (Lit) ---
    // 直接使用 sample 中计算好的 View 和 Proj
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderModel(*refModel, true);
    renderer->RenderSkybox();
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, texRef);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, renderW, renderH);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // --- Pass 2: OptModel (Unlit) ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderModel(*optModel, false);
    renderer->RenderSkybox();
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, texOpt);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, renderW, renderH);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // --- Pass 3: Composition ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scrWidth, scrHeight);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int panelW = scrWidth / 3;
    glViewport(0, 0, panelW, scrHeight);
    visualizer->RenderComposite(texRef, texOpt, 0);

    glViewport(panelW, 0, panelW, scrHeight);
    visualizer->RenderComposite(texRef, texOpt, 1);

    glViewport(panelW * 2, 0, panelW, scrHeight);
    visualizer->RenderComposite(texRef, texOpt, 2);
}

void Application::Run() {
    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        if (currentTime - lastTime > 1.0f) {
            currentViewIdx = (currentViewIdx + 1) % views.size();
            lastTime = currentTime;
            std::cout << "View [" << currentViewIdx << "/" << views.size() << "]" << std::endl;
        }

        if (!views.empty()) {
            RenderPasses(views[currentViewIdx]);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

// 辅助函数保持不变...
bool Application::SetupWorkingDirectory() {
    fs::path current = fs::current_path();
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(current / "assets/shaders/pbr/pbr.vert")) {
            fs::current_path(current); return true;
        }
        if (current.has_parent_path()) current = current.parent_path();
        else break;
    }
    return false;
}
std::string Application::FindFirstModel(const std::string& folder) {
    if (!fs::exists(folder)) return "";
    for (const auto& entry : fs::recursive_directory_iterator(folder)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj") return entry.path().string();
    }
    return "";
}
std::string Application::FindFirstHDR(const std::string& folder) {
    if (!fs::exists(folder)) return "";
    for (const auto& entry : fs::directory_iterator(folder)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".hdr") return entry.path().string();
    }
    return "";
}