#include "Application.h"
#include "Metrics/Evaluator.h"
#include "Metrics/ImageUtils.h"
#include "Metrics/MetricVisualizer.h"
#include "Renderer/IBLBaker.h"
#include "Renderer/PBRRenderer.h"
#include "Resources/ResourceManager.h"
#include "Scene/CameraSampler.h"
#include "Utils/FileSystemUtils.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

// =========================================================
// 辅助函数
// =========================================================
void Application::SetupOutputDirectories(const std::string& modelName) {
    fs::path root = config.paths.outputRoot;
    fs::path base = root / modelName;
    if (!fs::exists(base)) fs::create_directories(base);
    if (!fs::exists(base / "psnr")) fs::create_directories(base / "psnr");
    if (!fs::exists(base / "normal")) fs::create_directories(base / "normal");
    if (!fs::exists(base / "silhouette")) fs::create_directories(base / "silhouette");
}

void Application::AppendToGlobalCSV(const std::string& metricType, int viewIdx, double error) {
    std::string filename;

    // 根据类型决定文件名
    if (metricType == "PSNR") filename = "metrics_psnr.csv";
    else if (metricType == "Normal") filename = "metrics_normal.csv";
    else if (metricType == "Silhouette") filename = "metrics_silhouette.csv";
    else return; // 未知类型不写入

    fs::path csvPath = fs::path(config.paths.outputRoot) / filename;

    // 追加模式打开
    std::ofstream csv(csvPath, std::ios::app);
    if (csv.is_open()) {
        // 格式: ModelName, ViewIndex, ErrorValue
        csv << currentModelName << "," << viewIdx << "," << error << "\n";
    }
}

