#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// --- STB Image Write (用于保存图片) ---
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// --- C++ 标准库 ---
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>

// --- 工程模块 ---
#include "Core/PBRRenderer.h"
#include "Core/MetricVisualizer.h"
#include "Core/CameraSampler.h"
#include "Core/Model.h"
#include "Metrics/Evaluator.h"
#include "Metrics/ImageUtils.h"

namespace fs = std::filesystem;

// --- 全局配置 ---
const int SCR_WIDTH = 1024 * 3; // 左+中+右 (3072)
const int SCR_HEIGHT = 1024;
const float SPHERE_RADIUS = 2.0f; // 摄像机所在的半球半径
const int VIEW_SAMPLES = 64;      // 斐波那契采样点数

// --- 辅助函数：保存图像 ---
void SaveScreenshot(const std::string& filepath, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // OpenGL 原点在左下角，PNG 在左上角，需要垂直翻转
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
}

// --- 辅助函数：查找文件夹下的首个 .hdr 文件 ---
std::string FindFirstHDR(const std::string& folderPath) {
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.path().extension() == ".hdr") {
            return entry.path().string();
        }
    }
    return "";
}

// --- 辅助函数：查找模型文件 (支持 .gltf, .glb, .obj) ---
std::string FindModelPath(const fs::path& folderPath, const std::string& modelName) {
    // 优先查找同名文件
    std::vector<std::string> extensions = {".gltf", ".glb", ".obj"};
    for (const auto& ext : extensions) {
        fs::path p = folderPath / (modelName + ext);
        if (fs::exists(p)) return p.string();
    }
    // 如果找不到同名，查找文件夹内任意支持的模型文件
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj") {
            return entry.path().string();
        }
    }
    return "";
}

