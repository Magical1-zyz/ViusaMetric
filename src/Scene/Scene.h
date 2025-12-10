#pragma once
#include "Model.h"
#include "Renderer/IBLBaker.h" // 获取 IBLMaps 定义

namespace Scene {
    struct Scene {
        // 使用 shared_ptr，因为资源可能被管理器共享
        std::shared_ptr<Model> refModel;
        std::shared_ptr<Model> optModel;

        // 环境数据
        Renderer::IBLMaps envMaps;

        // 简单的资源释放辅助
        void Cleanup() {
            if (envMaps.envCubemap) glDeleteTextures(1, &envMaps.envCubemap);
            if (envMaps.irradianceMap) glDeleteTextures(1, &envMaps.irradianceMap);
            if (envMaps.prefilterMap) glDeleteTextures(1, &envMaps.prefilterMap);
            if (envMaps.brdfLUT) glDeleteTextures(1, &envMaps.brdfLUT);
        }
    };
}