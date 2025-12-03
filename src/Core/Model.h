#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include <string>
#include <vector>
#include <map>

namespace Core {

    class Model {
    public:
        // 存储模型的所有纹理，避免重复加载
        std::vector<Texture> textures_loaded;
        std::vector<Mesh>    meshes;
        std::string          directory;

        // 归一化参数
        glm::vec3 boundsMin;
        glm::vec3 boundsMax;
        glm::mat4 modelMatrix; // 归一化后的模型变换矩阵

        /* 函数  */
        Model(std::string const &path);

        // 绘制模型所有网格
        void Draw(unsigned int shaderID);

        // 获取归一化矩阵（将模型缩放并移动到单位球）
        glm::mat4 GetNormalizationMatrix() const;

    private:
        /* 私有变量与函数  */
        const aiScene* scenePtr; // 保存 scene 指针以处理嵌入纹理

        void loadModel(std::string const &path);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);

        // 加载纹理（支持文件路径和嵌入式 GLB）
        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);

        // 从内存或文件加载纹理 ID
        unsigned int TextureFromFile(const char *path, const std::string &texDirectory, const aiScene* scene);

        // 计算包围盒并生成 modelMatrix
        void computeBoundingBox();
    };
}