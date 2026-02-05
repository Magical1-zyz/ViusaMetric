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
    if (metricType == "PSNR") filename = "metrics_psnr.csv";
    else if (metricType == "Normal") filename = "metrics_normal.csv";
    else if (metricType == "Silhouette") filename = "metrics_silhouette.csv";
    else return;

    fs::path csvPath = fs::path(config.paths.outputRoot) / filename;
    std::ofstream csv(csvPath, std::ios::app);
    if (csv.is_open()) {
        csv << currentModelName << "," << viewIdx << "," << error << "\n";
    }
}

void Application::SaveScreenshot(int viewIdx) {
    int w = config.window.width;
    int h = config.window.height;
    std::vector<unsigned char> pixels(w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::vector<unsigned char> flipped(w * h * 3);
    for (int y = 0; y < h; ++y) {
        unsigned char* srcRow = pixels.data() + y * w * 3;
        unsigned char* dstRow = flipped.data() + (h - 1 - y) * w * 3;
        std::memcpy(dstRow, srcRow, w * 3);
    }

    std::string filename = currentOutputDir + "/view_" + std::to_string(viewIdx) + ".png";
    stbi_write_png(filename.c_str(), w, h, 3, flipped.data(), w * 3);
}

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
    std::vector<float> data(w * h);
    glBindTexture(GL_TEXTURE_2D, texID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, data.data());
    return data;
}

void Application::UploadGrayscaleToTexture(unsigned int texID, const std::vector<unsigned char>& data, int w, int h) {
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

    targets.Init(config.render.width, config.render.height);
    visualizer = std::make_unique<Metrics::MetricVisualizer>(config.window.width, config.window.height);
    renderer = std::make_unique<Renderer::PBRRenderer>(targets.width, targets.height);
    renderer->SetExposure(config.render.exposure);
    renderer->SetBackground(config.render.background);

    return true;
}

void Application::ProcessSingleModel(const std::string& refPath, const std::string& optPath, const std::string& modelName) {
    currentModelName = modelName;
    SetupOutputDirectories(modelName);

    scene.Cleanup();
    std::cout << "  [System] Loading..." << std::endl;
    scene.refModel = Resources::ResourceManager::GetInstance().LoadModel(refPath);
    scene.optModel = Resources::ResourceManager::GetInstance().LoadModel(optPath);

    fs::path assets = config.paths.assetsRoot;
    std::string hdrPath = Utils::FindFirstFileByExt((assets / config.paths.hdrDir).string(), {".hdr"});
    if (!hdrPath.empty()) {
        if (scene.envMaps.envCubemap == 0) {
            std::cout << "  [System] Baking IBL..." << std::endl;
            scene.envMaps = Renderer::IBLBaker::BakeIBL(hdrPath);
        }
    }

    float aspect = (float)targets.width / (float)targets.height;
    views = Scene::CameraSampler::GenerateSamples(config.sampling.viewCount, config.sampling.radius, aspect, 0.0f);

    currentViewIdx = 0;
    lastTime = (float)glfwGetTime();
    accumulatorError = 0.0;
    currentPhase = RenderPhase::PHASE_IBL_PSNR;
    lastSavedView = -1;

    fs::path outRoot = config.paths.outputRoot;
    currentOutputDir = (outRoot / modelName / "psnr").string();

    while (!glfwWindowShouldClose(window) && currentPhase != RenderPhase::FINISHED) {
        ProcessInput();
        UpdateState();
        RenderPasses();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    std::cout << "[System] Finished " << modelName << std::endl;
}

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
    createTex(texHeatmap, false);
}

