#include "PBRRenderer.h"
#include "Utils/GeometryUtils.h"

namespace Renderer {

    PBRRenderer::PBRRenderer(int w, int h) : width(w), height(h) {
        pbrShader = std::make_unique<Shader>("assets/shaders/pbr/pbr.vert", "assets/shaders/pbr/pbr.frag");
        pbrShader->use();
        pbrShader->setInt("irradianceMap", 0);
        pbrShader->setInt("prefilterMap", 1);
        pbrShader->setInt("brdfLUT", 2);
        pbrShader->setInt("albedoMap", 3);
        pbrShader->setInt("normalMap", 4);
        pbrShader->setInt("metallicRoughnessMap", 5);

        backgroundShader = std::make_unique<Shader>("assets/shaders/pbr/background.vert", "assets/shaders/pbr/background.frag");
        backgroundShader->use();
        backgroundShader->setInt("environmentMap", 0);

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

        // Color Attachment 0: RGBA16F
        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        // Color Attachment 1: RGB16F
        glGenTextures(1, &normalTex);
        glBindTexture(GL_TEXTURE_2D, normalTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

        // 【修改点】分离清除缓冲的操作。确保法线贴图缓冲区的背景被绝对置零 (0, 0, 0)，为 Evaluator 计算误差剔除背景做准备
        float bgColor[] = { this->background.r, this->background.g, this->background.b, 1.0f };
        float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, bgColor); // GL_COLOR_ATTACHMENT0 (画面背景)
        glClearBufferfv(GL_COLOR, 1, black);   // GL_COLOR_ATTACHMENT1 (法线背景强制纯黑)
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        pbrShader->use();
        pbrShader->setMat4("view", view);
        pbrShader->setMat4("projection", projection);
        pbrShader->setVec3("camPos", camPos);

        if (visShader) {
            visShader->use();
            visShader->setMat4("view", view);
            visShader->setMat4("projection", projection);
        }

        backgroundShader->use();
        backgroundShader->setMat4("view", view);
        backgroundShader->setMat4("projection", projection);
    }

    void PBRRenderer::RenderScene(const Scene::Scene& scene, bool isRefModel, const AppConfig& config, int renderMode) {
        Scene::Model* targetModel = isRefModel ? scene.refModel.get() : scene.optModel.get();
        if (!targetModel) return;

        glm::mat4 modelMatrix = targetModel->GetNormalizationMatrix();

        if (renderMode == 0) {
            pbrShader->use();
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, scene.envMaps.irradianceMap);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_CUBE_MAP, scene.envMaps.prefilterMap);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, scene.envMaps.brdfLUT);

            int lit = isRefModel ? config.render.refPBR : config.render.optPBR;
            pbrShader->setInt("u_ShadingModel", lit);
            pbrShader->setFloat("u_Exposure", this->exposure);
            pbrShader->setVec3("u_AlbedoDefault", glm::vec3(1.0f));
            pbrShader->setFloat("u_RoughnessDefault", config.render.roughnessDefault);
            pbrShader->setFloat("u_MetallicDefault", config.render.metallicDefault);
            pbrShader->setMat4("model", modelMatrix);

            targetModel->Draw(pbrShader->ID);
        }
        else {
            visShader->use();
            visShader->setInt("u_VisMode", renderMode);
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