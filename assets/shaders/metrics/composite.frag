#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D texRef; // 输入已经是 sRGB (Ready to Display)
uniform sampler2D texOpt; // 输入已经是 sRGB (Ready to Display)
uniform int viewMode;     // 0=Left, 1=Middle, 2=Right

vec3 jet(float v) {
    float r = clamp(1.5 - abs(4.0 * v - 3.0), 0.0, 1.0);
    float g = clamp(1.5 - abs(4.0 * v - 2.0), 0.0, 1.0);
    float b = clamp(1.5 - abs(4.0 * v - 1.0), 0.0, 1.0);
    return vec3(r, g, b);
}

void main() {
    vec3 color = vec3(0.0);

    // --- 0: 左侧视图 (Ref) ---
    if (viewMode == 0) {
        // [修复过曝] 直接采样输出，不做任何 Gamma/ToneMap
        color = texture(texRef, TexCoords).rgb;
    }
    // --- 1: 中间视图 (Opt) ---
    else if (viewMode == 1) {
        // [修复过曝] 直接采样输出
        color = texture(texOpt, TexCoords).rgb;
    }
    // --- 2: 右侧热力图 ---
    else if (viewMode == 2) {
        vec4 c1 = texture(texRef, TexCoords);
        vec4 c2 = texture(texOpt, TexCoords);

        // 剔除背景 (Alpha < 0.1)
        if (c1.a < 0.1 || c2.a < 0.1) {
            color = vec3(0.0);
        } else {
            // 计算 sRGB 空间的感知差异
            float diff = length(c1.rgb - c2.rgb);
            // 放大误差以便观察 (x5.0)，并截断到 0-1
            float val = clamp(diff * 5.0, 0.0, 1.0);
            color = jet(val);
        }
    }

    FragColor = vec4(color, 1.0);
}