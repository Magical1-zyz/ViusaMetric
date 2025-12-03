#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>

#include "Core/PBRRenderer.h"
#include "Core/MetricVisualizer.h"
#include "Core/CameraSampler.h"
#include "Core/Model.h"
#include "Metrics/Evaluator.h"
#include "Metrics/ImageUtils.h"

namespace fs = std::filesystem;

const int SCR_WIDTH = 1024 * 3;
const int SCR_HEIGHT = 1024;
const float SPHERE_RADIUS = 2.0f;
const int VIEW_SAMPLES = 64;

bool SetupResources() {
    fs::path current_p = fs::current_path();
    std::cout << "[System] Searching for resources starting at: " << current_p.string() << std::endl;
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(current_p / "assets/shaders/pbr/pbr.vert")) {
            std::cout << "[System] Found valid resources at: " << (current_p / "assets").string() << std::endl;
            fs::current_path(current_p);
            return true;
        }
        if (!current_p.has_parent_path()) break;
        current_p = current_p.parent_path();
    }
    return false;
}

void SaveScreenshot(const std::string& filepath, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
}

std::string FindFirstHDR(const std::string& folderPath) {
    if (!fs::exists(folderPath)) return "";
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.path().extension() == ".hdr") return entry.path().string();
    }
    return "";
}

std::string FindModelPath(const fs::path& folderPath, const std::string& modelName) {
    if (!fs::exists(folderPath)) return "";
    std::vector<std::string> extensions = {".gltf", ".glb", ".obj"};
    for (const auto& ext : extensions) {
        fs::path p = folderPath / (modelName + ext);
        if (fs::exists(p)) return p.string();
    }
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj") return entry.path().string();
    }
    return "";
}

int main() {
    if (!SetupResources()) return -1;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Visual Metrics", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    fs::create_directories("output");

    Core::PBRRenderer renderer(1024, 1024);
    Core::MetricVisualizer visualizer(SCR_WIDTH, SCR_HEIGHT);

    std::string hdrPath = FindFirstHDR("assets/hdrtextures");
    if (hdrPath.empty()) hdrPath = FindFirstHDR("assets/hdrtexture");
    if (hdrPath.empty()) { std::cerr << "No HDR found!" << std::endl; return -1; }

    std::cout << "Initializing IBL..." << std::endl;
    renderer.InitIBL(hdrPath);

    auto cameras = Core::CameraSampler::GenerateSamples(VIEW_SAMPLES, SPHERE_RADIUS, 1.0f);
    std::string refDir = "assets/refmodel";
    std::string baseDir = fs::exists("assets/basemodel") ? "assets/basemodel" : "assets/optmodel";

    if (!fs::exists(refDir) || !fs::exists(baseDir)) return -1;

    std::ofstream csvFile("output/metrics_results.csv");
    csvFile << "ModelName,ViewID,PSNR(dB),ND(MSE),SD(MSE)\n";

    for (const auto& entry : fs::directory_iterator(refDir)) {
        if (!entry.is_directory()) continue;
        std::string modelName = entry.path().filename().string();
        std::cout << ">>> Processing: " << modelName << std::endl;

        std::string refPath = FindModelPath(entry.path(), modelName);
        std::string basePath = FindModelPath(fs::path(baseDir) / modelName, modelName);

        if (refPath.empty() || basePath.empty()) continue;

        Core::Model modelRef(refPath);
        Core::Model modelBase(basePath);

        double sumPSNR = 0, sumND = 0, sumSD = 0;
        int validSamples = 0;
        std::string outputDir = "output/" + modelName;
        fs::create_directories(outputDir);

        for (const auto& cam : cameras) {
            // 1. Render Ref
            renderer.BeginScene(cam.view, cam.projection, cam.position);
            renderer.RenderModel(modelRef, true);
            renderer.RenderSkybox();
            renderer.EndScene();

            auto pixelsRef = renderer.ReadPixelsColor();
            auto normRef = renderer.ReadPixelsNormal();
            std::vector<float> depthRef(1024 * 1024);
            glBindTexture(GL_TEXTURE_2D, renderer.GetDepthTexture());
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthRef.data());

            // Store Ref Texture for Visualizer
            static unsigned int storedRefTex = 0;
            if(storedRefTex == 0) {
                glGenTextures(1, &storedRefTex);
                glBindTexture(GL_TEXTURE_2D, storedRefTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            glBindTexture(GL_TEXTURE_2D, storedRefTex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRef.data());

            // 2. Render Base
            renderer.BeginScene(cam.view, cam.projection, cam.position);
            renderer.RenderModel(modelBase, false);
            renderer.RenderSkybox();
            renderer.EndScene();

            auto pixelsOpt = renderer.ReadPixelsColor();
            auto normOpt = renderer.ReadPixelsNormal();
            std::vector<float> depthOpt(1024 * 1024);
            glBindTexture(GL_TEXTURE_2D, renderer.GetDepthTexture());
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthOpt.data());

            // 3. Calculate Metrics
            auto psnrRes = Metrics::Evaluator::ComputePSNR(pixelsRef, pixelsOpt, 1024, 1024);
            double nd = Metrics::Evaluator::ComputeNormalError(normRef, normOpt, 1024, 1024);
            auto silRef = Metrics::ImageUtils::GenerateSilhouetteCPU(depthRef, normRef, 1024, 1024);
            auto silOpt = Metrics::ImageUtils::GenerateSilhouetteCPU(depthOpt, normOpt, 1024, 1024);
            double sd = Metrics::Evaluator::ComputeSilhouetteError(silRef, silOpt, 1024, 1024);

            sumPSNR += psnrRes.second; sumND += nd; sumSD += sd; validSamples++;

            csvFile << modelName << "," << cam.id << "," << psnrRes.second << "," << nd << "," << sd << "\n";

            // 4. Visualize & Save (Classified)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

            // Save PSNR Map
            glClear(GL_COLOR_BUFFER_BIT);
            visualizer.RenderComparison(storedRefTex, renderer.GetColorTexture(), 0);
            SaveScreenshot(outputDir + "/view_" + std::to_string(cam.id) + "_PSNR.png", SCR_WIDTH, SCR_HEIGHT);

            // Save SD Map
            glClear(GL_COLOR_BUFFER_BIT);
            visualizer.RenderComparison(storedRefTex, renderer.GetColorTexture(), 2);
            SaveScreenshot(outputDir + "/view_" + std::to_string(cam.id) + "_SD.png", SCR_WIDTH, SCR_HEIGHT);

            glfwSwapBuffers(window);
            glfwPollEvents();
            std::cout << "  - View " << cam.id + 1 << "/64 | PSNR: " << psnrRes.second << "\r" << std::flush;
        }

        if (validSamples > 0) {
            csvFile << modelName << ",AVERAGE," << (sumPSNR/validSamples) << "," << (sumND/validSamples) << "," << (sumSD/validSamples) << "\n";
        }
        std::cout << "\nFinished " << modelName << ". Avg PSNR: " << (sumPSNR/validSamples) << std::endl;
    }
    return 0;
}