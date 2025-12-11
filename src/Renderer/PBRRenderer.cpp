#include "PBRRenderer.h"
#include "Utils/GeometryUtils.h"

namespace Renderer {

    PBRRenderer::PBRRenderer(int w, int h) : width(w), height(h) {
        // 1. PBR Shader
        pbrShader = std::make_unique<Shader>("assets/shaders/pbr/pbr.vert", "assets/shaders/pbr/pbr.frag");
        pbrShader->use();
        pbrShader->setInt("irradianceMap", 0);
        pbrShader->setInt("prefilterMap", 1);
        pbrShader->setInt("brdfLUT", 2);
        pbrShader->setInt("albedoMap", 3);
        pbrShader->setInt("normalMap", 4);
        pbrShader->setInt("metallicRoughnessMap", 5);

        // 2. Background Shader
        backgroundShader = std::make_unique<Shader>("assets/shaders/pbr/background.vert", "assets/shaders/pbr/background.frag");
        backgroundShader->use();
        backgroundShader->setInt("environmentMap", 0);

        // 3. [新增] Vis Shader (用于法线和轮廓)
        visShader = std::make_unique<Shader>("assets/shaders/visualize/vis_model.vert", "assets/shaders/visualize/vis_model.frag");

        SetupFBO();
    }

    PBRRenderer::~PBRRenderer() {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &colorTex);
        glDeleteTextures(1, &normalTex);
        glDeleteTextures(1, &depthTex);
    }

    void PBRRenderer::SetupFBO() {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // 使用 RGBA16F
        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        glGenTextures(1, &normalTex);
        glBindTexture(GL_TEXTURE_2D, normalTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTex, 0);

        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

        unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void PBRRenderer::BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
        // 使用纯黑清屏，Alpha=1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // 更新 PBR Shader 矩阵
        pbrShader->use();
        pbrShader->setMat4("view", view);
        pbrShader->setMat4("projection", projection);
        pbrShader->setVec3("camPos", camPos);

        // [关键修复] 必须同时更新 Vis Shader 矩阵，否则切换模式后模型会因为 MVP 矩阵缺失而不可见
        if (visShader) {
            visShader->use();
            visShader->setMat4("view", view);
            visShader->setMat4("projection", projection);
        }

        backgroundShader->use();
        backgroundShader->setMat4("view", view);
        backgroundShader->setMat4("projection", projection);
    }

    void PBRRenderer::RenderScene(const Scene::Scene& scene, bool isRefModel, int renderMode) {
        Scene::Model* targetModel = isRefModel ? scene.refModel.get() : scene.optModel.get();
        if (!targetModel) return;

        glm::mat4 modelMatrix = targetModel->GetNormalizationMatrix();

        // 模式 0: 标准 PBR
        if (renderMode == 0) {
            pbrShader->use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, scene.envMaps.irradianceMap);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, scene.envMaps.prefilterMap);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.envMaps.brdfLUT);

            pbrShader->setInt("u_ShadingModel", isRefModel ? 0 : 1);
            pbrShader->setMat3("u_EnvRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))));
            pbrShader->setFloat("u_Exposure", this->exposure);
            pbrShader->setVec3("u_AlbedoDefault", glm::vec3(1.0f));
            pbrShader->setFloat("u_RoughnessDefault", 0.5f);
            pbrShader->setFloat("u_MetallicDefault", 0.0f);

            pbrShader->setMat4("model", modelMatrix);
            targetModel->Draw(pbrShader->ID);
        }
            // 模式 1(Normal) 或 2(Silhouette)
        else {
            visShader->use();
            visShader->setInt("u_VisMode", renderMode); // 1=Normal, 2=Silhouette
            visShader->setMat4("model", modelMatrix);
            targetModel->Draw(visShader->ID);
        }
    }

    void PBRRenderer::RenderSkybox(unsigned int envCubemap) {
        glDepthFunc(GL_LEQUAL);
        backgroundShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        Utils::GeometryUtils::RenderCube();
        glDepthFunc(GL_LESS);
    }

    void PBRRenderer::EndScene() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
}