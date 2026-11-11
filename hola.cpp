#define NOMINMAX
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VBO.h"
#include "VAO.h"
#include "EBO.h"
#include "ShaderClass.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"


GLuint compilarShader(GLenum tipo, const char* ruta)
{
    std::ifstream archivo(ruta);
    if (!archivo) std::cout << "No se pudo abrir: " << ruta << std::endl;
    std::stringstream ss;
    ss << archivo.rdbuf();
    std::string src = ss.str();
    const char* codigo = src.c_str();

    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &codigo, NULL);
    glCompileShader(s);

    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        std::cout << "Error en " << ruta << ": " << log << std::endl;
    }
    return s;
}

GLuint crearPrograma(const char* rutaVert, const char* rutaFrag)
{
    GLuint vs = compilarShader(GL_VERTEX_SHADER, rutaVert);
    GLuint fs = compilarShader(GL_FRAGMENT_SHADER, rutaFrag);
    GLuint programa = glCreateProgram();
    glAttachShader(programa, vs);
    glAttachShader(programa, fs);
    glLinkProgram(programa);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return programa;
}


glm::vec3 objetivo(0.0f, 0.0f, 0.0f); // punto al que mira la camara (se mueve con WASD)
float yaw = 0.0f, pitch = 0.3f, distancia = 8.0f;
double ultimoX = 0.0, ultimoY = 0.0;
bool arrastrando = false;
float giroExtra = 0.0f; // giro adicional de Mario con Q / E

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
    pitch = glm::clamp(pitch + (float)(y - ultimoY) * 0.01f, -1.5f, 1.5f);
    ultimoX = x;
    ultimoY = y;
}

void mouseRueda(GLFWwindow* w, double dx, double dy)
{
    distancia = glm::clamp(distancia - (float)dy * 0.3f, 0.5f, 50.0f);
}


void procesarTeclado(GLFWwindow* w, float dt)
{
    float vel = 4.0f * dt;
    glm::vec3 adelante(-sin(yaw), 0.0f, -cos(yaw)); // hacia donde mira la camara (sobre el piso)
    glm::vec3 derecha(cos(yaw), 0.0f, -sin(yaw));

    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)
        objetivo += adelante * vel;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)
        objetivo -= adelante * vel;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) objetivo += derecha * vel;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) objetivo -= derecha * vel;

    if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  yaw += 1.5f * dt;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw -= 1.5f * dt;

    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) giroExtra += 2.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) giroExtra -= 2.0f * dt;
}

glm::vec3 P[4] = {
    glm::vec3(-2.5f, -1.0f,  0.0f),
    glm::vec3(-1.0f, -1.0f,  3.0f),
    glm::vec3(1.0f, -1.0f, -3.0f),
    glm::vec3(2.5f, -1.0f,  0.0f) };
float duracionRecorrido = 8.0f;

glm::vec3 puntoBezier(float t)
{
    float u = 1.0f - t;
    return u * u * u * P[0] + 3.0f * u * u * t * P[1] + 3.0f * u * t * t * P[2] + t * t * t * P[3];
}

glm::vec3 tangenteBezier(float t) // derivada: hacia donde va la curva
{
    float u = 1.0f - t;
    return 3.0f * u * u * (P[1] - P[0]) + 6.0f * u * t * (P[2] - P[1]) + 3.0f * t * t * (P[3] - P[2]);
}

GLuint crearVAOpuntos(const std::vector<glm::vec3>& puntos)
{
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, puntos.size() * sizeof(glm::vec3), puntos.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    return vao;
}

// =====================================================================
// CARGA DE MODELOS GLTF / GLB
// =====================================================================
struct Malla
{
    GLuint VAO = 0, textura = 0;
    GLsizei numIndices = 0;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
};