void Application::RenderTargets::Cleanup() {
    if (texRef) glDeleteTextures(1, &texRef);
    if (texOpt) glDeleteTextures(1, &texOpt);
    if (texHeatmap) glDeleteTextures(1, &texHeatmap);
}

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

            double avgError = accumulatorError / (double)views.size();
            std::string metricName;

            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) metricName = "Average PSNR (dB)";
            else if (currentPhase == RenderPhase::PHASE_NORMAL) metricName = "Normal Error (MSE)";
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) metricName = "Silhouette Error (MSE)";

            std::cout << "\n========================================" << std::endl;
            std::cout << "[RESULT] " << metricName << ": " << avgError << std::endl;
            std::cout << "========================================\n" << std::endl;

            accumulatorError = 0.0;

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
    renderer->RenderScene(scene, true, config, renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());

    // [修改点2]：在 Normal 模式下，需要读取 Attachment 1 (法线)，否则会读到黑色的 Color Attachment
    if (currentPhase == RenderPhase::PHASE_NORMAL) {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
    } else {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBindTexture(GL_TEXTURE_2D, targets.texRef);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // 恢复读取缓冲区，以免影响后续操作
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    // 数据抓取 (CPU计算用)
    std::vector<float> refDepth, refNormals;
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE || currentPhase == RenderPhase::PHASE_NORMAL) {
        if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
            refDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
        }
        refNormals = ReadTextureFloat(renderer->GetNormalTex(), targets.width, targets.height);
    }

    // --- Pass 2: OptModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, false, config, renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());

    // [修改点2]：同上，Opt Model 也要切换读取源
    if (currentPhase == RenderPhase::PHASE_NORMAL) {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
    } else {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBindTexture(GL_TEXTURE_2D, targets.texOpt);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targets.width, targets.height);

    // 恢复
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    std::vector<float> optDepth, optNormals;
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE || currentPhase == RenderPhase::PHASE_NORMAL) {
        if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
            optDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
        }
        optNormals = ReadTextureFloat(renderer->GetNormalTex(), targets.width, targets.height);
    }

    // =========================================================
    // 核心: CPU 计算误差 + 生成热力图
    // =========================================================
    std::vector<unsigned char> refBytes, optBytes;
    std::vector<float> refFloats, optFloats;

    if (currentPhase == RenderPhase::PHASE_NORMAL) {
        // Normal Mode: Float Data for Calculation, Byte Data for background checking (Ref=Black?)
        refFloats = refNormals;
        optFloats = optNormals;

        // 重新读取 Color Attachment 0 (此时应该是黑色背景 + 白色/黑色模型?)
        // 为了 GenerateHeatmap 能正确判断背景，我们需要 TexRef 和 TexOpt 的“可视”数据
        // 之前我们将 Normal 数据 copy 到了 texRef/texOpt，所以现在 ReadTextureByte 读到的也是法线颜色
        // 这是正确的，因为法线 (0,0,0) 是非法的，可以用来判断背景（如果 Shader 清屏是 0）
        refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
        optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);
    }
    else if(currentPhase == RenderPhase::PHASE_SILHOUETTE){
        std::vector<unsigned char> refSil = Metrics::ImageUtils::GenerateSilhouetteCPU(
                refDepth, refNormals, targets.width, targets.height, 0.01f, 0.1f
        );
        std::vector<unsigned char> optSil = Metrics::ImageUtils::GenerateSilhouetteCPU(
                optDepth, optNormals, targets.width, targets.height, 0.01f, 0.1f
        );

        currentViewError = Metrics::Evaluator::ComputeSilhouetteError(refSil, optSil, targets.width, targets.height);

        UploadGrayscaleToTexture(targets.texRef, refSil, targets.width, targets.height);
        UploadGrayscaleToTexture(targets.texOpt, optSil, targets.width, targets.height);

        refBytes.resize(targets.width * targets.height * 3);
        optBytes.resize(targets.width * targets.height * 3);
        for(size_t i=0; i<refSil.size(); ++i) {
            refBytes[i*3+0] = refSil[i]; refBytes[i*3+1] = refSil[i]; refBytes[i*3+2] = refSil[i];
            optBytes[i*3+0] = optSil[i]; optBytes[i*3+1] = optSil[i]; optBytes[i*3+2] = optSil[i];
        }
    }
    else {
        // PSNR
        refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
        optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);
        auto res = Metrics::Evaluator::ComputePSNR(refBytes, optBytes, targets.width, targets.height);
        currentViewError = res.second;
    }

    accumulatorError += currentViewError;

    int modeIdx = 0;
    if (currentPhase == RenderPhase::PHASE_IBL_PSNR) modeIdx = 0;
    else if (currentPhase == RenderPhase::PHASE_NORMAL) modeIdx = 1;
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

    if (currentViewIdx != lastSavedView) {
        SaveScreenshot(currentViewIdx);
        std::string mName = (currentPhase == RenderPhase::PHASE_IBL_PSNR) ? "PSNR" :
                            (currentPhase == RenderPhase::PHASE_SILHOUETTE ? "Silhouette" : "Normal");
        AppendToGlobalCSV(mName, currentViewIdx, currentViewError);
        lastSavedView = currentViewIdx;
    }
}