#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform vec2 texelSize;

// 1. 深度线性化
float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    float near = 0.1;
    float far  = 100.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

// 2. 检测是否为背景 (对应 C++ 中的 IsBackgroundDepth)
bool IsBackgroundDepth(float d) {
    return d >= 0.99999;
}

// 3. 将 RGB 颜色还原为真实的几何法线 (对应 C++ 中的 DecodeNormal01ToWorld)
vec3 DecodeNormal(vec2 uv) {
    vec3 n = texture(normalMap, uv).rgb;
    return normalize(n * 2.0 - 1.0);
}

void main() {
    // 获取中心点原始深度
    float dC_raw = texture(depthMap, TexCoords).r;

    // 如果中心点本身是天空背景，直接输出黑色（对应 C++ 中 silhouette[idx] = 0; continue;）
    if (IsBackgroundDepth(dC_raw)) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // 获取四周的原始深度
    float dL_raw = texture(depthMap, TexCoords + vec2(-texelSize.x, 0.0)).r;
    float dR_raw = texture(depthMap, TexCoords + vec2( texelSize.x, 0.0)).r;
    float dT_raw = texture(depthMap, TexCoords + vec2(0.0,  texelSize.y)).r;
    float dB_raw = texture(depthMap, TexCoords + vec2(0.0, -texelSize.y)).r;

    // 核心黑科技：如果周围像素是背景，强行增加 10000 深度，保证外轮廓绝对断层
    float dc = LinearizeDepth(dC_raw);
    float dL = IsBackgroundDepth(dL_raw) ? (dc + 10000.0) : LinearizeDepth(dL_raw);
    float dR = IsBackgroundDepth(dR_raw) ? (dc + 10000.0) : LinearizeDepth(dR_raw);
    float dT = IsBackgroundDepth(dT_raw) ? (dc + 10000.0) : LinearizeDepth(dT_raw);
    float dB = IsBackgroundDepth(dB_raw) ? (dc + 10000.0) : LinearizeDepth(dB_raw);

    // 计算中心与四周的最大深度梯度 gd
    float gd = 0.0;
    gd = max(gd, abs(dc - dL));
    gd = max(gd, abs(dc - dR));
    gd = max(gd, abs(dc - dT));
    gd = max(gd, abs(dc - dB));

    // 获取法线
    vec3 nC = DecodeNormal(TexCoords);
    vec3 nL = DecodeNormal(TexCoords + vec2(-texelSize.x, 0.0));
    vec3 nR = DecodeNormal(TexCoords + vec2( texelSize.x, 0.0));
    vec3 nT = DecodeNormal(TexCoords + vec2(0.0,  texelSize.y));
    vec3 nB = DecodeNormal(TexCoords + vec2(0.0, -texelSize.y));

    // 计算中心与四周的法线夹角偏差 gn (对应 C++ 中的 1.0f - dot)
    float gn = 0.0;
    gn = max(gn, 1.0 - clamp(dot(nC, nL), -1.0, 1.0));
    gn = max(gn, 1.0 - clamp(dot(nC, nR), -1.0, 1.0));
    gn = max(gn, 1.0 - clamp(dot(nC, nT), -1.0, 1.0));
    gn = max(gn, 1.0 - clamp(dot(nC, nB), -1.0, 1.0));

    // 严格使用 C++ 默认的超高精度阈值 (depthThresh: 0.01, normalThresh: 0.1)
    float isSilhouette = 0.0;
    if (gd > 0.01 || gn > 0.1) {
        isSilhouette = 1.0;
    }

    FragColor = vec4(vec3(isSilhouette), 1.0);
}