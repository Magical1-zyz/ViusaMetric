#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormalMap;

in vec3 Normal;

uniform int u_VisMode; // 1 = Normal (法线), 2 = Silhouette (轮廓)

void main() {
    // 归一化插值后的法线
    vec3 N = normalize(Normal);

    if (u_VisMode == 1) {
        // --- 法线模式 ---
        // 将 [-1, 1] 映射到 [0, 1]
        vec3 c = (N + 1.0) * 0.5;
        FragColor = vec4(c, 1.0);     // 屏幕显示
        FragNormalMap = c;            // 数据输出
    }
    else {
        // --- 轮廓模式 ---
        // 输出纯白，配合黑色背景形成二值图
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);

        // 保持法线数据有效（虽然 SD 计算可能只用 Color 附件，但填充它是个好习惯）
        FragNormalMap = (N + 1.0) * 0.5;
    }
}