int main() {
    namespace fs = std::filesystem;
    if (!fs::exists("assets")) {
        std::cout << "[System] 'assets' not found in current directory. Searching parent..." << std::endl;
        // 尝试去父目录找 (适配 cmake-build-release 在根目录下一级的情况)
        if (fs::exists("../assets")) {
            fs::current_path("..");
            std::cout << "[System] Working directory changed to project root." << std::endl;
        } else {
            std::cerr << "[System] Fatal Error: Could not find 'assets' directory!" << std::endl;
            // 这里可以暂停一下让你看到报错
            std::cin.get();
            return -1;
        }
    }

    // 1. 初始化 GLFW 和 OpenGL 上下文
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // 隐藏窗口进行离屏渲染 (如果需要看过程可以设为 GLFW_TRUE)
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Visual Metrics Batch", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 2. 准备输出目录
    fs::create_directories("output");

    // 3. 初始化渲染器
    Core::PBRRenderer renderer(1024, 1024); // 单个视口大小 1024x1024
    Core::MetricVisualizer visualizer(SCR_WIDTH, SCR_HEIGHT); // 组合视口大小

    // 4. 加载 HDR 环境图 (假设只有一个环境)
    std::string hdrPath = FindFirstHDR("assets/hdrtexture");
    if (hdrPath.empty()) {
        std::cerr << "Error: No .hdr texture found in assets/hdrtexture/" << std::endl;
        return -1;
    }
    std::cout << "Initializing IBL with: " << hdrPath << " (This may take a while...)" << std::endl;
    renderer.InitIBL(hdrPath);

    // 5. 生成视点
    float aspect = 1.0f; // 单个视口的宽高比 1:1
    auto cameras = Core::CameraSampler::GenerateSamples(VIEW_SAMPLES, SPHERE_RADIUS, aspect);

    // 6. 扫描模型目录
    std::string refDir = "assets/refmodel";
    std::string baseDir = "assets/optmodel";

    if (!fs::exists(refDir) || !fs::exists(baseDir)) {
        std::cerr << "Error: Model directories not found!" << std::endl;
        return -1;
    }

    // 7. 打开 CSV 文件准备写入
    std::ofstream csvFile("output/metrics_results.csv");
    csvFile << "ModelName,ViewID,PSNR(dB),ND(MSE),SD(MSE),TotalTime(ms)\n";

    // 8. 批量处理循环
    for (const auto& entry : fs::directory_iterator(refDir)) {
        if (!entry.is_directory()) continue;

        std::string modelName = entry.path().filename().string();
        std::cout << ">>> Processing Model Pair: " << modelName << std::endl;

        // 寻找对应文件
        std::string refPath = FindModelPath(entry.path(), modelName);
        std::string basePath = FindModelPath(fs::path(baseDir) / modelName, modelName);

        if (refPath.empty() || basePath.empty()) {
            std::cerr << "Skipping " << modelName << ": Missing ref or base file." << std::endl;
            continue;
        }

        // 加载模型
        std::cout << "Loading Ref: " << refPath << std::endl;
        Core::Model modelRef(refPath);

        std::cout << "Loading Base: " << basePath << std::endl;
        Core::Model modelBase(basePath); // Base 模型也会自动归一化到单位球

        // 创建该模型的输出子目录
        fs::create_directories("output/" + modelName);

        // 遍历所有视点
        for (const auto& cam : cameras) {
            // --- A. 渲染 Ref ---
            renderer.BeginScene(cam.view, cam.projection, cam.position);
            renderer.RenderModel(modelRef, true); // true = isRef (use PBR textures)
            renderer.RenderSkybox();
            renderer.EndScene(); // 结果存入 renderer.fbo

            // 暂存 Ref 数据
            auto pixelsRef = renderer.ReadPixelsColor();
            auto normRef = renderer.ReadPixelsNormal();
            // 注意：这里我们需要深度图来做 SD。我在之前的 ImageUtils 里假设了传入 float vector
            // 你需要给 PBRRenderer 加一个 ReadPixelsDepth() 并在 ImageUtils.h 声明
            // 这里为了演示完整性，假设你已经补全了 ReadPixelsDepth
            std::vector<float> depthRef(1024 * 1024);
            glBindTexture(GL_TEXTURE_2D, renderer.GetDepthTexture());
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthRef.data());

//            unsigned int texRefColor = renderer.GetColorTexture();
//            unsigned int texRefNormal = renderer.GetNormalTexture();

            // --- B. 渲染 Base ---
            renderer.BeginScene(cam.view, cam.projection, cam.position);
            renderer.RenderModel(modelBase, false); // false = isBase (diffuse only)
            renderer.RenderSkybox();
            renderer.EndScene();

            auto pixelsOpt = renderer.ReadPixelsColor();
            auto normOpt = renderer.ReadPixelsNormal();
            std::vector<float> depthOpt(1024 * 1024);
            glBindTexture(GL_TEXTURE_2D, renderer.GetDepthTexture());
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthOpt.data());

//            unsigned int texBaseColor = renderer.GetColorTexture();
//            unsigned int texBaseNormal = renderer.GetNormalTexture();

            // --- C. 计算指标 (CPU) ---

            // 1. PSNR
            auto psnrRes = Metrics::Evaluator::ComputePSNR(pixelsRef, pixelsOpt, 1024, 1024);

            // 2. ND (Normal Difference)
            // 此时 normRef/normOpt 是 float 数据
            double nd = Metrics::Evaluator::ComputeNormalError(normRef, normOpt, 1024, 1024);

            // 3. SD (Silhouette Difference)
            // 使用 CPU 算法提取轮廓
            auto silRef = Metrics::ImageUtils::GenerateSilhouetteCPU(depthRef, normRef, 1024, 1024);
            auto silOpt = Metrics::ImageUtils::GenerateSilhouetteCPU(depthOpt, normOpt, 1024, 1024);
            double sd = Metrics::Evaluator::ComputeSilhouetteError(silRef, silOpt, 1024, 1024);

            // 写入 CSV
            csvFile << modelName << "," << cam.id << ","
                    << std::fixed << std::setprecision(4) << psnrRes.second << ","
                    << nd << "," << sd << ",0\n"; // 0 placeholder for time

            // --- D. 生成对比图 (GPU Visualizer) ---
            // 这一步我们将绘制到屏幕 FBO (默认窗口Framebuffer)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT);

            // 绘制组合图：左(Ref) 中(Base) 右(误差热力图 - 默认显示 Color Diff)
            // 注意：PBRRenderer 的 texRefColor 是刚才渲染 Base 之前的状态，
            // 由于 PBRRenderer 复用同一个 FBO，此时 texColor 其实存的是 Base 的图。
            // *** 关键修正 ***：
            // PBRRenderer 设计是单例 FBO。为了同时持有两张纹理进行对比，
            // 我们需要先把 Ref 的纹理拷贝出来，或者实例化两个 Renderer。
            // 简单起见：我们在这里利用 glCopyImageSubData 或者在渲染 Ref 后 glReadPixels 存入 CPU，
            // 但为了 Visualizer 能工作，我们需要两个 GPU 纹理 ID。

            // 临时解决方案：Visualizer 渲染前，我们需要重新绑定 Ref 纹理。
            // 由于 renderer 覆盖了纹理，我们需要额外申请一个纹理 ID 来暂存 Ref 结果。
            // 优化建议：给 PBRRenderer 增加 "SaveSnapshot / RestoreSnapshot" 或者简单的 CopyTexture 功能。

            // 这里演示手动拷贝 Ref 纹理的方法：
            static unsigned int storedRefTex = 0;
            if(storedRefTex == 0) {
                glGenTextures(1, &storedRefTex);
                glBindTexture(GL_TEXTURE_2D, storedRefTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }

            // *重新渲染一遍 Ref* (为了获得纹理ID供 Shader 使用，这是一个性能权衡，或者使用 glCopyTexSubImage2D)
            // 为了最高效率，应在渲染 Ref 后立即拷贝：
            /* renderer.RenderModel(Ref);
               glCopyImageSubData(renderer.GetColorTexture(), ..., storedRefTex, ...);
               renderer.RenderModel(Base);
            */
            // 但为了代码简洁，这里假设你已经在 PBRRenderer.h 中添加了一个 `CopyColorBufferTo(unsigned int targetTexID)` 函数
            // 或者我们可以简单地先把 Ref 的像素传给 OpenGL 创建一个新纹理。

            glBindTexture(GL_TEXTURE_2D, storedRefTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRef.data());

            // 现在 storedRefTex 是 Ref，renderer.GetColorTexture() 是 Base
            // 渲染可视化
            visualizer.RenderComparison(storedRefTex, renderer.GetColorTexture(), 0); // 0 = Color Difference Mode

            // 保存截图
            std::string filename = "output/" + modelName + "/view_" + std::to_string(cam.id) + ".png";
            SaveScreenshot(filename, SCR_WIDTH, SCR_HEIGHT);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        std::cout << "Finished " << modelName << std::endl;
    }

    csvFile.close();
    glfwTerminate();
    return 0;
}