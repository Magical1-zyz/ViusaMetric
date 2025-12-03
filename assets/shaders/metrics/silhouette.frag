#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// 输入：从 PBR 阶段生成的纹理
uniform sampler2D depthMap;
uniform sampler2D normalMap;

// 参数：纹理的纹素大小 (1.0 / width, 1.0 / height)
uniform vec2 texelSize;

// 阈值：需要根据模型尺度在 C++端进行微调
uniform float depthThreshold;   // 建议值: 0.02 ~ 0.1 (取决于场景缩放)
uniform float normalThreshold;  // 建议值: 0.1 ~ 0.5 (法线向量差的模长)

// 线性化深度 (透视投影需要，正交投影不需要)
// 假设 Near=0.1, Far=100.0，需与 C++ 相机设置一致
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    float near = 0.1;
    float far  = 100.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

void main()
{
    // 索贝尔(Sobel)算子或简单的十字交叉采样计算梯度
    // 这里采用文中描述的简单相邻像素差值

    // 1. 采样当前像素及上下左右邻域
    // Center, Left, Right, Top, Bottom
    // 深度值
    float dC = LinearizeDepth(texture(depthMap, TexCoords).r);
    float dL = LinearizeDepth(texture(depthMap, TexCoords + vec2(-texelSize.x, 0.0)).r);
    float dR = LinearizeDepth(texture(depthMap, TexCoords + vec2( texelSize.x, 0.0)).r);
    float dT = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0,  texelSize.y)).r);
    float dB = LinearizeDepth(texture(depthMap, TexCoords + vec2(0.0, -texelSize.y)).r);

    // 法线值 (解码 [0,1] -> [-1,1])
    vec3 nC = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;
    vec3 nL = texture(normalMap, TexCoords + vec2(-texelSize.x, 0.0)).rgb * 2.0 - 1.0;
    vec3 nR = texture(normalMap, TexCoords + vec2( texelSize.x, 0.0)).rgb * 2.0 - 1.0;
    vec3 nT = texture(normalMap, TexCoords + vec2(0.0,  texelSize.y)).rgb * 2.0 - 1.0;
    vec3 nB = texture(normalMap, TexCoords + vec2(0.0, -texelSize.y)).rgb * 2.0 - 1.0;

    // 2. 计算梯度
    // 深度梯度：水平差绝对值 + 垂直差绝对值
    float grad_depth = abs(dL - dR) + abs(dT - dB);

    // 法线梯度：向量差的模长
    float grad_normal = length(nL - nR) + length(nT - nB);

    // 3. 阈值判定与二值化
    // 如果任一梯度超过阈值，则该像素为轮廓 (1.0)，否则为背景 (0.0)
    float isSilhouette = 0.0;
    if (grad_depth > depthThreshold || grad_normal > normalThreshold) {
        isSilhouette = 1.0;
    }

    // 输出黑白图像 (White=Edge, Black=Background)
    FragColor = vec4(vec3(isSilhouette), 1.0);
}