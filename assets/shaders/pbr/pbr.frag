#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec3 FragNormalMap;

in VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} fs_in;

// --- Textures ---
uniform sampler2D albedoMap;            // Slot 3
uniform sampler2D normalMap;            // Slot 4
uniform sampler2D metallicRoughnessMap; // Slot 5

// --- Switches ---
uniform bool hasAlbedoMap;
uniform bool hasNormalMap;
uniform bool hasMRMap;

// --- Settings ---
uniform int u_ShadingModel; // 0=Lit, 1=Unlit
uniform vec3  u_AlbedoDefault;
uniform float u_RoughnessDefault;
uniform float u_MetallicDefault;

// --- IBL ---
uniform samplerCube irradianceMap; // Slot 0
uniform samplerCube prefilterMap;  // Slot 1
uniform sampler2D   brdfLUT;       // Slot 2
uniform vec3 camPos;

// --- Fixes ---
uniform float u_Exposure;
uniform mat3  u_EnvRotation; // [NEW] Rotate IBL to match UE coordinates

const float PI = 3.14159265359;

// --- PBR Functions ---
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
vec3 ACESFilmicToneMapping(vec3 x) {
    float a = 2.51f; float b = 0.03f; float c = 2.43f; float d = 0.59f; float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    // 1. Albedo
    vec3 albedo;
    if (hasAlbedoMap) {
        vec3 sRGB = texture(albedoMap, fs_in.TexCoords).rgb;
        albedo = pow(sRGB, vec3(2.2));
    } else {
        albedo = u_AlbedoDefault;
    }

    vec3 finalColor = vec3(0.0);
    vec3 N_out = normalize(fs_in.Normal);

    // --- Mode 1: Unlit (OptModel) ---
    if (u_ShadingModel == 1) {
        finalColor = albedo;
        N_out = normalize(fs_in.Normal);
    }
    // --- Mode 0: Lit (RefModel) ---
    else {
        float metallic, roughness;
        if (hasMRMap) {
            vec4 mrSample = texture(metallicRoughnessMap, fs_in.TexCoords);
            roughness = mrSample.g;
            metallic = mrSample.b;
        } else {
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
        N_out = N;

        vec3 V = normalize(camPos - fs_in.WorldPos);
        vec3 R = reflect(-V, N);
        vec3 F0 = vec3(0.04);
        F0 = mix(F0, albedo, metallic);

        vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        // [Fix] Rotate Normal/Reflection vector to match UE HDR coordinates
        vec3 rotN = u_EnvRotation * N;
        vec3 rotR = u_EnvRotation * R;

        vec3 irradiance = texture(irradianceMap, rotN).rgb;
        vec3 diffuse    = irradiance * albedo;

        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, rotR,  roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

        finalColor = (kD * diffuse + specular);
    }

    // --- Post Process ---
    finalColor *= u_Exposure;
    finalColor = ACESFilmicToneMapping(finalColor);
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, 1.0);
    FragNormalMap = (N_out + 1.0) * 0.5;
}