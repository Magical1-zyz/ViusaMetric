#pragma once
#include "Scene/Model.h"

namespace Scene {
    class Model;
}

namespace Resources {
    class ResourceManager {
    public:
        // 获取单例实例
        static ResourceManager& GetInstance() {
            static ResourceManager instance;
            return instance;
        }

        // 加载或获取已缓存的模型
        std::shared_ptr<Scene::Model> LoadModel(const std::string& path);

        // 清理所有资源
        void Clear();

    private:
        ResourceManager() = default;
        std::unordered_map<std::string, std::shared_ptr<Scene::Model>> modelCache;
    };
}