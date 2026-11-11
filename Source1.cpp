#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// stb_image: una sola vez en todo el proyecto
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// tinygltf: reutiliza el stb_image de arriba (no lo incluye otra vez)
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

// ---------------------------------------------------------------
// SHADERS
// ---------------------------------------------------------------
const char* vertexShaderSource =
"#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"\n"
"out vec2 TexCoord;\n"
"\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"\n"
"void main()\n"
"{\n"
"    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"    TexCoord = aTexCoord;\n"
"}\0";

const char* fragmentShaderSource =
"#version 330 core\n"
"\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"\n"
"uniform sampler2D texture1;\n"
"uniform vec4 baseColor;\n"
"\n"
"void main()\n"
"{\n"
"    FragColor = texture(texture1, TexCoord) * baseColor;\n"
"}\0";

void revisarShader(GLuint shader, const char* nombre)
{
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "Error en " << nombre << ": " << infoLog << std::endl;
    }
}

// ---------------------------------------------------------------
// CAMARA CON MOUSE (click izquierdo = girar, rueda = zoom)
// ---------------------------------------------------------------
float yaw = 0.0f;
float pitch = 0.3f;
float distancia = 3.5f;
double ultimoX = 0.0;
double ultimoY = 0.0;
bool arrastrando = false;

void mouseBoton(GLFWwindow* w, int boton, int accion, int mods)
{
    if (boton == GLFW_MOUSE_BUTTON_LEFT)
    {
        arrastrando = (accion == GLFW_PRESS);
        glfwGetCursorPos(w, &ultimoX, &ultimoY);
    }
}

void mouseMovimiento(GLFWwindow* w, double x, double y)
{
    if (!arrastrando) return;
    yaw -= (float)(x - ultimoX) * 0.01f;
    pitch += (float)(y - ultimoY) * 0.01f;
    pitch = glm::clamp(pitch, -1.5f, 1.5f);
    ultimoX = x;
    ultimoY = y;
}

void mouseRueda(GLFWwindow* w, double dx, double dy)
{
    distancia -= (float)dy * 0.3f;
    distancia = glm::clamp(distancia, 0.5f, 50.0f);
}

// ---------------------------------------------------------------
// CARGA DE MODELOS GLTF / GLB
// ---------------------------------------------------------------
struct Malla
{
    GLuint VAO = 0;
    GLuint texture = 0;
    GLsizei numIndices = 0;
    glm::mat4 transform = glm::mat4(1.0f); // posicion del nodo dentro del modelo
    glm::vec4 color = glm::vec4(1.0f);     // color base del material
};

GLuint crearTextura(const unsigned char* pixeles, int w, int h, int componentes)
{
    GLenum formato = GL_RGB;
    if (componentes == 1) formato = GL_RED;
    else if (componentes == 2) formato = GL_RG;
    else if (componentes == 4) formato = GL_RGBA;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, formato, w, h, 0, formato, GL_UNSIGNED_BYTE, pixeles);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

// Devuelve puntero a los datos de un accessor y su stride en bytes
const unsigned char* datosAccessor(const tinygltf::Model& m, const tinygltf::Accessor& a, size_t& stride)
{
    const tinygltf::BufferView& bv = m.bufferViews[a.bufferView];
    int s = a.ByteStride(bv);
    stride = (s > 0) ? (size_t)s : 0;
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + a.byteOffset;
}

