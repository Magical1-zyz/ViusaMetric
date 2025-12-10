#include "Application.h"
#include "Utils/FileSystemUtils.h"
#include "Resources/ResourceManager.h"
#include "Renderer/IBLBaker.h"
#include "Renderer/PBRRenderer.h"
#include "Metrics/MetricVisualizer.h"
#include "Scene/CameraSampler.h"


// =========================================================
// RenderTargets (内部结构体) 实现
// =========================================================
void Application::RenderTargets::Init(int w, int h) {
    width = w;
    height = h;

    auto createTex = [&](unsigned int& tex) {
        if (tex) glDeleteTextures(1, &tex); // 防止内存泄漏
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    createTex(texRef);
    createTex(texOpt);
}

void Application::RenderTargets::Cleanup() {
    if (texRef) glDeleteTextures(1, &texRef);
    if (texOpt) glDeleteTextures(1, &texOpt);
    texRef = 0;
    texOpt = 0;
}

// =========================================================
// Application 主类实现
// =========================================================

Application::Application(int width, int height, const char* title)
        : scrWidth(width), scrHeight(height), appTitle(title), window(nullptr)
{
}

Application::~Application() {
    targets.Cleanup();
    scene.Cleanup(); // 清理场景资源 (IBL 纹理等)

    // Model 由 ResourceManager 的 shared_ptr 自动管理，无需手动 delete
    // Unique_ptr (renderer, visualizer) 也会自动析构

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Application::Init() {
    // 1. 设置工作目录 (使用 Utils)
    if (!Utils::SetupWorkingDirectory()) {
        std::cerr << "[Error] Failed to setup working directory!" << std::endl;
        return false;
    }

    // 2. 初始化 GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(scrWidth, scrHeight, appTitle, NULL, NULL);
    if (!window) {
        std::cerr << "[Error] Failed to create GLFW window" << std::endl;
        return false;
    }
    glfwMakeContextCurrent(window);

    // 3. 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Error] Failed to initialize GLAD" << std::endl;
        return false;
    }

    // 4. 资源查找 (使用 Utils)
    using namespace Utils;
    std::string hdrPath = FindFirstFileByExt("assets/hdrtextures", {".hdr"});
    if(hdrPath.empty()) hdrPath = FindFirstFileByExt("assets/hdrtexture", {".hdr"});

    std::string refPath = FindFirstFileByExt("assets/refmodel", {".gltf", ".glb", ".obj"});
    std::string optPath = FindFirstFileByExt("assets/optmodel", {".gltf", ".glb", ".obj"});

    if (optPath.empty()) optPath = refPath;
    if (refPath.empty()) {
        std::cerr << "[Error] No Models found!" << std::endl;
        return false;
    }

    // =========================================================
    // 5. 新架构核心：加载资源到 Scene
    // =========================================================

    // A. 加载模型 (通过 ResourceManager)
    std::cout << "[System] Loading Models..." << std::endl;
    scene.refModel = Resources::ResourceManager::GetInstance().LoadModel(refPath);
    scene.optModel = Resources::ResourceManager::GetInstance().LoadModel(optPath);

    // B. 烘焙 IBL (通过 IBLBaker)
    if (!hdrPath.empty()) {
        std::cout << "[System] Baking IBL maps from: " << hdrPath << std::endl;
        scene.envMaps = Renderer::IBLBaker::BakeIBL(hdrPath);
    } else {
        std::cerr << "[Warning] No HDR map found, IBL will be black." << std::endl;
    }

    // 6. 初始化渲染器
    // 注意：PBRRenderer 不再负责 IBL 生成，只负责绘制
    targets.Init(1024, 1024); // 内部渲染分辨率

    renderer = std::make_unique<Renderer::PBRRenderer>(targets.width, targets.height);
    visualizer = std::make_unique<Metrics::MetricVisualizer>(targets.width, targets.height);

    // 7. 生成采样点
    float aspect = (float)targets.width / (float)targets.height;
    views = Scene::CameraSampler::GenerateSamples(64, 2.0f, aspect, 0.0f);
    std::cout << "[System] Generated " << views.size() << " samples." << std::endl;

    return true;
}

void Application::ProcessInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void Application::UpdateState() {
    float currentTime = (float)glfwGetTime();
    if (currentTime - lastTime > 1.0f) {
        currentViewIdx = (currentViewIdx + 1) % views.size();
        lastTime = currentTime;
        std::cout << "View [" << currentViewIdx << "/" << views.size() << "]" << std::endl;
    }
}

void Application::RenderPasses() {
    if (views.empty()) return;

    const auto& cam = views[currentViewIdx];

    // --- Pass 1: RefModel (Lit) ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    // [修改] 使用 RenderScene 传入整个场景
    renderer->RenderScene(scene, true);
    // [修改] 传入具体的 Skybox 纹理 ID
    renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    // 拷贝到 texRef
    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texRef);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // --- Pass 2: OptModel (Unlit) ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, false);
    renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    // 拷贝到 texOpt
    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texOpt);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // --- Pass 3: Composition to Screen ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scrWidth, scrHeight);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int panelW = scrWidth / 3;

    // 左屏: 原始 Ref
    glViewport(0, 0, panelW, scrHeight);
    visualizer->RenderComposite(targets.texRef, targets.texOpt, 1);

    // 中屏: 优化 Opt
    glViewport(panelW, 0, panelW, scrHeight);
    visualizer->RenderComposite(targets.texRef, targets.texOpt, 0);

    // 右屏: 热力图
    glViewport(panelW * 2, 0, panelW, scrHeight);
    visualizer->RenderComposite(targets.texRef, targets.texOpt, 2);
}

void Application::Run() {
    while (!glfwWindowShouldClose(window)) {
        ProcessInput();
        UpdateState();
        RenderPasses();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}