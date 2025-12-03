#version 330 core
layout (location = 0) out vec4 FragColor;
// 注意：背景通常不需要输出法线信息，或者输出 0 向量
layout (location = 1) out vec3 FragNormal;

in vec3 WorldPos;

uniform samplerCube environmentMap;

void main()
{
    vec3 envColor = texture(environmentMap, WorldPos).rgb;

    // --- 色调映射 (Tone Mapping) ---
    // 必须与 pbr.frag 中的算法保持一致，否则物体和背景亮度会脱节
    envColor = envColor / (envColor + vec3(1.0)); // Reinhard

    // --- Gamma 校正 ---
    envColor = pow(envColor, vec3(1.0/2.2));

    FragColor = vec4(envColor, 1.0);

    // 背景没有法线，输出黑色或默认值即可
    FragNormal = vec3(0.0, 0.0, 0.0);
}