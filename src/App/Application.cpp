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

    // 局部目录生成闭包，顺便初始化局部 CSV 和它的表头
    auto initLocalDir = [&](const std::string& dirName) {
        fs::path dir = base / dirName;
        if (!fs::exists(dir)) fs::create_directories(dir);

        fs::path csvPath = dir / (modelName + "_metrics_" + dirName + ".csv");
        std::ofstream f(csvPath);
        if (f.is_open()) {
            f << "ViewIndex,ErrorValue\n";
        }
    };

    initLocalDir("psnr");
    initLocalDir("normal");
    initLocalDir("silhouette");

    // ================= 根据 Config 分别生成三个图例 =================
    // 提取一个通用的生成图例的 Lambda 表达式
    auto generateLegend = [&](const std::string& filename, const std::string& topText, const std::string& midText, const std::string& bottomText) {
        fs::path legendPath = root / filename;
        if (!fs::exists(legendPath)) {
            int legW = 120;  // 拓宽图像以容纳文字
            int legH = 600;
            int barW = 40;   // 左侧颜色条的宽度

            // 初始化背景为暗灰色 (RGB: 40,40,40)，使白色文字和高对比度热力图更加清晰
            std::vector<unsigned char> pixels(legW * legH * 3, 40);

            // 【黑科技】内置 3x5 像素的点阵字体，支持数字 0-9 和小数点
            const char* font[11] = {
                    "111101101101111", // 0
                    "010110010010111", // 1
                    "111001111100111", // 2
                    "111001111001111", // 3
                    "101101111001001", // 4
                    "111100111001111", // 5
                    "111100111101111", // 6
                    "111001010010010", // 7
                    "111101111101111", // 8
                    "111101111001111", // 9
                    "000000000000010"  // .
            };

            // 绘制文字的 Lambda 闭包
            auto drawText = [&](int startX, int startY, const std::string& text, int scale) {
                for (char c : text) {
                    int idx = -1;
                    if (c >= '0' && c <= '9') idx = c - '0';
                    else if (c == '.') idx = 10;

                    if (idx >= 0) {
                        const char* bitmap = font[idx];
                        for (int py = 0; py < 5; ++py) {
                            for (int px = 0; px < 3; ++px) {
                                if (bitmap[py * 3 + px] == '1') {
                                    // 放大像素点
                                    for(int sy = 0; sy < scale; ++sy) {
                                        for(int sx = 0; sx < scale; ++sx) {
                                            int drawX = startX + px * scale + sx;
                                            int drawY = startY + py * scale + sy;
                                            if(drawX >= 0 && drawX < legW && drawY >= 0 && drawY < legH) {
                                                int pIdx = (drawY * legW + drawX) * 3;
                                                pixels[pIdx+0] = 255; // 文字颜色：纯白
                                                pixels[pIdx+1] = 255;
                                                pixels[pIdx+2] = 255;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // 移动光标，准备画下一个字符 (字符宽 3 + 间距 1 = 4)
                    startX += 4 * scale;
                }
            };

            // 1. 绘制左侧的颜色渐变条
            for (int y = 0; y < legH; ++y) {
                float value = 1.0f - static_cast<float>(y) / static_cast<float>(legH - 1);
                auto smoothstep = [](float edge0, float edge1, float x) {
                    float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));
                    return t * t * (3.0f - 2.0f * t);
                };
                float pi = 3.1415926f;
                float r = smoothstep(0.5f, 0.8f, value);
                float g = std::sin(value * pi);
                float b = smoothstep(0.5f, 0.2f, value);
                unsigned char cR = static_cast<unsigned char>(r * 255.0f);
                unsigned char cG = static_cast<unsigned char>(g * 255.0f);
                unsigned char cB = static_cast<unsigned char>(b * 255.0f);

                for (int x = 0; x < barW; ++x) {
                    int idx = (y * legW + x) * 3;
                    pixels[idx + 0] = cR;
                    pixels[idx + 1] = cG;
                    pixels[idx + 2] = cB;
                }
            }

            // 2. 绘制右侧的数值标签
            int textX = barW + 15;
            int textScale = 3;  // 字体放大倍数
            int charH = 5 * textScale;

            drawText(textX, 10, topText, textScale);                              // 顶部 (红色/最大误差)
            drawText(textX, (legH - charH) / 2, midText, textScale);              // 中部 (绿色/中等误差)
            drawText(textX, legH - 10 - charH, bottomText, textScale);            // 底部 (蓝色/最小误差)

            // 写入本地 PNG 文件
            stbi_write_png(legendPath.string().c_str(), legW, legH, 3, pixels.data(), legW * 3);
        }
    };

    // --- 动态计算标签并输出图例 ---

    // 快速截取两位小数的格式化工具
    auto formatFloat = [](float v) {
        std::string s = std::to_string(v);
        return s.substr(0, 4);
    };

    // PSNR 热力图的最大真实颜色误差 = 1.0 / 倍率
    float psnrMax = 1.0f / config.render.colorErrorMultiplier;
    std::string psnrTopStr = formatFloat(psnrMax);
    std::string psnrMidStr = formatFloat(psnrMax / 2.0f);

    generateLegend(config.paths.legendPsnr, psnrTopStr, psnrMidStr, "0.0");
    generateLegend(config.paths.legendNormal, "1.0", "0.5", "0.0");
    generateLegend(config.paths.legendSilhouette, "1.0", "0.5", "0.0");
    // ====================================================================
}

void Application::AppendToGlobalCSV(const std::string& metricType, double avgError) {
    std::string filename;
    if (metricType == "PSNR") filename = "metrics_psnr.csv";
    else if (metricType == "Normal") filename = "metrics_normal.csv";
    else if (metricType == "Silhouette") filename = "metrics_silhouette.csv";
    else return;

    fs::path csvPath = fs::path(config.paths.outputRoot) / filename;
    std::ofstream csv(csvPath, std::ios::app);
    if (csv.is_open()) {
        csv << currentModelName << "," << avgError << "\n";
    }
}

void Application::AppendToLocalCSV(const std::string& metricType, int viewIdx, double error) {
    std::string dirName;
    if (metricType == "PSNR") dirName = "psnr";
    else if (metricType == "Normal") dirName = "normal";
    else if (metricType == "Silhouette") dirName = "silhouette";
    else return;

    fs::path csvPath = fs::path(config.paths.outputRoot) / currentModelName / dirName / (currentModelName + "_metrics_" + dirName + ".csv");
    std::ofstream csv(csvPath, std::ios::app);
    if (csv.is_open()) {
        csv << viewIdx << "," << error << "\n";
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

    // 获取 Config 中设置的纯色背景
    unsigned char bgR = static_cast<unsigned char>(config.render.background.r * 255.0f);
    unsigned char bgG = static_cast<unsigned char>(config.render.background.g * 255.0f);
    unsigned char bgB = static_cast<unsigned char>(config.render.background.b * 255.0f);

    // 取 Config 中设置的模型轮廓色
    unsigned char silR = static_cast<unsigned char>(config.render.silhouetteColor.r * 255.0f);
    unsigned char silG = static_cast<unsigned char>(config.render.silhouetteColor.g * 255.0f);
    unsigned char silB = static_cast<unsigned char>(config.render.silhouetteColor.b * 255.0f);

    for (int i = 0; i < w * h; ++i) {
        unsigned char val = data[i];
        if (val == 0) {
            // 若为剪影背景，则填入 config 的颜色
            rgba[i * 4 + 0] = bgR;
            rgba[i * 4 + 1] = bgG;
            rgba[i * 4 + 2] = bgB;
        } else {
            // 值不为0，代表识别到了模型轮廓，填入我们指定的轮廓颜色
            rgba[i * 4 + 0] = silR;
            rgba[i * 4 + 1] = silG;
            rgba[i * 4 + 2] = silB;
        }
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
            std::string shortName;

            if (currentPhase == RenderPhase::PHASE_IBL_PSNR) {
                metricName = "Average PSNR (dB)";
                shortName = "PSNR";
            }
            else if (currentPhase == RenderPhase::PHASE_NORMAL) {
                metricName = "Normal Error (MSE)";
                shortName = "Normal";
            }
            else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) {
                metricName = "Silhouette Error (MSE)";
                shortName = "Silhouette";
            }

            std::cout << "\n========================================" << std::endl;
            std::cout << "[RESULT] " << metricName << ": " << avgError << std::endl;
            std::cout << "========================================\n" << std::endl;

            AppendToGlobalCSV(shortName, avgError);

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
            drawSkybox = config.render.showSkyboxPSNR;
            renderMode = 0;
            break;
        case RenderPhase::PHASE_SILHOUETTE:
            drawSkybox = config.render.showSkyBoxSilhouette;
            renderMode = 1;
            break;
        case RenderPhase::PHASE_NORMAL:
            drawSkybox = config.render.showSkyBoxNormal;
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
    refDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE || currentPhase == RenderPhase::PHASE_NORMAL) {
        refNormals = ReadTextureFloat(renderer->GetNormalTex(), targets.width, targets.height);
    }

    // --- Pass 2: OptModel ---
    renderer->BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
    renderer->RenderScene(scene, false, config, renderMode);
    if (drawSkybox) renderer->RenderSkybox(scene.envMaps.envCubemap);
    renderer->EndScene();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->GetFBO());

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
    optDepth = ReadTextureDepth(renderer->GetDepthTex(), targets.width, targets.height);
    if (currentPhase == RenderPhase::PHASE_SILHOUETTE || currentPhase == RenderPhase::PHASE_NORMAL) {
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

        // 之前我们将 Normal 数据 copy 到了 texRef/texOpt，所以现在 ReadTextureByte 读到的也是法线颜色
        // 这是正确的，因为法线 (0,0,0) 是非法的，可以用来判断背景（如果 Shader 清屏是 0）
        refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
        optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);

        // 为了使展示和保存的图片具备设定的背景色，我们对用于展示的纹理背景进行染色
        unsigned char bgR = static_cast<unsigned char>(config.render.background.r * 255.0f);
        unsigned char bgG = static_cast<unsigned char>(config.render.background.g * 255.0f);
        unsigned char bgB = static_cast<unsigned char>(config.render.background.b * 255.0f);

        std::vector<unsigned char> refUpload(targets.width * targets.height * 4);
        std::vector<unsigned char> optUpload(targets.width * targets.height * 4);

        for (int i = 0; i < targets.width * targets.height; ++i) {
            // 利用准确的浮点精度判断是否为清屏背景色 (0, 0, 0)
            bool refIsBg = (refFloats[i*3] == 0.0f && refFloats[i*3+1] == 0.0f && refFloats[i*3+2] == 0.0f);
            if (refIsBg) {
                refUpload[i*4+0] = bgR; refUpload[i*4+1] = bgG; refUpload[i*4+2] = bgB;
            } else {
                refUpload[i*4+0] = refBytes[i*3+0]; refUpload[i*4+1] = refBytes[i*3+1]; refUpload[i*4+2] = refBytes[i*3+2];
            }
            refUpload[i*4+3] = 255;

            bool optIsBg = (optFloats[i*3] == 0.0f && optFloats[i*3+1] == 0.0f && optFloats[i*3+2] == 0.0f);
            if (optIsBg) {
                optUpload[i*4+0] = bgR; optUpload[i*4+1] = bgG; optUpload[i*4+2] = bgB;
            } else {
                optUpload[i*4+0] = optBytes[i*3+0]; optUpload[i*4+1] = optBytes[i*3+1]; optUpload[i*4+2] = optBytes[i*3+2];
            }
            optUpload[i*4+3] = 255;
        }

        // 将上了背景色的图片重新覆盖至 GPU，供下方的 Visualizer 渲染以及保存截图时使用
        glBindTexture(GL_TEXTURE_2D, targets.texRef);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, refUpload.data());

        glBindTexture(GL_TEXTURE_2D, targets.texOpt);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, optUpload.data());
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
        // PSNR
        refBytes = ReadTextureByte(targets.texRef, targets.width, targets.height);
        optBytes = ReadTextureByte(targets.texOpt, targets.width, targets.height);

        // 1. 先使用原始包含背景的画面计算出正确的 PSNR
        auto res = Metrics::Evaluator::ComputePSNR(refBytes, optBytes, targets.width, targets.height);
        currentViewError = res.second;

        // ================= PSNR 背景色替换逻辑 =================
        unsigned char bgR = static_cast<unsigned char>(config.render.heatmapBackground.r * 255.0f);
        unsigned char bgG = static_cast<unsigned char>(config.render.heatmapBackground.g * 255.0f);
        unsigned char bgB = static_cast<unsigned char>(config.render.heatmapBackground.b * 255.0f);

        std::vector<unsigned char> refUpload(targets.width * targets.height * 4);
        std::vector<unsigned char> optUpload(targets.width * targets.height * 4);

        for (int i = 0; i < targets.width * targets.height; ++i) {
            // 利用深度缓冲识别背景（深度趋近于 1.0 的必定是背景或天空盒）
            bool refIsBg = (refDepth[i] >= 0.9999f);
            if (refIsBg) {
                // 上传用的展示图填入 Config 背景色
                refUpload[i*4+0] = bgR; refUpload[i*4+1] = bgG; refUpload[i*4+2] = bgB;
                // 将 refBytes 置黑，确保下方的 GenerateHeatmap 能成功判定 isBackground = true
                refBytes[i*3+0] = 0; refBytes[i*3+1] = 0; refBytes[i*3+2] = 0;
            } else {
                refUpload[i*4+0] = refBytes[i*3+0]; refUpload[i*4+1] = refBytes[i*3+1]; refUpload[i*4+2] = refBytes[i*3+2];
            }
            refUpload[i*4+3] = 255;

            bool optIsBg = (optDepth[i] >= 0.9999f);
            if (optIsBg) {
                optUpload[i*4+0] = bgR; optUpload[i*4+1] = bgG; optUpload[i*4+2] = bgB;
                optBytes[i*3+0] = 0; optBytes[i*3+1] = 0; optBytes[i*3+2] = 0;
            } else {
                optUpload[i*4+0] = optBytes[i*3+0]; optUpload[i*4+1] = optBytes[i*3+1]; optUpload[i*4+2] = optBytes[i*3+2];
            }
            optUpload[i*4+3] = 255;
        }

        // 将上了背景色的纯净模型图片重新覆盖至 GPU
        glBindTexture(GL_TEXTURE_2D, targets.texRef);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, refUpload.data());

        glBindTexture(GL_TEXTURE_2D, targets.texOpt);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, targets.width, targets.height, GL_RGBA, GL_UNSIGNED_BYTE, optUpload.data());
        currentViewError = res.second;
    }

    int modeIdx = 0;
    if (currentPhase == RenderPhase::PHASE_IBL_PSNR) modeIdx = 0;
    else if (currentPhase == RenderPhase::PHASE_NORMAL) modeIdx = 1;
    else if (currentPhase == RenderPhase::PHASE_SILHOUETTE) modeIdx = 2;

    // 获取 Config 中设置的背景色（如果上方代码已经声明过 bgR, bgG, bgB，可以直接复用）
    unsigned char heatmapBgR = static_cast<unsigned char>(config.render.heatmapBackground.r * 255.0f);
    unsigned char heatmapBgG = static_cast<unsigned char>(config.render.heatmapBackground.g * 255.0f);
    unsigned char heatmapBgB = static_cast<unsigned char>(config.render.heatmapBackground.b * 255.0f);

    std::vector<unsigned char> heatmapData = Metrics::Evaluator::GenerateHeatmap(
            refBytes, refFloats,
            optBytes, optFloats,
            targets.width, targets.height,
            modeIdx,
            heatmapBgR, heatmapBgG, heatmapBgB,
            config.render.colorErrorMultiplier
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
        // 1. 写入当前视角的误差到单独的 CSV
        AppendToLocalCSV(mName, currentViewIdx, currentViewError);

        // 2. 在这里进行累加！确保每个视角只累加一次！
        accumulatorError += currentViewError;
        lastSavedView = currentViewIdx;
    }
}
