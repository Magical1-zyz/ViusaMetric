#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "Shader.h"
#include "Model.h"

namespace Core {

    class PBRRenderer {
    public:
        PBRRenderer(int scrWidth, int scrHeight);
        ~PBRRenderer();

        // 初始化 IBL (读取 HDR，生成 Irradiance, Prefilter, BRDF LUT)
        void InitIBL(const std::string& hdrPath);

        // 调整视口大小
        void Resize(int width, int height);

        // 开始新的一帧渲染 (绑定 FBO, 清屏)
        void BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos);

        // 渲染模型 (区分 Ref 和 Base)
        void RenderModel(Model& model, bool isRefModel);

        // 绘制天空盒背景
        void RenderSkybox();

        // 结束场景渲染 (解绑 FBO)
        void EndScene();

        // 获取渲染结果纹理 ID
        unsigned int GetColorTexture() const { return colorTex; }
        unsigned int GetNormalTexture() const { return normalTex; }
        unsigned int GetDepthTexture() const { return depthTex; }
        unsigned int GetFBO() const { return fbo; }

        // 读取像素数据用于 CPU 端计算 (MSE/PSNR)
        std::vector<unsigned char> ReadPixelsColor();
        std::vector<float> ReadPixelsNormal(); // 返回 float 以便高精度计算

    private:
        int width, height;

        // --- FBO 相关 ---
        unsigned int fbo, rbo;
        unsigned int colorTex;  // 存储 PBR 颜色
        unsigned int normalTex; // 存储世界空间法线 (RGB)
        unsigned int depthTex;  // 存储深度 (用于 SD)

        // --- IBL 相关 ---
        unsigned int envCubemap;
        unsigned int irradianceMap;
        unsigned int prefilterMap;
        unsigned int brdfLUTTexture;

        // --- 几何体 VAO (用于渲染背景立方体和屏幕四边形) ---
        unsigned int cubeVAO = 0;
        unsigned int cubeVBO = 0;
        unsigned int quadVAO = 0;
        unsigned int quadVBO = 0;

        // --- Shaders ---
        std::unique_ptr<Shader> pbrShader;
        std::unique_ptr<Shader> backgroundShader;
        std::unique_ptr<Shader> equirectangularToCubemapShader;
        std::unique_ptr<Shader> irradianceShader;
        std::unique_ptr<Shader> prefilterShader;
        std::unique_ptr<Shader> brdfShader;

        // --- 内部辅助函数 ---
        void SetupFBO();
        void RenderCube();
        void RenderQuad();
        void PrecomputeIBL(const std::string& hdrPath);
    };
}