// Calcula la matriz local de un nodo
glm::mat4 matrizDelNodo(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                m[c][r] = (float)node.matrix[c * 4 + r];
        return m;
    }

    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (node.translation.size() == 3)
        T = glm::translate(glm::mat4(1.0f),
            glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
    if (node.rotation.size() == 4)
    {
        glm::quat q((float)node.rotation[3], (float)node.rotation[0],
            (float)node.rotation[1], (float)node.rotation[2]); // w, x, y, z
        R = glm::mat4_cast(q);
    }
    if (node.scale.size() == 3)
        S = glm::scale(glm::mat4(1.0f),
            glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
    return T * R * S;
}

// Convierte una primitiva del modelo en una Malla lista para dibujar
void procesarPrimitiva(const tinygltf::Model& model, const tinygltf::Primitive& prim,
    const glm::mat4& transform, std::vector<Malla>& mallas,
    glm::vec3& minB, glm::vec3& maxB)
{
    if (prim.mode != TINYGLTF_MODE_TRIANGLES) return;
    if (!prim.attributes.count("POSITION")) return;

    // --- Vertices: X Y Z U V ---
    const tinygltf::Accessor& accPos = model.accessors[prim.attributes.at("POSITION")];
    size_t stridePos;
    const unsigned char* pos = datosAccessor(model, accPos, stridePos);
    if (stridePos == 0) stridePos = 3 * sizeof(float);

    const unsigned char* uv = nullptr;
    size_t strideUV = 0;
    if (prim.attributes.count("TEXCOORD_0"))
    {
        const tinygltf::Accessor& accUV = model.accessors[prim.attributes.at("TEXCOORD_0")];
        if (accUV.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            uv = datosAccessor(model, accUV, strideUV);
            if (strideUV == 0) strideUV = 2 * sizeof(float);
        }
    }

    std::vector<float> vertices;
    vertices.reserve(accPos.count * 5);
    for (size_t i = 0; i < accPos.count; i++)
    {
        const float* p = (const float*)(pos + i * stridePos);
        vertices.push_back(p[0]);
        vertices.push_back(p[1]);
        vertices.push_back(p[2]);

        // Limites del modelo (para centrarlo y escalarlo despues)
        glm::vec3 w = glm::vec3(transform * glm::vec4(p[0], p[1], p[2], 1.0f));
        minB = glm::min(minB, w);
        maxB = glm::max(maxB, w);

        if (uv)
        {
            const float* t = (const float*)(uv + i * strideUV);
            vertices.push_back(t[0]);
            vertices.push_back(t[1]);
        }
        else
        {
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
        }
    }

    // --- Indices (se convierten todos a unsigned int) ---
    std::vector<unsigned int> indices;
    if (prim.indices >= 0)
    {
        const tinygltf::Accessor& accIdx = model.accessors[prim.indices];
        size_t strideIdx;
        const unsigned char* idx = datosAccessor(model, accIdx, strideIdx);

        size_t tam = 1;
        if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) tam = 2;
        else if (accIdx.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) tam = 4;
        if (strideIdx == 0) strideIdx = tam;

        for (size_t i = 0; i < accIdx.count; i++)
        {
            const unsigned char* d = idx + i * strideIdx;
            if (tam == 2)      indices.push_back(*(const unsigned short*)d);
            else if (tam == 4) indices.push_back(*(const unsigned int*)d);
            else               indices.push_back(*d);
        }
    }
    else
    {
        for (size_t i = 0; i < accPos.count; i++) indices.push_back((unsigned int)i);
    }

    // --- Material: textura y color base ---
    GLuint tex = 0;
    glm::vec4 color(1.0f);
    if (prim.material >= 0)
    {
        const tinygltf::Material& mat = model.materials[prim.material];
        const std::vector<double>& cf = mat.pbrMetallicRoughness.baseColorFactor;
        if (cf.size() == 4)
            color = glm::vec4((float)cf[0], (float)cf[1], (float)cf[2], (float)cf[3]);

        int t = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (t >= 0 && t < (int)model.textures.size())
        {
            int src = model.textures[t].source;
            if (src >= 0 && src < (int)model.images.size())
            {
                const tinygltf::Image& img = model.images[src];
                if (!img.image.empty())
                    tex = crearTextura(img.image.data(), img.width, img.height, img.component);
            }
        }
    }
    if (tex == 0)
    {
        unsigned char blanco[3] = { 255, 255, 255 };
        tex = crearTextura(blanco, 1, 1, 3);
    }

    // --- VAO / VBO / EBO ---
    Malla m;
    GLuint VBO, EBO;
    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    m.texture = tex;
    m.numIndices = (GLsizei)indices.size();
    m.transform = transform;
    m.color = color;
    mallas.push_back(m);
}

