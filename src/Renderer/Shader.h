#pragma once

namespace Renderer {

    class Shader {
    public:
        unsigned int ID;

        Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr);
        ~Shader();

        void use() const;

        // --- Uniform 工具函数声明 ---
        void setBool(const std::string &name, bool value) const;
        void setInt(const std::string &name, int value) const;
        void setFloat(const std::string &name, float value) const;

        // Vec2
        void setVec2(const std::string &name, const glm::vec2 &value) const;
        void setVec2(const std::string &name, float x, float y) const; // 之前缺少这个

        // Vec3
        void setVec3(const std::string &name, const glm::vec3 &value) const;
        void setVec3(const std::string &name, float x, float y, float z) const; // 之前缺少这个

        // Vec4
        void setVec4(const std::string &name, const glm::vec4 &value) const;
        void setVec4(const std::string &name, float x, float y, float z, float w) const; // 之前缺少这个

        // Matrix
        void setMat2(const std::string &name, const glm::mat2 &mat) const; // 之前缺少这个
        void setMat3(const std::string &name, const glm::mat3 &mat) const;
        void setMat4(const std::string &name, const glm::mat4 &mat) const;

    private:
        void checkCompileErrors(GLuint shader, std::string type);
    };
}