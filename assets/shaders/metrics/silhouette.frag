#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
// uniform sampler2D normalMap; // 屏蔽法线图
uniform vec2 texelSize;

float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    float near = 0.1;
    float far  = 100.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main()
{
    // 仅使用深度梯度检测轮廓
    float dC = LinearizeDepth(texture(depthMap, TexCoords).r);
    float dL = LinearizeDepth(texture(depthMap, TexCoords + vec2(-texelSize.x, 0.0)).r);
    float dR = LinearizeDepth(texture(depthMap, TexCoords + vec2( texelSize.x, 0.0)).r);
    float dT = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0,  texelSize.y)).r);
    float dB = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0, -texelSize.y)).r);

    float grad_depth = abs(dL - dR) + abs(dT - dB);

    // 阈值判定
    float isSilhouette = (grad_depth > 0.05) ? 1.0 : 0.0;

    FragColor = vec4(vec3(isSilhouette), 1.0);
}