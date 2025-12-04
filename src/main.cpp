#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/PBRRenderer.h"
#include "Core/Model.h"
#include "Core/MetricVisualizer.h"

#include <iostream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// 窗口设置：宽度设为 1800 以便容纳三个 600x600 的视口
const int SCR_WIDTH = 1800;
const int SCR_HEIGHT = 600;

// ==============================================================================================
// 1. 路径修复工具 (解决 FILE_NOT_SUCCESFULLY_READ 错误)
// ==============================================================================================
bool SetupWorkingDirectory() {
    fs::path current = fs::current_path();
    // 向上寻找最多 5 层
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(current / "assets/shaders/pbr/pbr.vert")) {
            fs::current_path(current); // 设置为当前工作目录
            std::cout << "[System] Working directory set to: " << current.string() << std::endl;
            return true;
        }
        if (current.has_parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    std::cerr << "[Fatal] Could not find 'assets' folder! Check your project structure." << std::endl;
    return false;
}

std::string FindFirstModel(const std::string& folderPath) {
    if (!fs::exists(folderPath)) return "";
    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        std::string ext = entry.path().extension().string();
        // 转换为小写比较
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

// ==============================================================================================
// 2. 纹理绘制工具 (把 FBO 纹理画到屏幕上)
// ==============================================================================================
class TextureBlitter {
public:
    unsigned int shaderProgram;
    unsigned int VAO;

    TextureBlitter() {
        // 简单的 Shader：直接把纹理画出来，不做任何处理
        const char* vs = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTex;
            out vec2 TexCoords;
            void main() {
                gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
                TexCoords = aTex;
            }
        )";
        const char* fs = R"(
            #version 330 core
            out vec4 FragColor;
            in vec2 TexCoords;
            uniform sampler2D screenTexture;
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";

        unsigned int v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
        shaderProgram = glCreateProgram(); glAttachShader(shaderProgram, v); glAttachShader(shaderProgram, f); glLinkProgram(shaderProgram);
        glDeleteShader(v); glDeleteShader(f);

        float quadVertices[] = {
                // pos        // tex
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                1.0f, -1.0f,  1.0f, 0.0f,

                -1.0f,  1.0f,  0.0f, 1.0f,
                1.0f, -1.0f,  1.0f, 0.0f,
                1.0f,  1.0f,  1.0f, 1.0f
        };
        unsigned int VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }

    void Draw(unsigned int textureID) {
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};

// ==============================================================================================
// 3. 主流程
// ==============================================================================================
int main() {
    // 1. 设置工作目录 (必须最先执行!)
    if (!SetupWorkingDirectory()) return -1;

    // 2. 初始化窗口
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Ref(Lit) vs Opt(Unlit) vs Heatmap", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // 3. 加载资源
    std::string hdrPath = FindFirstHDR("assets/hdrtextures");
    if(hdrPath.empty()) hdrPath = FindFirstHDR("assets/hdrtexture");
    if (hdrPath.empty()) { std::cerr << "No HDR found!" << std::endl; return -1; }

    std::string refPath = FindFirstModel("assets/refmodel");
    std::string optPath = FindFirstModel("assets/optmodel");
    if (optPath.empty()) { std::cout << "OptModel not found, using RefModel for both." << std::endl; optPath = refPath; }
    if (refPath.empty()) { std::cerr << "No Models found!" << std::endl; return -1; }

    std::cout << "HDR: " << hdrPath << std::endl;
    std::cout << "Ref: " << refPath << std::endl;
    std::cout << "Opt: " << optPath << std::endl;

    // 4. 初始化模块
    // 渲染分辨率可以比屏幕小，也可以一样。这里设为 1024x1024 保证纹理质量
    int RENDER_W = 1024, RENDER_H = 1024;

    Core::PBRRenderer renderer(RENDER_W, RENDER_H);
    renderer.InitIBL(hdrPath);

    Core::Model modelRef(refPath);
    Core::Model modelOpt(optPath);

    Core::MetricVisualizer visualizer(RENDER_W, RENDER_H);
    TextureBlitter blitter;

    // 5. 创建临时纹理 (用于保存 Ref 的结果，因为 Renderer 只有一个 FBO)
    unsigned int refResultTex;
    glGenTextures(1, &refResultTex);
    glBindTexture(GL_TEXTURE_2D, refResultTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, RENDER_W, RENDER_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 6. 渲染循环
    float angle = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        // 旋转相机
        angle += 0.005f;
        float radius = 3.5f;
        glm::vec3 camPos(sin(angle) * radius, 1.5f, cos(angle) * radius);
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0, 0.5, 0), glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

        // -------------------------------------------------------
        // 阶段 A: 渲染 RefModel (PBR Lit)
        // -------------------------------------------------------
        renderer.BeginScene(view, proj, camPos);
        // true = RefModel (Lit Mode)
        renderer.RenderModel(modelRef, true);
        renderer.RenderSkybox();
        renderer.EndScene();

        // 把结果从 Renderer 的 FBO 拷贝出来，存到 refResultTex
        // 这样我们就可以接着用 Renderer 去画 OptModel 了
        glBindTexture(GL_TEXTURE_2D, refResultTex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer.GetColorTexture());
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, RENDER_W, RENDER_H);

        // -------------------------------------------------------
        // 阶段 B: 渲染 OptModel (Unlit / Baked)
        // -------------------------------------------------------
        renderer.BeginScene(view, proj, camPos);
        // false = OptModel (Unlit Mode)
        renderer.RenderModel(modelOpt, false);
        renderer.RenderSkybox();
        renderer.EndScene();

        // 此时 renderer.GetColorTexture() 里存的是 OptModel 的结果

        // -------------------------------------------------------
        // 阶段 C: 分屏显示 (Screen Pass)
        // -------------------------------------------------------
        // 绑定默认帧缓冲 (屏幕)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int viewW = SCR_WIDTH / 3;
        int viewH = SCR_HEIGHT;

        // [左屏] RefModel (Lit)
        glViewport(0, 0, viewW, viewH);
        blitter.Draw(refResultTex);

        // [中屏] OptModel (Unlit)
        glViewport(viewW, 0, viewW, viewH);
        blitter.Draw(renderer.GetColorTexture());

        // [右屏] Heatmap (Ref vs Opt)
        glViewport(viewW * 2, 0, viewW, viewH);
        // 假设 Mode 0 是热力图/PSNR
        visualizer.RenderComparison(refResultTex, renderer.GetColorTexture(), 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}