// Recorre los nodos del modelo (cada nodo puede tener una malla y nodos hijos)
void procesarNodo(const tinygltf::Model& model, int indiceNodo, const glm::mat4& padre,
    std::vector<Malla>& mallas, glm::vec3& minB, glm::vec3& maxB)
{
    const tinygltf::Node& node = model.nodes[indiceNodo];
    glm::mat4 global = padre * matrizDelNodo(node);

    if (node.mesh >= 0)
    {
        for (const tinygltf::Primitive& prim : model.meshes[node.mesh].primitives)
            procesarPrimitiva(model, prim, global, mallas, minB, maxB);
    }
    for (int hijo : node.children)
        procesarNodo(model, hijo, global, mallas, minB, maxB);
}

std::vector<Malla> cargarGLTF(const char* ruta, glm::vec3& minB, glm::vec3& maxB)
{
    std::vector<Malla> mallas;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    std::string r = ruta;
    bool esGLB = r.size() >= 4 && r.substr(r.size() - 4) == ".glb";
    bool ok = esGLB ? loader.LoadBinaryFromFile(&model, &err, &warn, r)
        : loader.LoadASCIIFromFile(&model, &err, &warn, r);

    if (!warn.empty()) std::cout << "Aviso: " << warn << std::endl;
    if (!err.empty())  std::cout << "Error: " << err << std::endl;
    if (!ok) return mallas;

    int escena = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (escena < (int)model.scenes.size())
    {
        for (int n : model.scenes[escena].nodes)
            procesarNodo(model, n, glm::mat4(1.0f), mallas, minB, maxB);
    }
    return mallas;
}

