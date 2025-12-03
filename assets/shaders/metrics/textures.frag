#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D tex; // 对应 C++ 代码中的绑定

void main()
{
    FragColor = texture(tex, TexCoords);
}