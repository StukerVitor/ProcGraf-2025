#version 450 core
out vec4 FragColor;
uniform vec4 finalColor;
void main() {
    FragColor = finalColor;
}