// ---------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------
int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Modelo GLTF con camara", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Error creando ventana" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glViewport(0, 0, 800, 800);
    glEnable(GL_DEPTH_TEST);

    // Callbacks del mouse para la camara
    glfwSetMouseButtonCallback(window, mouseBoton);
    glfwSetCursorPosCallback(window, mouseMovimiento);
    glfwSetScrollCallback(window, mouseRueda);

    // ----- Piramide (se dibuja solo si no se encuentra el modelo) -----
    GLfloat vertices[] =
    {
        // X      Y      Z      U     V
        -0.5f, -0.4f,  0.5f,   0.0f, 0.0f,
         0.5f, -0.4f,  0.5f,   1.0f, 0.0f,
         0.5f, -0.4f, -0.5f,   1.0f, 1.0f,
        -0.5f, -0.4f, -0.5f,   0.0f, 1.0f,

        -0.5f, -0.4f,  0.5f,   0.0f, 0.0f,
         0.5f, -0.4f,  0.5f,   1.0f, 0.0f,
         0.0f,  0.5f,  0.0f,   0.5f, 1.0f,

         0.5f, -0.4f,  0.5f,   0.0f, 0.0f,
         0.5f, -0.4f, -0.5f,   1.0f, 0.0f,
         0.0f,  0.5f,  0.0f,   0.5f, 1.0f,

         0.5f, -0.4f, -0.5f,   0.0f, 0.0f,
        -0.5f, -0.4f, -0.5f,   1.0f, 0.0f,
         0.0f,  0.5f,  0.0f,   0.5f, 1.0f,

        -0.5f, -0.4f, -0.5f,   0.0f, 0.0f,
        -0.5f, -0.4f,  0.5f,   1.0f, 0.0f,
         0.0f,  0.5f,  0.0f,   0.5f, 1.0f
    };

    GLuint indices[] =
    {
        0, 3, 2,   0, 2, 1,
        4, 5, 6,
        7, 8, 9,
        10, 11, 12,
        13, 14, 15
    };

    // ----- Shaders -----
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    revisarShader(vertexShader, "vertex shader");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    revisarShader(fragmentShader, "fragment shader");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // ----- VAO, VBO, EBO de la piramide -----
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // ----- Cargar el modelo GLB -----
    // IMPORTANTE: se carga ANTES de stbi_set_flip_vertically_on_load(true),
    // porque esa opcion es global y voltearia las texturas del modelo.
    const char* rutasModelo[] =
    {
        "modelo.glb",
        "C:/Users/ruqui/Downloads/modelo.glb",
        "C:/Users/ruqui/source/repos/Project1/modelo.glb"
    };

    glm::vec3 minB(1e9f), maxB(-1e9f);
    std::vector<Malla> mallas;
    for (const char* ruta : rutasModelo)
    {
        mallas = cargarGLTF(ruta, minB, maxB);
        if (!mallas.empty())
        {
            std::cout << "Modelo cargado desde: " << ruta
                << " (" << mallas.size() << " mallas)" << std::endl;
            break;
        }
    }
    if (mallas.empty())
        std::cout << "No se encontro el modelo, se dibuja la piramide." << std::endl;

    // Centrar y escalar el modelo automaticamente para que quepa en ~2 unidades
    glm::mat4 ajusteModelo(1.0f);
    if (!mallas.empty())
    {
        glm::vec3 centro = (minB + maxB) * 0.5f;
        glm::vec3 tam = maxB - minB;
        float maxDim = std::max(tam.x, std::max(tam.y, tam.z));
        float escala = (maxDim > 0.0f) ? (2.0f / maxDim) : 1.0f;
        ajusteModelo = glm::scale(glm::mat4(1.0f), glm::vec3(escala))
            * glm::translate(glm::mat4(1.0f), -centro);
    }

    // ----- Textura de la piramide -----
    GLuint texturaPiramide;
    glGenTextures(1, &texturaPiramide);
    glBindTexture(GL_TEXTURE_2D, texturaPiramide);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    const char* rutas[] =
    {
        "agua.jpg",
        "C:/Users/ruqui/Downloads/agua.jpg",
        "C:/Users/ruqui/source/repos/Project1/agua.jpg"
    };

    unsigned char* data = NULL;
    for (const char* ruta : rutas)
    {
        data = stbi_load(ruta, &width, &height, &channels, 0);
        if (data)
        {
            std::cout << "Imagen encontrada en: " << ruta << std::endl;
            break;
        }
    }

    if (data)
    {
        GLenum format = GL_RGB;
        if (channels == 4) format = GL_RGBA;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Textura cargada con exito!" << std::endl;
        stbi_image_free(data);
    }
    else
    {
        std::cout << "No se encontro agua.jpg" << std::endl;
    }

    // ----- Uniforms fijos -----
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "baseColor");

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 800.0f, 0.1f, 100.0f);

    // ----- Loop principal -----
    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Camara orbital: gira alrededor del origen segun el mouse
        glm::vec3 objetivo(0.0f, 0.0f, 0.0f);
        glm::vec3 camaraPos = objetivo + distancia * glm::vec3(
            cos(pitch) * sin(yaw),
            sin(pitch),
            cos(pitch) * cos(yaw));
        glm::mat4 view = glm::lookAt(camaraPos, objetivo, glm::vec3(0.0f, 1.0f, 0.0f));

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glActiveTexture(GL_TEXTURE0);

        if (!mallas.empty())
        {
            // Dibujar el modelo GLB
            for (const Malla& m : mallas)
            {
                glm::mat4 model = ajusteModelo * m.transform;
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform4fv(colorLoc, 1, glm::value_ptr(m.color));

                glBindTexture(GL_TEXTURE_2D, m.texture);
                glBindVertexArray(m.VAO);
                glDrawElements(GL_TRIANGLES, m.numIndices, GL_UNSIGNED_INT, 0);
            }
        }
        else
        {
            // Dibujar la piramide (respaldo)
            glm::mat4 model = glm::mat4(1.0f);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

            glBindTexture(GL_TEXTURE_2D, texturaPiramide);
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ----- Limpieza -----
    for (const Malla& m : mallas)
    {
        glDeleteVertexArrays(1, &m.VAO);
        glDeleteTextures(1, &m.texture);
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &texturaPiramide);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}