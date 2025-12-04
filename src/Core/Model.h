#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h" // 包含 Mesh 定义 (Vertex, Texture, MaterialProps)
#include <string>
#include <vector>

namespace Core {

    class Model {
    public:
        std::vector<Texture> textures_loaded;
        std::vector<Mesh>    meshes;
        std::string          directory;

        glm::vec3 boundsMin;
        glm::vec3 boundsMax;
        glm::mat4 modelMatrix;

        Model(std::string const &path);
        void Draw(unsigned int shaderID);
        glm::mat4 GetNormalizationMatrix() const;

    private:
        const aiScene* scenePtr;

        void loadModel(std::string const &path);
        void processNode(aiNode *node, const aiScene *scene);
        Mesh processMesh(aiMesh *mesh, const aiScene *scene);

        std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);
        unsigned int TextureFromFile(const char *path, const std::string &texDirectory, const aiScene* scene);
        void computeBoundingBox();
    };
}