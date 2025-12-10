#pragma once

#include <assimp/scene.h>
#include "Scene/Mesh.h" // 包含 Mesh 定义 (Vertex, Texture, MaterialProps)


namespace Scene {

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