void Application::SaveScreenshot(int viewIdx) {
    int w = config.window.width;
    int h = config.window.height;
    std::vector<unsigned char> pixels(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // 垂直翻转
    std::vector<unsigned char> flipped(w * h * 3);
    for (int y = 0; y < h; ++y) {
        unsigned char* srcRow = pixels.data() + y * w * 3;
        unsigned char* dstRow = flipped.data() + (h - 1 - y) * w * 3;
        std::memcpy(dstRow, srcRow, w * 3);
    }

    std::string filename = currentOutputDir + "/view_" + std::to_string(viewIdx) + ".png";
    stbi_write_png(filename.c_str(), w, h, 3, flipped.data(), w * 3);
}

// =========================================================
// Helpers (Textures Read)
// =========================================================
std::vector<float> Application::ReadTextureFloat(unsigned int texID, int w, int h) {
    std::vector<float> data(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, data.data());
    return data;
}

std::vector<unsigned char> Application::ReadTextureByte(unsigned int texID, int w, int h) {
    std::vector<unsigned char> data(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    return data;
}

std::vector<float> Application::ReadTextureDepth(unsigned int texID, int w, int h) {
    std::vector<float> data(w * h); // 单通道
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, data.data());
    return data;
}

void Application::UploadGrayscaleToTexture(unsigned int texID, const std::vector<unsigned char>& data, int w, int h) {
    // 将单通道扩展为 RGBA (白色轮廓，黑色背景)
    std::vector<unsigned char> rgba(w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        unsigned char val = data[i];
        rgba[i * 4 + 0] = val;
        rgba[i * 4 + 1] = val;
        rgba[i * 4 + 2] = val;
        rgba[i * 4 + 3] = 255;
    }
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

void Application::UpdateHeatmapTexture(const std::vector<unsigned char>& data) {
    glBindTexture(GL_TEXTURE_2D, targets.texHeatmap);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
}

// =========================================================
// Application 实现
// =========================================================
Application::Application(const AppConfig& cfg) : config(cfg) {}

Application::~Application() {
    targets.Cleanup();
    scene.Cleanup();
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

bool Application::InitSystem() {
    if (!Utils::SetupWorkingDirectory()) return false;

    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (!config.render.display)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    else
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    window = glfwCreateWindow(config.window.width, config.window.height, config.window.title.c_str(), NULL, NULL);
    if (!window) return false;
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return false;

    // 初始化可视化器和渲染器资源 (RenderTargets)
    targets.Init(config.render.width, config.render.height);
    visualizer = std::make_unique<Metrics::MetricVisualizer>(config.window.width, config.window.height);
    renderer = std::make_unique<Renderer::PBRRenderer>(targets.width, targets.height);
    renderer->SetExposure(config.render.exposure);

    return true;
}

void Application::ProcessSingleModel(const std::string& refPath, const std::string& optPath, const std::string& modelName) {
    currentModelName = modelName;

    // --- A. 准备阶段 ---
    // 1. 创建该模型的输出目录 output/modelName/
    SetupOutputDirectories(modelName);

    // 2. 加载模型
    // 注意：这里需要先清理上一个场景
    scene.Cleanup();

    std::cout << "  [System] Loading..." << std::endl;
    scene.refModel = Resources::ResourceManager::GetInstance().LoadModel(refPath);
    scene.optModel = Resources::ResourceManager::GetInstance().LoadModel(optPath);

    // 3. 处理 HDR (如果有全局 HDR 配置，最好只加载一次，这里为了简单假设每次检查)
    fs::path assets = config.paths.assetsRoot;
    std::string hdrPath = Utils::FindFirstFileByExt((assets / config.paths.hdrDir).string(), {".hdr"});
    if (!hdrPath.empty()) {
        if (scene.envMaps.envCubemap == 0) {
            std::cout << "  [System] Baking IBL..." << std::endl;
            scene.envMaps = Renderer::IBLBaker::BakeIBL(hdrPath);
        }
    }

    // 4. 重置状态
    float aspect = (float)targets.width / (float)targets.height;
    views = Scene::CameraSampler::GenerateSamples(config.sampling.viewCount, config.sampling.radius, aspect, 0.0f);

    currentViewIdx = 0;
    lastTime = (float)glfwGetTime(); // 重置时间基准
    accumulatorError = 0.0;
    currentPhase = RenderPhase::PHASE_IBL_PSNR; // 从第一个阶段开始
    lastSavedView = -1;

    // 设置初始输出目录 (psnr)
    fs::path outRoot = config.paths.outputRoot;
    currentOutputDir = (outRoot / modelName / "psnr").string();

    // --- B. 渲染循环 ---
    // 这个循环只针对当前模型运行，直到所有阶段完成
    while (!glfwWindowShouldClose(window) && currentPhase != RenderPhase::FINISHED) {
        ProcessInput(); // 保持窗口响应
        UpdateState();  // 状态机流转
        RenderPasses(); // 渲染与计算

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- C. 清理 ---
    // 循环结束意味着该模型处理完毕
    std::cout << "[System] Finished " << modelName << std::endl;
    // 显存清理交给下一次循环开头的 scene.Cleanup() 或析构函数
}

// =========================================================
// RenderTargets 实现
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
// Rendering process
// =========================================================
void Application::ProcessInput() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void Application::UpdateState() {
    if (currentPhase == RenderPhase::FINISHED) return;

    float currentTime = (float)glfwGetTime();
    if (currentTime - lastTime > config.render.delayTime) {
        lastTime = currentTime;
        currentViewIdx++;

        if (currentViewIdx >= views.size()) {
            currentViewIdx = 0;

            // 计算平均值
            double avgError = accumulatorError / (double)views.size();
            std::string metricName;

            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) metricName = "Average PSNR (dB)";
            else if (currentPhase == RenderPhase::PHASE_NORMAL) metricName = "Normal Error (MSE)";
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) metricName = "Silhouette Error (MSE)";

            std::cout << "\n========================================" << std::endl;
            std::cout << "[RESULT] " << metricName << ": " << avgError << std::endl;
            std::cout << "========================================\n" << std::endl;

            accumulatorError = 0.0;

            // 切换阶段
            fs::path outRoot = config.paths.outputRoot;
            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) {
                currentPhase = RenderPhase::PHASE_SILHOUETTE;
                currentOutputDir = (outRoot / currentModelName / "silhouette").string();
                std::cout << ">>> Phase Switch: IBL -> Silhouette" << std::endl;
                lastSavedView = -1;
            }
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
                currentPhase = RenderPhase::PHASE_NORMAL;
                currentOutputDir = (outRoot / currentModelName / "normal").string();
                std::cout << ">>> Phase Switch: Silhouette -> Normal" << std::endl;
                lastSavedView = -1;
            }
            else if (currentPhase == RenderPhase::PHASE_NORMAL) {
                currentPhase = RenderPhase::FINISHED;
                std::cout << ">>> All Metrics Calculated. Done." << std::endl;
            }
        }
    }
}