GLuint crearTextura(const unsigned char* px, int w, int h, int comp)
{
    GLenum f = (comp == 1) ? GL_RED : (comp == 2) ? GL_RG : (comp == 4) ? GL_RGBA : GL_RGB;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

// Puntero a los datos de un accessor y su stride (si es 0 usa "defecto")
const unsigned char* datosAccessor(const tinygltf::Model& m, const tinygltf::Accessor& a,
    size_t& stride, size_t defecto)
{
    const tinygltf::BufferView& bv = m.bufferViews[a.bufferView];
    int s = a.ByteStride(bv);
    stride = (s > 0) ? (size_t)s : defecto;
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + a.byteOffset;
}

glm::mat4 matrizDelNodo(const tinygltf::Node& n)
{
    if (n.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                m[c][r] = (float)n.matrix[c * 4 + r];
        return m;
    }
    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (n.translation.size() == 3)
        T = glm::translate(glm::mat4(1.0f), glm::vec3(n.translation[0], n.translation[1], n.translation[2]));
    if (n.rotation.size() == 4)
        R = glm::mat4_cast(glm::quat((float)n.rotation[3], (float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2]));
    if (n.scale.size() == 3)
        S = glm::scale(glm::mat4(1.0f), glm::vec3(n.scale[0], n.scale[1], n.scale[2]));
    return T * R * S;
}

void procesarPrimitiva(const tinygltf::Model& model, const tinygltf::Primitive& prim,
    const glm::mat4& transform, std::vector<Malla>& mallas, glm::vec3& minB, glm::vec3& maxB)
{
    if (prim.mode != TINYGLTF_MODE_TRIANGLES || !prim.attributes.count("POSITION")) return;

    // --- Vertices: X Y Z U V NX NY NZ (8 floats por vertice) ---
    const tinygltf::Accessor& accPos = model.accessors[prim.attributes.at("POSITION")];
    size_t n = accPos.count, sP, sU = 0, sN = 0;
    const unsigned char* pos = datosAccessor(model, accPos, sP, 12);
    const unsigned char* uv = nullptr;
    const unsigned char* nor = nullptr;

    if (prim.attributes.count("TEXCOORD_0"))
    {
        const tinygltf::Accessor& a = model.accessors[prim.attributes.at("TEXCOORD_0")];
        if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) uv = datosAccessor(model, a, sU, 8);
    }
    if (prim.attributes.count("NORMAL"))
    {
        const tinygltf::Accessor& a = model.accessors[prim.attributes.at("NORMAL")];
        if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) nor = datosAccessor(model, a, sN, 12);
    }

    std::vector<float> v(n * 8, 0.0f);
    for (size_t i = 0; i < n; i++)
    {
        const float* p = (const float*)(pos + i * sP);
        v[i * 8 + 0] = p[0]; v[i * 8 + 1] = p[1]; v[i * 8 + 2] = p[2];

        glm::vec3 w = glm::vec3(transform * glm::vec4(p[0], p[1], p[2], 1.0f)); // para centrar/escalar
        minB = glm::min(minB, w);
        maxB = glm::max(maxB, w);

        if (uv)
        {
            const float* t = (const float*)(uv + i * sU);
            v[i * 8 + 3] = t[0]; v[i * 8 + 4] = t[1];
        }
        if (nor)
        {
            const float* nn = (const float*)(nor + i * sN);
            v[i * 8 + 5] = nn[0]; v[i * 8 + 6] = nn[1]; v[i * 8 + 7] = nn[2];
        }
    }

    // --- Indices (todos a unsigned int) ---
    std::vector<unsigned int> indices;
    if (prim.indices >= 0)
    {
        const tinygltf::Accessor& a = model.accessors[prim.indices];
        size_t tam = (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? 2 :
            (a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) ? 4 : 1;
        size_t sI;
        const unsigned char* idx = datosAccessor(model, a, sI, tam);
        for (size_t i = 0; i < a.count; i++)
        {
            const unsigned char* d = idx + i * sI;
            indices.push_back(tam == 2 ? *(const unsigned short*)d : tam == 4 ? *(const unsigned int*)d : *d);
        }
    }
    else
        for (size_t i = 0; i < n; i++) indices.push_back((unsigned int)i);

    // --- Si el modelo no trae normales, se calculan con los triangulos ---
    if (!nor)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            unsigned int a = indices[i], b = indices[i + 1], c = indices[i + 2];
            glm::vec3 pa(v[a * 8], v[a * 8 + 1], v[a * 8 + 2]);
            glm::vec3 pb(v[b * 8], v[b * 8 + 1], v[b * 8 + 2]);
            glm::vec3 pc(v[c * 8], v[c * 8 + 1], v[c * 8 + 2]);
            glm::vec3 fn = glm::cross(pb - pa, pc - pa);
            for (unsigned int k : { a, b, c })
            {
                v[k * 8 + 5] += fn.x; v[k * 8 + 6] += fn.y; v[k * 8 + 7] += fn.z;
            }
        }
        for (size_t i = 0; i < n; i++)
        {
            glm::vec3 nn(v[i * 8 + 5], v[i * 8 + 6], v[i * 8 + 7]);
            nn = (glm::length(nn) > 0.0f) ? glm::normalize(nn) : glm::vec3(0.0f, 1.0f, 0.0f);
            v[i * 8 + 5] = nn.x; v[i * 8 + 6] = nn.y; v[i * 8 + 7] = nn.z;
        }
    }

    // --- Material: color base y textura (si no hay, textura blanca) ---
    GLuint tex = 0;
    glm::vec4 color(1.0f);
    if (prim.material >= 0)
    {
        const tinygltf::Material& mat = model.materials[prim.material];
        const std::vector<double>& cf = mat.pbrMetallicRoughness.baseColorFactor;
        if (cf.size() == 4) color = glm::vec4(cf[0], cf[1], cf[2], cf[3]);

        int t = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (t >= 0 && t < (int)model.textures.size())
        {
            int src = model.textures[t].source;
            if (src >= 0 && src < (int)model.images.size() && !model.images[src].image.empty())
            {
                const tinygltf::Image& img = model.images[src];
                tex = crearTextura(img.image.data(), img.width, img.height, img.component);
            }
        }
    }
    if (tex == 0)
    {
        unsigned char blanco[3] = { 255, 255, 255 };
        tex = crearTextura(blanco, 1, 1, 3);
    }

    Malla m;
    GLuint VBO, EBO;
    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    m.textura = tex;
    m.numIndices = (GLsizei)indices.size();
    m.transform = transform;
    m.color = color;
    mallas.push_back(m);
}

