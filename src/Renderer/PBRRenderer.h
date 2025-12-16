#pragma once
#include "Renderer/Shader.h"
#include "Scene/Scene.h"

namespace Renderer {
    class PBRRenderer {
    public:
        PBRRenderer(int width, int height);
        ~PBRRenderer();

        void BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos);
        void RenderScene(const Scene::Scene& scene, bool isRefModel, bool lit, int renderMode = 0);
        void RenderSkybox(unsigned int envCubemap);
        void EndScene();

        void SetExposure(float exp) {exposure = exp;}

        unsigned int GetFBO() const { return fbo; }
        unsigned int GetColorTex() const {return colorTex;}
        unsigned int GetNormalTex() const {return normalTex;}
        unsigned int GetDepthTex() const {return depthTex;}

    private:
        int width, height;
        unsigned int fbo;
        unsigned int colorTex, normalTex, depthTex;
        float exposure = 1.0f;

        std::unique_ptr<Shader> pbrShader;
        std::unique_ptr<Shader> backgroundShader;
        std::unique_ptr<Shader> visShader;

        void SetupFBO();
    };
}