void Application::RenderPasses() {
    if (views.empty() || currentPhase == RenderPhase::FINISHED) return;

    RenderPhase phaseToDraw = currentPhase;
    const auto& cam = views[currentViewIdx];
    bool drawSkybox = false;
    int renderMode = 0;

    switch (phaseToDraw) {
        case RenderPhase::PHASE_IBL_PSNR:
            drawSkybox = true;
            renderMode = 0;
            break;
        case RenderPhase::PHASE_SILHOUETTE:
            drawSkybox = false;
            renderMode = 1;
            break;
        case RenderPhase::PHASE_NORMAL:
            drawSkybox = false;
            renderMode = 1;
            break;
    }

    // --- Pass 1: RefModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, true, config,renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texRef);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // 如果是轮廓阶段，我们需要抓取法线和深度数据用于后续 CPU 计算
    std::vector<float> refDepth, refNormals;
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
        refDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
        refNormals = ReadTextureFloat(renderer->GetNormalTex(), targets.width, targets.height);
    }

    // --- Pass 2: OptModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, false, config,renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());
    glBindTexture(GL_TEXTURE_2D, targets.texOpt);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // 抓取 Opt 的法线和深度
    std::vector<float> optDepth, optNormals;
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE && currentPhase != RenderPhase::FINISHED) {
        optDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
        optNormals = ReadTextureFloat(renderer->GetNormalTex(), targets.width, targets.height);
    }

    // =========================================================
    // 核心: CPU 计算误差 + 生成热力图
    // =========================================================


        std::vector<unsigned char> refBytes, optBytes;
        std::vector<float> refFloats, optFloats;

        if (currentPhase == RenderPhase::PHASE_NORMAL) {
            // 法线模式：Float数据
            refFloats = ReadTextureFloat(targets.texRef, targets.width, targets.height);
            optFloats = ReadTextureFloat(targets.texOpt, targets.width, targets.height);
            currentViewError = Metrics::Evaluator::ComputeNormalError(refFloats, optFloats, targets.width, targets.height);
        }else if(currentPhase == RenderPhase::PHASE_SILHOUETTE){
            // 1. 调用算法生成轮廓图 (0或255)
            // 这里的 depthThresh 和 normalThresh 可以根据需要调整
            std::vector<unsigned char> refSil = Metrics::ImageUtils::GenerateSilhouetteCPU(
                    refDepth, refNormals, targets.width, targets.height, 0.01f, 0.1f
            );
            std::vector<unsigned char> optSil = Metrics::ImageUtils::GenerateSilhouetteCPU(
                    optDepth, optNormals, targets.width, targets.height, 0.01f, 0.1f
            );

            // 2. 计算误差
            currentViewError = Metrics::Evaluator::ComputeSilhouetteError(refSil, optSil, targets.width, targets.height);

            // 3. 将生成的轮廓图上传回 texRef 和 texOpt，这样屏幕上看到的就是白色的轮廓线，而不是填充模型
            UploadGrayscaleToTexture(targets.texRef, refSil, targets.width, targets.height);
            UploadGrayscaleToTexture(targets.texOpt, optSil, targets.width, targets.height);

            // 4. 为热力图准备数据 (GenerateHeatmap 需要 refBytes/optBytes)
            // 因为轮廓图是 Byte 类型的，所以赋值给 refBytes
            // 注意：GenerateHeatmap 内部对于 mode=2 会只取 R 通道，所以这里拷贝数据没问题
            // 为了匹配 RGBA 格式，我们需要把单通道数据做一下格式转换给 Evaluator 吗？
            // Evaluator::GenerateHeatmap 接收的是 RGB/RGBA bytes。
            // 我们可以直接把 Upload 后的 texRef 读回来，或者手动构建 refBytes。
            // 手动构建比较快：
            refBytes.resize(targets.width * targets.height * 3);
            optBytes.resize(targets.width * targets.height * 3);
            for(size_t i=0; i<refSil.size(); ++i) {
                refBytes[i*3+0] = refSil[i]; refBytes[i*3+1] = refSil[i]; refBytes[i*3+2] = refSil[i];
                optBytes[i*3+0] = optSil[i]; optBytes[i*3+1] = optSil[i]; optBytes[i*3+2] = optSil[i];
            }
        }
        else {
            // psnr
            refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
            optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);
            auto res = Metrics::Evaluator::ComputePSNR(refBytes, optBytes, targets.width, targets.height);
            currentViewError = res.second;

        }

        accumulatorError += currentViewError;

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


    // --- Pass 3: Visualization ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, config.window.width, config.window.height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    visualizer->RenderComparison(targets.texRef, targets.texOpt, targets.texHeatmap);

    // 保存结果
    if (currentViewIdx != lastSavedView) {
        SaveScreenshot(currentViewIdx);
        std::string mName = (currentPhase == RenderPhase::PHASE_IBL_PSNR) ? "PSNR" :
                            (currentPhase == RenderPhase::PHASE_SILHOUETTE ? "Silhouette" : "Normal");
        AppendToGlobalCSV(mName, currentViewIdx, currentViewError);
        lastSavedView = currentViewIdx;
    }
}