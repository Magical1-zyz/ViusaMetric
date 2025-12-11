#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D texRef;
uniform sampler2D texBase;
uniform int errorType;

// 热力图颜色映射: 0.0(蓝/好) -> 1.0(红/差)
vec3 valueToHeatmap(float value) {
    value = clamp(value, 0.0, 1.0);
    float r = smoothstep(0.5, 0.8, value);
    float g = sin(value * 3.14159);
    float b = smoothstep(0.5, 0.2, value);
    return vec3(r, g, b);
}

void main() {
    vec3 ref = texture(texRef, TexCoords).rgb;
    vec3 base = texture(texBase, TexCoords).rgb;

    float mapValue = 0.0;

    if (errorType == 0) { // PSNR (Color) Mode
        // 1. 计算均方误差 (MSE)
        vec3 diffVec = ref - base;
        float mse = dot(diffVec, diffVec) / 3.0; // r^2 + g^2 + b^2 平均值

        // 2. 处理完美匹配的情况 (避免 log(0))
        // 如果 MSE 极小，认为 PSNR 达到上限 (例如 60dB)
        if (mse < 0.0000001) {
            mapValue = 0.0; // 纯蓝
        } else {
            // 3. 计算 PSNR (假设最大像素值是 1.0)
            // PSNR = 10 * log10(MAX^2 / MSE)
            // GLSL log() 是自然对数，所以 log10(x) = log(x) / 2.30258
            float psnr = -10.0 * log(mse) / 2.302585;

            // 4. 根据文献范围映射颜色
            // 设定范围: [20dB (红) ~ 50dB (蓝)]
            float min_dB = 20.0;
            float max_dB = 50.0;

            // 归一化 psnr 到 [0, 1] (0是20dB, 1是50dB)
            float normPSNR = clamp((psnr - min_dB) / (max_dB - min_dB), 0.0, 1.0);

            // 反转: 高PSNR(1.0) -> 蓝色(value=0.0)
            mapValue = 1.0 - normPSNR;
        }
    }
    else if (errorType == 1) {
        // 法线误差保持线性 (ND)
        vec3 n1 = normalize(ref * 2.0 - 1.0);
        vec3 n2 = normalize(base * 2.0 - 1.0);
        mapValue = (1.0 - dot(n1, n2)); // 范围 0~2，通常很小
        mapValue *= 5.0; // 适当放大以便观察
    }
    else {
        // 轮廓误差 (SD)
        mapValue = abs(ref.r - base.r);
    }

    FragColor = vec4(valueToHeatmap(mapValue), 1.0);
}