#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
// 根据模型格式，可能还有切线
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool useNormalMap; // 开关：是否计算 TBN 矩阵

void main()
{
    vs_out.TexCoords = aTexCoords;
    vs_out.WorldPos = vec3(model * vec4(aPos, 1.0));

    // 法线矩阵处理非均匀缩放
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vs_out.Normal = normalize(normalMatrix * aNormal);

    // 如果有法线贴图，计算切线空间
    if(useNormalMap) {
        vec3 T = normalize(normalMatrix * aTangent);
        vec3 B = normalize(normalMatrix * aBitangent);
        vec3 N = normalize(normalMatrix * aNormal);
        vs_out.TBN = mat3(T, B, N);
    } else {
        // 占位，防止编译警告
        vs_out.TBN = mat3(1.0);
    }

    gl_Position = projection * view * vec4(vs_out.WorldPos, 1.0);
}