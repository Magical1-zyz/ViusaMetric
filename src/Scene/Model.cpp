#include "Scene/Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Scene {

    // ---- 小工具：用 std::isfinite 做分量检查（不依赖 glm::isfinite）----
    static inline bool IsFiniteVec3(const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    Model::Model(std::string const &path) {
        stbi_set_flip_vertically_on_load(false);
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

        // ✅ 关键：不要强制 GenSmoothNormals，否则 ref/opt 只要 mesh 切分不同就可能重建出不同法线
        // 只 Triangulate / FlipUVs / CalcTangentSpace 等保持一致即可
        const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_Triangulate |
                aiProcess_FlipUVs |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices
        );

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        // ⚠️ 注意：Assimp::Importer 在函数结束会释放 scene，所以这里存 scenePtr 是危险的。
        // 你原工程里确实这么做了。严格做法是：把 importer 作为成员保存，或把需要的数据拷贝出来。
        // 这里为了“不改变你工程结构”，仍沿用原逻辑，但建议你后续把 importer 变成 Model 的成员。
        this->scenePtr = scene;

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
        MaterialProps matProps;

        // 1) 顶点
        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;

            glm::vec3 v;
            v.x = mesh->mVertices[i].x;
            v.y = mesh->mVertices[i].y;
            v.z = mesh->mVertices[i].z;
            vertex.Position = v;

            if (mesh->HasNormals()) {
                v.x = mesh->mNormals[i].x;
                v.y = mesh->mNormals[i].y;
                v.z = mesh->mNormals[i].z;
                vertex.Normal = v;
            } else {
                // 没法线才给一个兜底
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if(mesh->mTextureCoords[0]) {
                glm::vec2 uv;
                uv.x = mesh->mTextureCoords[0][i].x;
                uv.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = uv;

                if (mesh->HasTangentsAndBitangents()) {
                    v.x = mesh->mTangents[i].x;
                    v.y = mesh->mTangents[i].y;
                    v.z = mesh->mTangents[i].z;
                    vertex.Tangent = v;

                    v.x = mesh->mBitangents[i].x;
                    v.y = mesh->mBitangents[i].y;
                    v.z = mesh->mBitangents[i].z;
                    vertex.Bitangent = v;
                }
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        // 2) 索引
        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // 3) 材质/纹理（保持你原来的逻辑）
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            bool hasAlbedo = false;

            // BaseColor -> Diffuse -> Emissive
            std::vector<Texture> baseMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "albedoMap");
            if(!baseMaps.empty()) {
                textures.insert(textures.end(), baseMaps.begin(), baseMaps.end());
                hasAlbedo = true;
            }

            if(!hasAlbedo) {
                std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap");
                if(!diffuseMaps.empty()) {
                    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
                    hasAlbedo = true;
                }
            }

            if(!hasAlbedo) {
                std::vector<Texture> emissiveMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "albedoMap");
                if(!emissiveMaps.empty()) {
                    textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
                    hasAlbedo = true;
                    std::cout << "[Model] Found texture in EMISSIVE channel." << std::endl;
                }
            }

            std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap");
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

            std::vector<Texture> armMaps = loadMaterialTextures(material, aiTextureType_UNKNOWN, "metallicRoughnessMap");
            textures.insert(textures.end(), armMaps.begin(), armMaps.end());

            aiColor4D color;
            if (AI_SUCCESS == material->Get(AI_MATKEY_BASE_COLOR, color)) {
                matProps.baseColor = glm::vec4(color.r, color.g, color.b, color.a);
            } else if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
                matProps.baseColor = glm::vec4(color.r, color.g, color.b, color.a);
            }

            float val;
            if (AI_SUCCESS == material->Get(AI_MATKEY_METALLIC_FACTOR, val)) matProps.metallic = val;
            if (AI_SUCCESS == material->Get(AI_MATKEY_ROUGHNESS_FACTOR, val)) matProps.roughness = val;
        }

        return Mesh(vertices, indices, textures, matProps);
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

    unsigned int Model::TextureFromFile(const char *path, const std::string &texDirectory, const aiScene* scene) {
        std::string filename = std::string(path);
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        unsigned char *data = nullptr;

        const aiTexture* embeddedTex = nullptr;
        if (path[0] == '*') {
            size_t index = std::stoi(filename.substr(1));
            if (scene && index < scene->mNumTextures) embeddedTex = scene->mTextures[index];
        }

        if (embeddedTex) {
            if (embeddedTex->mHeight == 0) {
                data = stbi_load_from_memory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        embeddedTex->mWidth,
                        &width, &height, &nrComponents, 0
                );
            } else {
                data = stbi_load_from_memory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        embeddedTex->mWidth * embeddedTex->mHeight * 4,
                        &width, &height, &nrComponents, 0
                );
            }
        } else {
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
            std::cout << "[Error] Texture failed to load: " << path << std::endl;
            unsigned char pink[] = { 255, 0, 255, 255 };
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
        }
        return textureID;
    }

    void Model::computeBoundingBox() {
        if (meshes.empty()) return;

        float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
        float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;

        for (const auto& mesh : meshes) {
            for (const auto& vert : mesh.vertices) {
                minX = std::min(minX, vert.Position.x);
                minY = std::min(minY, vert.Position.y);
                minZ = std::min(minZ, vert.Position.z);

                maxX = std::max(maxX, vert.Position.x);
                maxY = std::max(maxY, vert.Position.y);
                maxZ = std::max(maxZ, vert.Position.z);
            }
        }

        boundsMin = glm::vec3(minX, minY, minZ);
        boundsMax = glm::vec3(maxX, maxY, maxZ);

        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        glm::vec3 size = boundsMax - boundsMin;

        // ✅ 这里用 std::isfinite 的分量检查，避免 glm::isfinite 兼容性问题
        if (!IsFiniteVec3(center) || !IsFiniteVec3(size)) {
            std::cout << "[Model] computeBoundingBox got non-finite values, skip normalization." << std::endl;
            modelMatrix = glm::mat4(1.0f);
            return;
        }

        float maxDim = std::max(std::max(size.x, size.y), size.z);
        if (!std::isfinite(maxDim) || maxDim <= 1e-8f) {
            std::cout << "[Model] maxDim invalid, skip normalization." << std::endl;
            modelMatrix = glm::mat4(1.0f);
            return;
        }

        float scaleFactor = 2.0f / maxDim;

        // 注意：矩阵顺序保持你原逻辑（先 scale 再 translate）
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scaleFactor));
        modelMatrix = glm::translate(modelMatrix, -center);

        std::cout << "Model Normalized. Center: " << center.x << " " << center.y << " " << center.z
                  << " Scale: " << scaleFactor << std::endl;
    }

} // namespace Scene
