#pragma once
#include "Renderer/Shader.h"
#include "Scene/Scene.h"

namespace Renderer {
    class PBRRenderer {
    public:
        PBRRenderer(int width, int height);
        ~PBRRenderer();

        void BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos);
        // [新增] renderMode 参数，默认 0
        void RenderScene(const Scene::Scene& scene, bool isRefModel, int renderMode = 0);
        void RenderSkybox(unsigned int envCubemap);
        void EndScene();

        unsigned int GetFBO() const { return fbo; }

    private:
        int width, height;
        unsigned int fbo;
        unsigned int colorTex, normalTex, depthTex;

        std::unique_ptr<Shader> pbrShader;
        std::unique_ptr<Shader> backgroundShader;
        // [新增] 可视化 Shader
        std::unique_ptr<Shader> visShader;

        void SetupFBO();
    };
}