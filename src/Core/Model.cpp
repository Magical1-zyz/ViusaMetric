#include "Model.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <algorithm>

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
        // 关键标志位：
        // aiProcess_Triangulate: 也就是必须是三角形
        // aiProcess_FlipUVs: OpenGL 纹理坐标系处理
        // aiProcess_CalcTangentSpace: PBR 需要切线空间计算法线贴图
        const aiScene* scene = importer.ReadFile(path,
                                                 aiProcess_Triangulate |
                                                 aiProcess_GenSmoothNormals |
                                                 aiProcess_FlipUVs |
                                                 aiProcess_CalcTangentSpace |
                                                 aiProcess_JoinIdenticalVertices);

        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        // 保存 scene 指针供后续纹理加载使用（针对内嵌纹理）
        // 注意：在 loadModel 结束后 importer 析构，scene 也会失效。
        // 但我们在 processNode 过程中就会完成所有数据拷贝，所以是安全的。
        // 如果需要在类生命周期外访问 scene，必须拷贝或持久化 importer。
        // 这里我们只在函数内部遍历，所以没问题。
        this->scenePtr = scene;

        directory = path.substr(0, path.find_last_of('/'));
        // Windows 兼容
        if (directory == path) directory = path.substr(0, path.find_last_of('\\'));

        processNode(scene->mRootNode, scene);
    }

    void Model::processNode(aiNode *node, const aiScene *scene) {
        // 处理节点所有的网格
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // 递归处理子节点
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        // 1. 处理顶点
        for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 vector;

            // Position
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            // Normal
            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }

            // TexCoords
            if(mesh->mTextureCoords[0]) {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;

                // Tangent
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

        // 2. 处理索引
        for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // 3. 处理材质
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // PBR 工作流贴图映射
        // 1. Albedo / Base Color
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 注意：某些 GLTF 导出器将 BaseColor 放在 aiTextureType_BASE_COLOR (Assimp 5.1+)
        // 如果是较旧 Assimp，GLTF 的 BaseColor 经常映射到 DIFFUSE

        // 2. Normal
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

        // 3. Metallic Roughness
        // GLTF 标准中，Metallic 和 Roughness 打包在同一张图里 (B=Metal, G=Roughness)
        // Assimp 通常将其加载为 aiTextureType_UNKNOWN (index 0) 或者 aiTextureType_METALNESS (新版)
        // 这里我们先尝试 UNKNOWN，如果是 GLTF 通常在 UNKNOWN[0]
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
                    skip = true;
                    break;
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

        // --- 检查是否为嵌入式纹理 (GLB) ---
        const aiTexture* embeddedTex = nullptr;
        if (path[0] == '*') {
            // 格式如 "*0", "*1", 解析索引
            int index = std::stoi(filename.substr(1));
            if (index < scene->mNumTextures) {
                embeddedTex = scene->mTextures[index];
            }
        } else {
            // 有些时候 Assimp 会将嵌入纹理路径设为文件名，但数据仍在 mTextures 中
            // 这种情况比较少见，这里主要处理 path 以 '*' 开头的情况
            // 或者直接按文件加载
        }

        if (embeddedTex) {
            // --- 加载嵌入式数据 ---
            if (embeddedTex->mHeight == 0) {
                // 这是一个压缩纹理 (jpg/png 等)，使用 stb_image 从内存加载
                data = stbi_load_from_memory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        embeddedTex->mWidth,
                        &width, &height, &nrComponents, 0);
            } else {
                // 这是一个原始的 ARGB8888 数据
                data = stbi_load_from_memory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        embeddedTex->mWidth * embeddedTex->mHeight * 4, // 估算大小
                        &width, &height, &nrComponents, 0);
            }
        } else {
            // --- 加载普通文件 ---
            filename = texDirectory + '/' + filename;
            data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
        }

        if (data) {
            GLenum format;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 3)
                format = GL_RGB;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        } else {
            std::cout << "Texture failed to load at path: " << path << std::endl;
            stbi_image_free(data);
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

        // 计算中心点
        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;

        // 计算尺寸
        glm::vec3 size = boundsMax - boundsMin;

        // 找出最大的一边
        float maxDim = std::max(std::max(size.x, size.y), size.z);

        // 计算缩放因子：目标半径为1，直径为2，所以缩放到 2.0/maxDim
        // 这样整个模型会被限制在 [-1, 1] 盒子内
        float scaleFactor = 2.0f / maxDim;

        // 构建模型矩阵：先平移回原点，再缩放
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(scaleFactor));
        modelMatrix = glm::translate(modelMatrix, -center);

        std::cout << "Model Normalized. Center: (" << center.x << ", " << center.y << ", " << center.z
                  << ") Scale: " << scaleFactor << std::endl;
    }
}