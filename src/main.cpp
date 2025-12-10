#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/PBRRenderer.h"
#include "Core/Model.h"
#include "Core/MetricVisualizer.h"
#include "Core/CameraSampler.h" // [新增] 斐波那契采样

#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

const int SCR_WIDTH = 1800;
const int SCR_HEIGHT = 600;

bool SetupWorkingDirectory() {
    fs::path current = fs::current_path();
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(current / "assets/shaders/pbr/pbr.vert")) {
            fs::current_path(current);
            return true;
        }
        if (current.has_parent_path()) current = current.parent_path();
        else break;
    }
    return false;
}

std::string FindFirstModel(const std::string& folderPath) {
    if (!fs::exists(folderPath)) return "";
    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".gltf" || ext == ".glb" || ext == ".obj") return entry.path().string();
    }
    return "";
}

std::string FindFirstHDR(const std::string& folderPath) {
    if (!fs::exists(folderPath)) return "";
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".hdr") return entry.path().string();
    }
    return "";
}

int main() {
    if (!SetupWorkingDirectory()) return -1;

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Ref(Lit) vs Opt(Unlit) vs Heatmap", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // 资源加载
    std::string hdrPath = FindFirstHDR("assets/hdrtextures");
    if(hdrPath.empty()) hdrPath = FindFirstHDR("assets/hdrtexture");

    std::string refPath = FindFirstModel("assets/refmodel");
    std::string optPath = FindFirstModel("assets/optmodel");
    if (optPath.empty()) optPath = refPath;

    int RENDER_W = 1024, RENDER_H = 1024;
    Core::PBRRenderer renderer(RENDER_W, RENDER_H);
    renderer.InitIBL(hdrPath);

    Core::Model modelRef(refPath);
    Core::Model modelOpt(optPath);

    Core::MetricVisualizer visualizer(RENDER_W, RENDER_H);

    // 中间纹理 (GL_RGBA16F)
    unsigned int texRef, texOpt;
    auto createTex = [&](unsigned int& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, RENDER_W, RENDER_H, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    createTex(texRef);
    createTex(texOpt);

    // [新增] 斐波那契采样 (64点, 半径2.0)
    float aspect = (float)RENDER_W / (float)RENDER_H;
    // 参数：数量, 半径, 宽高比, 抖动
    std::vector<Core::CameraSample> views = Core::CameraSampler::GenerateSamples(64, 2.0f, aspect, 0.0f);

    int currentViewIdx = 0;
    float lastTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentTime = (float)glfwGetTime();
        if (currentTime - lastTime > 1.0f) {
            currentViewIdx = (currentViewIdx + 1) % views.size();
            lastTime = currentTime;
            std::cout << "View [" << currentViewIdx << "]" << std::endl;
        }

        const auto& cam = views[currentViewIdx];

        // -------------------------------------------------------
        // Pass 1: Render RefModel (Lit) -> texRef
        // -------------------------------------------------------
        renderer.BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
        renderer.RenderModel(modelRef, true); // true = Lit
        renderer.RenderSkybox();
        renderer.EndScene();

        // 拷贝 Ref 结果
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.GetFBO());
        glBindTexture(GL_TEXTURE_2D, texRef);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, RENDER_W, RENDER_H);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // -------------------------------------------------------
        // Pass 2: Render OptModel (Unlit) -> texOpt
        // -------------------------------------------------------
        renderer.BeginScene(cam.viewMatrix, cam.projMatrix, cam.position);
        renderer.RenderModel(modelOpt, false); // false = Unlit
        renderer.RenderSkybox();
        renderer.EndScene();

        // 拷贝 Opt 结果
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer.GetFBO());
        glBindTexture(GL_TEXTURE_2D, texOpt);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, RENDER_W, RENDER_H);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // -------------------------------------------------------
        // Pass 3: Screen Composition
        // -------------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int viewW = SCR_WIDTH / 3;

        // [修复位置] 如果你之前觉得反了，这里我们显式指定谁显示在哪

        // 左屏 (Mode 0): 强制显示 texRef (RefModel)
        glViewport(0, 0, viewW, SCR_HEIGHT);
        // 注意：RenderComposite 的参数是 (texRef, texOpt, mode)
        // 这里的 texRef 和 texOpt 只是传入 Shader 的两个纹理槽位
        // Shader 中 Mode 0 采样第一个纹理，Mode 1 采样第二个
        visualizer.RenderComposite(texRef, texOpt, 1);

        // 中屏 (Mode 1): 强制显示 texOpt (OptModel)
        glViewport(viewW, 0, viewW, SCR_HEIGHT);
        visualizer.RenderComposite(texRef, texOpt, 0);

        // 右屏 (Mode 2): 热力图
        glViewport(viewW * 2, 0, viewW, SCR_HEIGHT);
        visualizer.RenderComposite(texRef, texOpt, 2);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteTextures(1, &texRef);
    glDeleteTextures(1, &texOpt);
    glfwTerminate();
    return 0;
}