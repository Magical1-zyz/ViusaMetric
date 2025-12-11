#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // 将法线变换到世界空间
    // 如果只有统一缩放，直接用 mat3(model) 即可；若有非统一缩放，需用 transpose(inverse(mat3(model)))
    // 这里为了通用性，我们假设 C++ 端或者驱动会处理好，或者简化为 mat3(model)
    Normal = mat3(model) * aNormal;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}