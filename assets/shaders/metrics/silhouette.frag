#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform vec2 texelSize;

// 1. 严格对应论文中的 "线性深度图"
float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    float near = 0.1;
    float far  = 100.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main()
{
    // 获取线性深度
    float depthLeft  = LinearizeDepth(texture(depthMap, TexCoords + vec2(-texelSize.x, 0.0)).r);
    float depthRight = LinearizeDepth(texture(depthMap, TexCoords + vec2( texelSize.x, 0.0)).r);
    float depthUp    = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0,  texelSize.y)).r);
    float depthDown  = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0, -texelSize.y)).r);

    // 获取法线
    vec3 normalLeft  = normalize(texture(normalMap, TexCoords + vec2(-texelSize.x, 0.0)).rgb);
    vec3 normalRight = normalize(texture(normalMap, TexCoords + vec2( texelSize.x, 0.0)).rgb);
    vec3 normalUp    = normalize(texture(normalMap, TexCoords + vec2(0.0,  texelSize.y)).rgb);
    vec3 normalDown  = normalize(texture(normalMap, TexCoords + vec2(0.0, -texelSize.y)).rgb);

    // 2. 计算相邻像素深度的绝对差值
    float depthGradientX = abs(depthRight - depthLeft);
    float depthGradientY = abs(depthUp - depthDown);

    // 3. 对于法线图，计算差值模长
    float normalGradientX = length(normalRight - normalLeft);
    float normalGradientY = length(normalUp - normalDown);

    // 设定自适应阈值 (由于深度变成了线性，阈值需要调大一点，比如0.1；法线阈值保持0.3左右)
    float depthThreshold = 0.1;
    float normalThreshold = 0.3;

    // 4. 当深度梯度 *或* 法线梯度超过临界值时
    float isSilhouette = 0.0;
    if ((depthGradientX > depthThreshold || depthGradientY > depthThreshold) ||
    (normalGradientX > normalThreshold || normalGradientY > normalThreshold)) {
        isSilhouette = 1.0;
    }

    FragColor = vec4(vec3(isSilhouette), 1.0);
}