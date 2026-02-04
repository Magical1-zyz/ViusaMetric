#version 330 core
layout (location = 0) out vec4 FragColor;   // 输出到 colorTex
layout (location = 1) out vec3 FragNormal;  // 输出到 normalTex

in VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

void main()
{
    // 1. 颜色输出：纯白模式
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);

    // 2. 法线输出：纯几何法线
    // 归一化插值后的顶点法线
    vec3 N = normalize(fs_in.Normal);

    // 映射到 [0, 1] 范围存储
    FragNormal = N * 0.5 + 0.5;
}