#include "Application.h"
#include "Metrics/Evaluator.h"
#include "Metrics/MetricVisualizer.h"
#include "Renderer/IBLBaker.h"
#include "Renderer/PBRRenderer.h"
#include "Resources/ResourceManager.h"
#include "Scene/CameraSampler.h"
#include "Utils/FileSystemUtils.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// =========================================================
// 辅助函数
// =========================================================
std::vector<float> ReadTextureFloat(unsigned int texID, int w, int h) {
    std::vector<float> data(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, data.data());
    return data;
}

std::vector<unsigned char> ReadTextureByte(unsigned int texID, int w, int h) {
    std::vector<unsigned char> data(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    return data;
}

// =========================================================
// RenderTargets 实现 (之前可能漏掉了这部分)
// =========================================================
void Application::RenderTargets::Init(int w, int h) {
    width = w;
    height = h;
    auto createTex = [&](unsigned int& tex, bool isFloat) {
        if (tex) glDeleteTextures(1, &tex);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, isFloat ? GL_RGBA16F : GL_RGBA, width, height, 0, GL_RGBA, isFloat ? GL_FLOAT : GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    createTex(texRef, true);
    createTex(texOpt, true);
    createTex(texHeatmap, false); // 热力图用普通 RGBA8
}

void Application::RenderTargets::Cleanup() {
    if (texRef) glDeleteTextures(1, &texRef);
    if (texOpt) glDeleteTextures(1, &texOpt);
    if (texHeatmap) glDeleteTextures(1, &texHeatmap);
}

// =========================================================
// 输出辅助函数实现
// =========================================================
void Application::EnsureDirectories() {
    namespace fs = std::filesystem;
    if (!fs::exists("output")) fs::create_directory("output");
    if (!fs::exists("output/psnr")) fs::create_directory("output/psnr");
    if (!fs::exists("output/normal")) fs::create_directory("output/normal");
    if (!fs::exists("output/silhouette")) fs::create_directory("output/silhouette");
}

void Application::InitPhaseOutput(const std::string& phaseName) {
    currentOutputDir = "output/" + phaseName;
    lastSavedView = -1;

    if (csvFile.is_open()) csvFile.close();

    std::string csvPath = currentOutputDir + "/result.csv";
    csvFile.open(csvPath);
    if (csvFile.is_open()) {
        csvFile << "Model,View,Error\n";
    }
}

void Application::SaveScreenshot(int viewIdx) {
    std::vector<unsigned char> pixels(scrWidth * scrHeight * 3);
    glReadPixels(0, 0, scrWidth, scrHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 垂直翻转
    std::vector<unsigned char> flipped(scrWidth * scrHeight * 3);
    for (int y = 0; y < scrHeight; ++y) {
        unsigned char* srcRow = pixels.data() + y * scrWidth * 3;
        unsigned char* dstRow = flipped.data() + (scrHeight - 1 - y) * scrWidth * 3;
        std::memcpy(dstRow, srcRow, scrWidth * 3);
    }

    std::string filename = currentOutputDir + "/view_" + std::to_string(viewIdx) + ".png";
    stbi_write_png(filename.c_str(), scrWidth, scrHeight, 3, flipped.data(), scrWidth * 3);
}

void Application::LogToCSV(int viewIdx, double error) {
    if (csvFile.is_open()) {
        csvFile << "MyModel," << viewIdx << "," << error << "\n";
    }
}

void Application::UpdateHeatmapTexture(const std::vector<unsigned char>& data) {
    glBindTexture(GL_TEXTURE_2D, targets.texHeatmap);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
}

// =========================================================
// Application 实现
// =========================================================
Application::Application(int width, int height, const char* title)
        : scrWidth(width), scrHeight(height), appTitle(title) {}

Application::~Application() {
    if (csvFile.is_open()) csvFile.close();
    targets.Cleanup();
    scene.Cleanup();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Application::Init() {
    if (!Utils::SetupWorkingDirectory()) return false;
    EnsureDirectories();

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(scrWidth, scrHeight, appTitle, NULL, NULL);
    if (!window) return false;
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

    // 资源加载
    std::string hdrPath = Utils::FindFirstFileByExt("assets/hdrtextures", {".hdr"});
    std::string refPath = Utils::FindFirstFileByExt("assets/refmodel", {".gltf", ".glb", ".obj"});
    std::string optPath = Utils::FindFirstFileByExt("assets/optmodel", {".gltf", ".glb", ".obj"});

    if (refPath.empty()) {
        std::cerr << "[Error] Ref Model not found!" << std::endl;
        return false;
    }
    if (optPath.empty()) optPath = refPath;

    std::cout << "[System] Loading Models..." << std::endl;
    scene.refModel = Resources::ResourceManager::GetInstance().LoadModel(refPath);
    scene.optModel = Resources::ResourceManager::GetInstance().LoadModel(optPath);

    if (!hdrPath.empty()) {
        std::cout << "[System] Baking IBL maps..." << std::endl;
        scene.envMaps = Renderer::IBLBaker::BakeIBL(hdrPath);
    }

    targets.Init(1024, 1024);
    renderer = std::make_unique<Renderer::PBRRenderer>(targets.width, targets.height);
    visualizer = std::make_unique<Metrics::MetricVisualizer>(scrWidth, scrHeight);

    float aspect = (float)targets.width / (float)targets.height;
    views = Scene::CameraSampler::GenerateSamples(64, 2.0f, aspect, 0.0f);
    std::cout << "[System] Generated " << views.size() << " samples." << std::endl;

    InitPhaseOutput("psnr");
    return true;
}

void Application::ProcessInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void Application::UpdateState() {
    if (currentPhase == RenderPhase::FINISHED) return;

    float currentTime = (float)glfwGetTime();
    // 0.5s 一帧
    if (currentTime - lastTime > 0.5f) {
        lastTime = currentTime;
        currentViewIdx++;

        if (currentViewIdx >= views.size()) {
            currentViewIdx = 0;

            double avgError = accumulatorError / (double)views.size();
            std::string metricName;
            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) metricName = "Average PSNR (dB)";
            else if (currentPhase == RenderPhase::PHASE_NORMAL) metricName = "Normal Error (MSE)";
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) metricName = "Silhouette Error (MSE)";

            std::cout << "\n========================================" << std::endl;
            std::cout << "[RESULT] " << metricName << ": " << avgError << std::endl;
            std::cout << "========================================\n" << std::endl;

            accumulatorError = 0.0;

            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) {
                currentPhase = RenderPhase::PHASE_SILHOUETTE;
                std::cout << ">>> Phase Switch: IBL -> Silhouette" << std::endl;
                InitPhaseOutput("silhouette");
            }
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
                currentPhase = RenderPhase::PHASE_NORMAL;
                std::cout << ">>> Phase Switch: Silhouette -> Normal" << std::endl;
                InitPhaseOutput("normal");
            }
            else if (currentPhase == RenderPhase::PHASE_NORMAL) {
                currentPhase = RenderPhase::FINISHED;
                std::cout << ">>> All Metrics Calculated. Done." << std::endl;
                if (csvFile.is_open()) csvFile.close();
            }
        }
    }
}

