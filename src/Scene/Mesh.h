#pragma once


namespace Scene {
    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoords;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
    };
    struct Texture {
        unsigned int id;
        std::string type;
        std::string path;
    };
    struct MaterialProps {
        glm::vec4 baseColor = glm::vec4(1.0f);
        float metallic = 0.0f;
        float roughness = 0.5f;
    };

    class Mesh {
    public:
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;
        MaterialProps             matProps;
        unsigned int VAO;

        Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures, MaterialProps props) {
            this->vertices = vertices;
            this->indices = indices;
            this->textures = textures;
            this->matProps = props;
            setupMesh();
        }

        void Draw(unsigned int shaderProgram) {
            const unsigned int SLOT_ALBEDO = 3;
            const unsigned int SLOT_NORMAL = 4;
            const unsigned int SLOT_MR     = 5;

            bool hasAlbedo = false;
            bool hasNormal = false;
            bool hasMR     = false;

            for(unsigned int i = 0; i < textures.size(); i++) {
                std::string name = textures[i].type;
                if(name == "albedoMap") {
                    glActiveTexture(GL_TEXTURE0 + SLOT_ALBEDO);
                    glBindTexture(GL_TEXTURE_2D, textures[i].id);
                    glUniform1i(glGetUniformLocation(shaderProgram, "albedoMap"), SLOT_ALBEDO);
                    hasAlbedo = true;
                } else if(name == "normalMap") {
                    glActiveTexture(GL_TEXTURE0 + SLOT_NORMAL);
                    glBindTexture(GL_TEXTURE_2D, textures[i].id);
                    glUniform1i(glGetUniformLocation(shaderProgram, "normalMap"), SLOT_NORMAL);
                    hasNormal = true;
                } else if(name == "metallicRoughnessMap") {
                    glActiveTexture(GL_TEXTURE0 + SLOT_MR);
                    glBindTexture(GL_TEXTURE_2D, textures[i].id);
                    glUniform1i(glGetUniformLocation(shaderProgram, "metallicRoughnessMap"), SLOT_MR);
                    hasMR = true;
                }
            }

            // 显式更新状态，防止残留
            glUniform1i(glGetUniformLocation(shaderProgram, "hasAlbedoMap"), hasAlbedo);
            glUniform1i(glGetUniformLocation(shaderProgram, "hasNormalMap"), hasNormal);
            glUniform1i(glGetUniformLocation(shaderProgram, "hasMRMap"), hasMR);

            // 传递兜底颜色
            glUniform3f(glGetUniformLocation(shaderProgram, "u_AlbedoDefault"), matProps.baseColor.r, matProps.baseColor.g, matProps.baseColor.b);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_RoughnessDefault"), matProps.roughness);
            glUniform1f(glGetUniformLocation(shaderProgram, "u_MetallicDefault"), matProps.metallic);

            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glActiveTexture(GL_TEXTURE0);
        }

    private:
        unsigned int VBO, EBO;
        void setupMesh() {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
            glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
            glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
            glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
            glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
            glBindVertexArray(0);
        }
    };
}