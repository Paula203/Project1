#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec4 baseColor;

void main()
{
    vec3 colorObjeto = texture(texture1, TexCoord).rgb * baseColor.rgb;

    vec3 N = normalize(Normal);
    if (!gl_FrontFacing) N = -N;
    vec3 L = normalize(lightPos - FragPos);   // hacia la luz
    vec3 V = normalize(viewPos - FragPos);    // hacia la camara
    vec3 R = reflect(-L, N);                  // rayo reflejado

    // MODELO DE PHONG = ambiente + difusa + especular
    vec3 ambiente  = 0.20 * lightColor;
    vec3 difusa    = max(dot(N, L), 0.0) * lightColor;
    vec3 especular = 0.40 * pow(max(dot(V, R), 0.0), 32.0) * lightColor;

    FragColor = vec4((ambiente + difusa + especular) * colorObjeto, 1.0);
}