void Application::RenderPasses() {
    if (views.empty()) return;

    RenderPhase phaseToDraw = currentPhase;
    if (phaseToDraw == RenderPhase::FINISHED) {
        phaseToDraw = RenderPhase::PHASE_NORMAL;
    }

    const auto& cam = views[currentViewIdx];

    bool drawSkybox = false;
    int renderMode = 0;

    switch (phaseToDraw) {
        case RenderPhase::PHASE_IBL_PSNR:
            drawSkybox = false;
            renderMode = 0;
            break;
        case RenderPhase::PHASE_SILHOUETTE:
            drawSkybox = false;
            renderMode = 2;
            break;
        case RenderPhase::PHASE_NORMAL:
            drawSkybox = false;
            renderMode = 1;
            break;
    }

    // --- Pass 1: RefModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, true, renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texRef);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // --- Pass 2: OptModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, false, renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texOpt);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // =========================================================
    // 核心: CPU 计算误差 + 生成热力图
    // =========================================================
    if (currentPhase != RenderPhase::FINISHED) {

        std::vector<unsigned char> refBytes, optBytes;
        std::vector<float> refFloats, optFloats;

        if (currentPhase == RenderPhase::PHASE_NORMAL) {
            // 法线模式：Float数据
            refFloats = ReadTextureFloat(targets.texRef, targets.width, targets.height);
            optFloats = ReadTextureFloat(targets.texOpt, targets.width, targets.height);

            double mse = Metrics::Evaluator::ComputeNormalError(refFloats, optFloats, targets.width, targets.height);
            accumulatorError += mse;
            currentViewError = mse;
        } else {
            // 颜色/轮廓模式：Byte数据
            refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
            optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);

            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) {
                auto res = Metrics::Evaluator::ComputePSNR(refBytes, optBytes, targets.width, targets.height);
                accumulatorError += res.second;
                currentViewError = res.second;
            } else {
                // PHASE_SILHOUETTE
                double silErr = Metrics::Evaluator::ComputeSilhouetteError(refBytes, optBytes, targets.width, targets.height);
                accumulatorError += silErr;
                currentViewError = silErr;
            }
        }

        // 映射 App Phase -> Evaluator Mode
        int modeIdx = 0;
        if (currentPhase == RenderPhase::PHASE_IBL_PSNR) modeIdx = 0;
        else if (currentPhase == RenderPhase::PHASE_NORMAL) modeIdx = 1; // Evaluator::Normal Expects Float
        else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) modeIdx = 2;

        std::vector<unsigned char> heatmapData = Metrics::Evaluator::GenerateHeatmap(
                refBytes, refFloats,
                optBytes, optFloats,
                targets.width, targets.height,
                modeIdx
        );

        UpdateHeatmapTexture(heatmapData);
    }

    // --- Pass 3: Visualization ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scrWidth, scrHeight);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    visualizer->RenderComparison(targets.texRef, targets.texOpt, targets.texHeatmap);

    // 保存结果
    if (currentPhase != RenderPhase::FINISHED && currentViewIdx != lastSavedView) {
        SaveScreenshot(currentViewIdx);
        LogToCSV(currentViewIdx, currentViewError);
        lastSavedView = currentViewIdx;
    }
}

// 确保 Run 函数存在 (之前可能漏掉了)
void Application::Run() {
    while (!glfwWindowShouldClose(window)) {
        ProcessInput();
        UpdateState();
        RenderPasses();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}