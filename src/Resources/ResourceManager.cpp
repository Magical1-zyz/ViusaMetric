#include "ResourceManager.h"

namespace Resources {
    std::shared_ptr<Scene::Model> ResourceManager::LoadModel(const std::string& path) {
        // 检查缓存
        if (modelCache.find(path) != modelCache.end()) {
            return modelCache[path];
        }

        // 加载新模型
        std::cout << "[Res] Loading Model: " << path << std::endl;
        auto model = std::make_shared<Scene::Model>(path);
        modelCache[path] = model;
        return model;
    }

    void ResourceManager::Clear() {
        modelCache.clear();
    }
}