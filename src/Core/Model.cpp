#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Model.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace Core {

    Model::Model(std::string const &path) {
        loadModel(path);
        computeBoundingBox();
    }

    void Model::Draw(unsigned int shaderID) {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shaderID);
    }

    glm::mat4 Model::GetNormalizationMatrix() const {
        return modelMatrix;
    }

    void Model::loadModel(std::string const &path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
                                                 aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                                                 aiProcess_FlipUVs | aiProcess_CalcTangentSpace |
                                                 aiProcess_JoinIdenticalVertices);

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        this->scenePtr = scene;
        // [修复] 使用 filesystem 自动处理路径分隔符
        std::filesystem::path p(path);
        this->directory = p.parent_path().string();

        processNode(scene->mRootNode, scene);
    }

    void Model::processNode(aiNode *node, const aiScene *scene) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 vector;
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            if(mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                if (mesh->HasTangentsAndBitangents()) {
                    vector.x = mesh->mTangents[i].x;
                    vector.y = mesh->mTangents[i].y;
                    vector.z = mesh->mTangents[i].z;
                    vertex.Tangent = vector;
                    vector.x = mesh->mBitangents[i].x;
                    vector.y = mesh->mBitangents[i].y;
                    vector.z = mesh->mBitangents[i].z;
                    vertex.Bitangent = vector;
                }
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }
            vertices.push_back(vertex);
        }

        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        std::vector<Texture> armMaps = loadMaterialTextures(material, aiTextureType_UNKNOWN, "metallicRoughnessMap");
        textures.insert(textures.end(), armMaps.begin(), armMaps.end());

        return Mesh(vertices, indices, textures);
    }

    std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {
        std::vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++) {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true; break;
                }
            }
            if(!skip) {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory, this->scenePtr);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

    // [修改] 带有白色备用纹理的加载函数
    unsigned int Model::TextureFromFile(const char *path, const std::string &texDirectory, const aiScene* scene) {
        std::string filename = std::string(path);
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char *data = nullptr;

        const aiTexture* embeddedTex = nullptr;
        if (path[0] == '*') {
            int index = std::stoi(filename.substr(1));
            if (index < scene->mNumTextures) embeddedTex = scene->mTextures[index];
        }

        if (embeddedTex) {
            if (embeddedTex->mHeight == 0) {
                data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(embeddedTex->pcData), embeddedTex->mWidth, &width, &height, &nrComponents, 0);
            } else {
                data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(embeddedTex->pcData), embeddedTex->mWidth * embeddedTex->mHeight * 4, &width, &height, &nrComponents, 0);
            }
        } else {
            // 使用 filesystem 拼接路径
            std::filesystem::path p = std::filesystem::path(texDirectory) / filename;
            data = stbi_load(p.string().c_str(), &width, &height, &nrComponents, 0);
        }

        if (data) {
            GLenum format = GL_RGB;
            if (nrComponents == 1) format = GL_RED;
            else if (nrComponents == 3) format = GL_RGB;
            else if (nrComponents == 4) format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        } else {
            // [关键] 如果加载失败，生成白色纹理，防止黑屏
            std::cout << "[Warning] Missing Texture: " << path << " -> Using WHITE fallback." << std::endl;
            unsigned char whitePixel[] = { 255, 255, 255, 255 };
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        return textureID;
    }

    void Model::computeBoundingBox() {
        if (meshes.empty()) return;
        float minX = 1e9, minY = 1e9, minZ = 1e9;
        float maxX = -1e9, maxY = -1e9, maxZ = -1e9;
        for (const auto& mesh : meshes) {
            for (const auto& vert : mesh.vertices) {
                if (vert.Position.x < minX) minX = vert.Position.x;
                if (vert.Position.y < minY) minY = vert.Position.y;
                if (vert.Position.z < minZ) minZ = vert.Position.z;
                if (vert.Position.x > maxX) maxX = vert.Position.x;
                if (vert.Position.y > maxY) maxY = vert.Position.y;
                if (vert.Position.z > maxZ) maxZ = vert.Position.z;
            }
        }
        boundsMin = glm::vec3(minX, minY, minZ);
        boundsMax = glm::vec3(maxX, maxY, maxZ);
        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        glm::vec3 size = boundsMax - boundsMin;
        float maxDim = std::max(std::max(size.x, size.y), size.z);
        float scaleFactor = 2.0f / maxDim;
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scaleFactor));
        modelMatrix = glm::translate(modelMatrix, -center);
        std::cout << "Model Normalized. Center: (" << center.x << ", " << center.y << ", " << center.z << ") Scale: " << scaleFactor << std::endl;
    }
}