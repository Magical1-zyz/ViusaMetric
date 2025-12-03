#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D texRef;  // 原始模型渲染图 (Color 或 Normal)
uniform sampler2D texBase; // 简化模型渲染图
uniform int errorType;     // 0 = Color (PSNR), 1 = Normal, 2 = Silhouette

// 热力图映射函数: 0.0(蓝) -> 0.5(绿) -> 1.0(红)
vec3 valueToHeatmap(float value) {
    value = clamp(value, 0.0, 1.0);
    float r = smoothstep(0.5, 0.8, value); // 0.5以上开始变红
    float g = sin(value * 3.14159);        // 中间是绿色
    float b = smoothstep(0.5, 0.2, value); // 0.5以下是蓝色
    return vec3(r, g, b);
}

void main() {
    vec4 refVal = texture(texRef, TexCoords);
    vec4 baseVal = texture(texBase, TexCoords);
    float diff = 0.0;

    if (errorType == 0) {
        // --- Color Difference (RGB) ---
        // 计算欧氏距离
        diff = distance(refVal.rgb, baseVal.rgb);
        // 稍微放大差异以便观察 (Scale factor)
        diff *= 5.0;
    }
    else if (errorType == 1) {
        // --- Normal Difference ---
        // 假设输入的是 [0,1] 的法线贴图，先解码回 [-1,1]
        vec3 n1 = normalize(refVal.rgb * 2.0 - 1.0);
        vec3 n2 = normalize(baseVal.rgb * 2.0 - 1.0);
        // 计算角度差 (1 - dot) / 2
        diff = (1.0 - dot(n1, n2)) * 2.0; // 放大系数
    }
    else if (errorType == 2) {
        // --- Silhouette Difference ---
        // 假设输入是二值图 (0 或 1)
        diff = abs(refVal.r - baseVal.r);
    }

    // 输出热力图颜色
    FragColor = vec4(valueToHeatmap(diff), 1.0);
}