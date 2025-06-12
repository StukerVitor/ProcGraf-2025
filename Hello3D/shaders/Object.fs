#version 450 core

in vec3 fragPos;
in vec2 finalTexCoord;
in vec4 finalColor;
in vec3 scaledNormal;

uniform sampler2D tex;
uniform float kaR, kaG, kaB;
uniform float kdR, kdG, kdB;
uniform float ksR, ksG, ksB;
uniform float ns;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;
uniform vec3 extraColor;
uniform int skipLighting;

out vec4 color;

void main() {
    if (skipLighting == 1) {
        vec4 finalTexture = texture(tex, finalTexCoord);
        color = vec4(mix(vec3(finalTexture), extraColor, 0.2), 1.0); 
    }
    else {
        vec3 ambient = vec3(kaR, kaG, kaB) * lightColor;
        vec3 N = normalize(scaledNormal);
        vec3 L = normalize(lightPos - fragPos);
        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = vec3(kdR, kdG, kdB) * diff * lightColor;
        vec3 V = normalize(cameraPos - fragPos);
        vec3 R = normalize(reflect(-L, N));
        float spec = max(dot(R, V), 0.0);
        spec = pow(spec, ns);
        vec3 specular = vec3(ksR, ksG, ksB) * spec * lightColor;
        vec4 finalTexture = texture(tex, finalTexCoord);
        vec3 result = (ambient + diffuse) * vec3(finalTexture) + specular;
        color = vec4(mix(result, extraColor, 0.2), 1.0);
    }
}