void procesarNodo(const tinygltf::Model& model, int i, const glm::mat4& padre,
    std::vector<Malla>& mallas, glm::vec3& minB, glm::vec3& maxB)
{
    const tinygltf::Node& node = model.nodes[i];
    glm::mat4 global = padre * matrizDelNodo(node);
    if (node.mesh >= 0)
        for (const tinygltf::Primitive& prim : model.meshes[node.mesh].primitives)
            procesarPrimitiva(model, prim, global, mallas, minB, maxB);
    for (int hijo : node.children)
        procesarNodo(model, hijo, global, mallas, minB, maxB);
}

std::vector<Malla> cargarGLTF(const char* ruta, glm::vec3& minB, glm::vec3& maxB)
{
    std::vector<Malla> mallas;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn, r = ruta;

    bool esGLB = r.size() >= 4 && r.substr(r.size() - 4) == ".glb";
    bool ok = esGLB ? loader.LoadBinaryFromFile(&model, &err, &warn, r)
        : loader.LoadASCIIFromFile(&model, &err, &warn, r);
    if (!err.empty()) std::cout << "Error: " << err << std::endl;
    if (!ok) return mallas;

    // Mario viene con Z hacia arriba: se gira -90 grados en X para pararlo
    glm::mat4 rotacionInicial = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
    int escena = model.defaultScene >= 0 ? model.defaultScene : 0;
    for (int n : model.scenes[escena].nodes)
        procesarNodo(model, n, rotacionInicial, mallas, minB, maxB);
    return mallas;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Mario", NULL, NULL);
    if (!window)
    {
        std::cout << "Error creando ventana" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glViewport(0, 0, 800, 800);
    glEnable(GL_DEPTH_TEST);

    glfwSetMouseButtonCallback(window, mouseBoton);
    glfwSetCursorPosCallback(window, mouseMovimiento);
    glfwSetScrollCallback(window, mouseRueda);

    // Shaders desde archivos (la carpeta "shaders" debe estar junto al ejecutable / directorio de trabajo)
    GLuint shaderProgram = crearPrograma("shaders/modelo.vert", "shaders/modelo.frag");
    GLuint shaderLinea = crearPrograma("shaders/linea.vert", "shaders/linea.frag");

    // Cargar a Mario
    glm::vec3 minB(1e9f), maxB(-1e9f);
    std::vector<Malla> mallas = cargarGLTF("Mario.glb", minB, maxB);
    if (mallas.empty())
        std::cout << "No se encontro Mario.glb: ponlo en el directorio de trabajo." << std::endl;

    // Centrar y escalar el modelo para que mida ~2 unidades
    glm::mat4 ajusteModelo(1.0f);
    if (!mallas.empty())
    {
        glm::vec3 centro = (minB + maxB) * 0.5f;
        glm::vec3 tam = maxB - minB;
        float maxDim = (std::max)(tam.x, (std::max)(tam.y, tam.z));
        ajusteModelo = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f / maxDim))
            * glm::translate(glm::mat4(1.0f), -centro);
    }

    // Curva de Bezier y puntos de control en la GPU
    std::vector<glm::vec3> puntosCurva;
    for (int i = 0; i <= 100; i++) puntosCurva.push_back(puntoBezier(i / 100.0f));
    GLuint vaoCurva = crearVAOpuntos(puntosCurva);
    GLuint vaoControl = crearVAOpuntos({ P[0], P[1], P[2], P[3] });
    glPointSize(10.0f);

    // Uniforms y valores fijos
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glm::vec3 luzPos(3.0f, 3.0f, 4.0f), luzColor(1.0f);

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(luzPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(luzColor));
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "baseColor");

    glUseProgram(shaderLinea);
    glUniformMatrix4fv(glGetUniformLocation(shaderLinea, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    GLint viewLineaLoc = glGetUniformLocation(shaderLinea, "view");
    GLint colorLineaLoc = glGetUniformLocation(shaderLinea, "color");

    float tiempoAnterior = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float tiempo = (float)glfwGetTime();
        float dt = tiempo - tiempoAnterior;
        tiempoAnterior = tiempo;

        procesarTeclado(window, dt);

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Camara orbital alrededor de "objetivo" (que se mueve con el teclado)
        glm::vec3 camaraPos = objetivo + distancia * glm::vec3(
            cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw));
        glm::mat4 view = glm::lookAt(camaraPos, objetivo, glm::vec3(0.0f, 1.0f, 0.0f));

        // Posicion de Mario sobre la curva (ida y vuelta)
        float ciclo = fmod(tiempo / duracionRecorrido, 2.0f);
        float t = (ciclo < 1.0f) ? ciclo : 2.0f - ciclo;
        float sentido = (ciclo < 1.0f) ? 1.0f : -1.0f;
        glm::vec3 tangente = tangenteBezier(t) * sentido;
        float angulo = atan2(tangente.x, tangente.z);

        glm::mat4 movimiento = glm::translate(glm::mat4(1.0f), puntoBezier(t) + glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(glm::mat4(1.0f), angulo + giroExtra, glm::vec3(0.0f, 1.0f, 0.0f));

        // Dibujar a Mario
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(camaraPos));
        glActiveTexture(GL_TEXTURE0);

        for (const Malla& m : mallas)
        {
            glm::mat4 modelMalla = movimiento * ajusteModelo * m.transform;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMalla));
            glUniform4fv(colorLoc, 1, glm::value_ptr(m.color));
            glBindTexture(GL_TEXTURE_2D, m.textura);
            glBindVertexArray(m.VAO);
            glDrawElements(GL_TRIANGLES, m.numIndices, GL_UNSIGNED_INT, 0);
        }

        // Dibujar la curva de Bezier, su poligono de control y los puntos
        glUseProgram(shaderLinea);
        glUniformMatrix4fv(viewLineaLoc, 1, GL_FALSE, glm::value_ptr(view));

        glUniform3f(colorLineaLoc, 0.2f, 1.0f, 0.3f); // curva (verde)
        glBindVertexArray(vaoCurva);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)puntosCurva.size());

        glUniform3f(colorLineaLoc, 0.6f, 0.6f, 0.6f); // poligono de control (gris)
        glBindVertexArray(vaoControl);
        glDrawArrays(GL_LINE_STRIP, 0, 4);

        glUniform3f(colorLineaLoc, 1.0f, 0.9f, 0.1f); // puntos de control (amarillo)
        glDrawArrays(GL_POINTS, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (const Malla& m : mallas)
    {
        glDeleteVertexArrays(1, &m.VAO);
        glDeleteTextures(1, &m.textura);
    }
    glDeleteProgram(shaderProgram);
    glDeleteProgram(shaderLinea);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}