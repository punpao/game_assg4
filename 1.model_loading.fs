#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

void main() {
    vec4 base = texture(texture_diffuse1, TexCoords);
    // slight gamma-ish lift for visibility on dark rock “ground”
    FragColor = vec4(pow(base.rgb, vec3(1.0/1.1)), base.a);
}
