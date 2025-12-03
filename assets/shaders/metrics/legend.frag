#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

vec3 valueToHeatmap(float value) {
    value = clamp(value, 0.0, 1.0);
    float r = smoothstep(0.5, 0.8, value);
    float g = sin(value * 3.14159);
    float b = smoothstep(0.5, 0.2, value);
    return vec3(r, g, b);
}

void main() {
    // 根据纹理坐标的 Y 值直接生成渐变
    FragColor = vec4(valueToHeatmap(TexCoords.y), 1.0);
}