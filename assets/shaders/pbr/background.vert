#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    WorldPos = aPos;

    // 移除位移分量，使天空盒永远跟随相机移动
    mat4 rotView = mat4(mat3(view));
    vec4 clipPos = projection * rotView * vec4(WorldPos, 1.0);

    // 关键优化：将 Z 分量设为 W，透视除法后 Z 就会变成 1.0 (最远深度)
    gl_Position = clipPos.xyww;
}