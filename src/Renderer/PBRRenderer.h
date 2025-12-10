#pragma once

#include "Shader.h"
#include "Scene/Scene.h" // 引入 Scene

namespace Renderer {

    class PBRRenderer {
    public:
        PBRRenderer(int scrWidth, int scrHeight);
        ~PBRRenderer();

        // (删除) InitIBL - 功能已移至 IBLBaker

        void BeginScene(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos);

        // [修改] 接受 Scene 对象，而不是单个 Model
        // isRefModel: 决定是渲染 refModel 还是 optModel
        void RenderScene(const Scene::Scene& scene, bool isRefModel);

        // [修改] 渲染天空盒需要传入环境贴图 ID
        void RenderSkybox(unsigned int envCubemap);

        void EndScene();
        unsigned int GetFBO() const { return fbo; }

    private:
        int width, height;
        unsigned int fbo, colorTex, normalTex, depthTex;

        // 仅保留运行时需要的 Shader
        std::unique_ptr<Shader> pbrShader;
        std::unique_ptr<Shader> backgroundShader;

        void SetupFBO();
    };
}