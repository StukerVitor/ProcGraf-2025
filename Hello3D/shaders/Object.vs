#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 color;
layout (location = 3) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragPos;
out vec4 finalColor;
out vec2 finalTexCoord;
out vec3 scaledNormal;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragPos = vec3(model * vec4(position, 1.0));
    finalColor = vec4(color, 1.0);
    finalTexCoord = texCoord;
    scaledNormal = mat3(transpose(inverse(model))) * normal;
}