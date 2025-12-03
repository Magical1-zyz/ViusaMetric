#version 330 core
// MRT 输出：0=颜色图(用于PSNR), 1=法线图(用于ND和SD)
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormalMap;

in VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} fs_in;

// --- 材质输入 ---
// 贴图
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicRoughnessMap; // glTF通常是 G=Roughness, B=Metallic

// 开关与默认值 (用于 Base 模型)
uniform bool hasNormalMap;
uniform bool hasMRMap;      // 是否有金属/粗糙度贴图

uniform vec3  u_AlbedoDefault;    // 如果没有 albedo 贴图时的颜色
uniform float u_RoughnessDefault; // Base 模型建议设为 0.9-1.0
uniform float u_MetallicDefault;  // Base 模型建议设为 0.0

// --- IBL 输入 (由预处理阶段生成) ---
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;

uniform vec3 camPos;

const float PI = 3.14159265359;

// --- PBR 辅助函数 ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // 1. 获取材质属性
    vec3 albedo = texture(albedoMap, fs_in.TexCoords).rgb;
    // 如果是 Base 模型，albedoMap 依然有效，但 MR 和 Normal 可能无效

    float metallic;
    float roughness;

    if (hasMRMap) {
        // glTF 标准: B=Metallic, G=Roughness
        vec4 mrSample = texture(metallicRoughnessMap, fs_in.TexCoords);
        roughness = mrSample.g;
        metallic = mrSample.b;
    } else {
        // Base 模型使用 Uniform 默认值
        roughness = u_RoughnessDefault;
        metallic = u_MetallicDefault;
    }

    vec3 N;
    if (hasNormalMap) {
        vec3 tangentNormal = texture(normalMap, fs_in.TexCoords).rgb;
        tangentNormal = tangentNormal * 2.0 - 1.0;
        N = normalize(fs_in.TBN * tangentNormal);
    } else {
        N = normalize(fs_in.Normal);
    }

    vec3 V = normalize(camPos - fs_in.WorldPos);
    vec3 R = reflect(-V, N);

    // 2. IBL 计算 (环境光照)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    // 漫反射部分 (Irradiance)
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    // 镜面反射部分 (Prefilter + LUT)
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    // 简单组合 AO (这里假设没有AO贴图，设为1)
    float ao = 1.0;
    vec3 ambient = (kD * diffuse + specular) * ao;

    vec3 color = ambient;

    // HDR Tone Mapping & Gamma Correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    // --- Output 0: PBR 图像 ---
    FragColor = vec4(color, 1.0);

    // --- Output 1: 归一化法线图 ---
    // 映射 [-1, 1] -> [0, 1]
    FragNormalMap = (N + 1.0) * 0.5;
}