// Librerias estandar de C++: consola, leer archivos, texto, listas y utilidades
#define NOMINMAX
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

// GLAD: carga las funciones de OpenG
#include <glad/glad.h>
//crea la ventana y maneja el mouse y teclado
#include <GLFW/glfw3.h>

//matematicas para graficos 3D: matrices, vectores, quaterniones
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // translate, rotate, scale, perspective
#include <glm/gtc/quaternion.hpp> // rotaciones con quaterniones
#include <glm/gtc/type_ptr.hpp>// convierte una matriz a puntero (para OpenGL)

//clases del tutorial, como sacar los shaders de archivos, VBO, VAO, EBO, etc
#include "VBO.h"
#include "VAO.h"
#include "EBO.h"
#include "ShaderClass.h"
#define STB_IMAGE_IMPLEMENTATION //lee las imagenes (jpg, png, etc) y las convierte a un array de bytes
#include "stb_image.h"

//tinygltf: lee archivos.gltf / .glb.Estos #define le dicen que use el
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

//lee un archivo de shader lo compila y devuelve el ID del shader
//dice si es vertex o fragment shader con el parametro "tipo" (GL_VERTEX_SHADER o GL_FRAGMENT_SHADER)
GLuint compilarShader(GLenum tipo, const char* ruta)
{
	//abre el archivo, si no existe imprime un mensaje de error
    std::ifstream archivo(ruta);
    if (!archivo) std::cout << "No se pudo abrir: " << ruta << std::endl;
	std::stringstream ss; //lee todo el contenido del archivo y lo pone en un string
    ss << archivo.rdbuf();
    std::string src = ss.str();
    const char* codigo = src.c_str(); //opengl necesita el contexto en const char*

	//crea el shader , le pasa el codigo y lo compila
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &codigo, NULL);
    glCompileShader(s);

	//pregunta si compilo bien, si no imprime el log de error
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
//une el vertex y fragment shader en un programa de OpenGL y devuelve el ID del programa, listos para usar
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

//camara
glm::vec3 objetivo(0.0f, 0.0f, 0.0f); // punto al que mira la camara (se mueve con WASD)
float yaw = 0.0f, pitch = 0.3f, distancia = 8.0f; //angulo horizontal, vertical y distancia de la camara respecto al objetivo
double ultimoX = 0.0, ultimoY = 0.0;
bool arrastrando = false;
float giroExtra = 0.0f; // giro adicional de Mario con Q / E

//se llama al presionar el boton del mouse, si es click izquierdo activa el arrastre y guarda la posicion del mouse
void mouseBoton(GLFWwindow* w, int boton, int accion, int mods)
{
    if (boton == GLFW_MOUSE_BUTTON_LEFT)
    {
        arrastrando = (accion == GLFW_PRESS);
        glfwGetCursorPos(w, &ultimoX, &ultimoY);
    }
}
//mouse
//se llama cada vez que el mouse se mueve, si esta arrastrando cambia el yaw y pitch de la camara
void mouseMovimiento(GLFWwindow* w, double x, double y)
{
    if (!arrastrando) return;
    yaw -= (float)(x - ultimoX) * 0.01f;
    pitch = glm::clamp(pitch + (float)(y - ultimoY) * 0.01f, -1.5f, 1.5f);
    ultimoX = x;
    ultimoY = y;
}
//se llama al mover la rueda del mouse, cambia la distancia de la camara respecto al objetivo
void mouseRueda(GLFWwindow* w, double dx, double dy)
{
    distancia = glm::clamp(distancia - (float)dy * 0.3f, 0.5f, 50.0f);
}

//teclado traslacion de objetivo y giro extra de Mario
void procesarTeclado(GLFWwindow* w, float dt)
{
    float vel = 4.0f * dt;
    glm::vec3 adelante(-sin(yaw), 0.0f, -cos(yaw)); // hacia donde mira la camara (sobre el piso)
    glm::vec3 derecha(cos(yaw), 0.0f, -sin(yaw));

	//mover el punto objeto = trasladar la camara con WASD o flechas
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)
        objetivo += adelante * vel;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)
        objetivo -= adelante * vel;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) objetivo += derecha * vel;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) objetivo -= derecha * vel;
	//girar la camara con las flechas izquierda/derecha
    if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)  yaw += 1.5f * dt;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw -= 1.5f * dt;
	//girar Mario con Q/E
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS) giroExtra += 2.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS) giroExtra -= 2.0f * dt;
}

//curva de Bezier cubica: 4 puntos de control y duracion del recorrido
glm::vec3 P[4] = {
    glm::vec3(-2.5f, -1.0f,  0.0f),
    glm::vec3(-1.0f, -1.0f,  3.0f),
    glm::vec3(1.0f, -1.0f, -3.0f),
    glm::vec3(2.5f, -1.0f,  0.0f) };
float duracionRecorrido = 8.0f; //segundos que tarda Mario en recorrer la curva de ida y vuelta
// Devuelve el punto de la curva para un valor t entre 0 y 1
glm::vec3 puntoBezier(float t)
{
    float u = 1.0f - t;
    return u * u * u * P[0] + 3.0f * u * u * t * P[1] + 3.0f * u * t * t * P[2] + t * t * t * P[3];
}
// Devuelve la direccion hacia donde va la curva en t (la derivada).
// Sirve para que Mario mire hacia donde camina.
glm::vec3 tangenteBezier(float t) // derivada: hacia donde va la curva
{
    float u = 1.0f - t;
    return 3.0f * u * u * (P[1] - P[0]) + 6.0f * u * t * (P[2] - P[1]) + 3.0f * t * t * (P[3] - P[2]);
}
// Sube una lista de puntos 3D a la GPU y devuelve su VAO.
// VBO = buffer con los datos | VAO = guarda como leer esos datos
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
//carga de modelo glTF: cada malla tiene un VAO, una textura, un color base, un numero de indices y una transformacion
struct Malla
{
    GLuint VAO = 0, textura = 0;
    GLsizei numIndices = 0;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
};
// Crea una textura de OpenGL a partir de los pixeles de una imagen
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

// En un glTF los datos estan en buffers. Un "accessor" dice donde empiezan
// y cada cuantos bytes esta el siguiente valor (stride).
// Esta funcion devuelve el puntero a los datos y su stride.
const unsigned char* datosAccessor(const tinygltf::Model& m, const tinygltf::Accessor& a,
    size_t& stride, size_t defecto)
{
    const tinygltf::BufferView& bv = m.bufferViews[a.bufferView];
    int s = a.ByteStride(bv);
    stride = (s > 0) ? (size_t)s : defecto;
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + a.byteOffset;
}
// Calcula la matriz local de un nodo del modelo (cada nodo tiene su
// traslacion, rotacion y escala, o una matriz ya hecha)
glm::mat4 matrizDelNodo(const tinygltf::Node& n)
{
    // Caso 1: el nodo ya trae una matriz de 16 numeros
    if (n.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                m[c][r] = (float)n.matrix[c * 4 + r];
        return m;
    }
	// Caso 2: el nodo trae traslacion, rotacion y escala por separado
    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (n.translation.size() == 3)
        T = glm::translate(glm::mat4(1.0f), glm::vec3(n.translation[0], n.translation[1], n.translation[2]));
    if (n.rotation.size() == 4)
        R = glm::mat4_cast(glm::quat((float)n.rotation[3], (float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2]));
    if (n.scale.size() == 3)
        S = glm::scale(glm::mat4(1.0f), glm::vec3(n.scale[0], n.scale[1], n.scale[2]));
    return T * R * S;
}
// Convierte una parte del modelo en una Malla (VAO + VBO + EBO + textura)
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
    // Leer coordenadas de textura (si el modelo las trae)
    if (prim.attributes.count("TEXCOORD_0"))
    {
        const tinygltf::Accessor& a = model.accessors[prim.attributes.at("TEXCOORD_0")];
        if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) uv = datosAccessor(model, a, sU, 8);
    }
    // Leer normales (si el modelo las trae)
    if (prim.attributes.count("NORMAL"))
    {
        const tinygltf::Accessor& a = model.accessors[prim.attributes.at("NORMAL")];
        if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) nor = datosAccessor(model, a, sN, 12);
    }
    // Juntar todo en un solo arreglo "v": 8 floats por cada vertice
    std::vector<float> v(n * 8, 0.0f);
    for (size_t i = 0; i < n; i++)
    {
        const float* p = (const float*)(pos + i * sP);
        v[i * 8 + 0] = p[0]; v[i * 8 + 1] = p[1]; v[i * 8 + 2] = p[2];
        // Se calcula la caja que contiene al modelo (minB y maxB) para
        // poder centrarlo y escalarlo despues en el main
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

    // Los indices dicen que vertices forman cada triangulo (de 3 en 3)
    std::vector<unsigned int> indices;
    if (prim.indices >= 0)
    {
        // Los indices pueden venir en 1, 2 o 4 bytes; se convierten todos a unsigned int
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
            // Producto cruz de dos lados del triangulo = vector perpendicular (la normal)
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

    //Material: color base y textura (si no hay, textura blanca) 
    GLuint tex = 0;
    glm::vec4 color(1.0f);
    if (prim.material >= 0)
    {
		const tinygltf::Material& mat = model.materials[prim.material]; //color base del material (no tienen textura)
        const std::vector<double>& cf = mat.pbrMetallicRoughness.baseColorFactor;
        if (cf.size() == 4) color = glm::vec4(cf[0], cf[1], cf[2], cf[3]);
		//textura del material (si la tiene)
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
        // Sin textura: se usa un pixel blanco para que solo se vea el color base
        unsigned char blanco[3] = { 255, 255, 255 };
        tex = crearTextura(blanco, 1, 1, 3);
    }
    //Subir todo a la GPU: VAO / VBO / EBO
    // VBO = vertices | EBO = indices | VAO = guarda como leerlos
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
    // Como leer los 8 floats de cada vertice (8 * sizeof(float) = tamano de cada vertice):
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0); // posicion 0
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);// posicion 1 = coordenadas de textura
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(2);// posicion 2 = normales

    // Guardar los datos en la Malla y agregarla a la lista
    m.textura = tex;
    m.numIndices = (GLsizei)indices.size();
    m.transform = transform;
    m.color = color;
    mallas.push_back(m);
}
// Recorre los nodos del modelo (cada nodo puede tener una malla y nodos hijos).
// La matriz global de un nodo = matriz del padre * matriz propia
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
    // se llama a si misma para los hijos
}
// Carga un archivo.glb o.gltf y devuelve la lista de mallas
std::vector<Malla> cargarGLTF(const char* ruta, glm::vec3& minB, glm::vec3& maxB)
{
    std::vector<Malla> mallas;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn, r = ruta;
    // .glb es binario y .gltf es texto, se cargan de forma distinta
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
//inicia el programa, crea la ventana, shaders, carga a Mario y entra al loop principal
int main()
{
	//crea la ventana y el contexto OpenGL 3.3
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
    glfwMakeContextCurrent(window);// activa el contexto de OpenGL de esta ventana
    gladLoadGL();
    glViewport(0, 0, 800, 800);
    glEnable(GL_DEPTH_TEST);
    // Registrar las funciones del mouse para que GLFW las llame
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
    // Valores que no cambian: se envian UNA sola vez, antes del ciclo
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(luzPos));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(luzColor));
    // Ubicaciones de los uniforms que SI cambian en cada frame
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLint colorLoc = glGetUniformLocation(shaderProgram, "baseColor");

    glUseProgram(shaderLinea);
    glUniformMatrix4fv(glGetUniformLocation(shaderLinea, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    GLint viewLineaLoc = glGetUniformLocation(shaderLinea, "view");
    GLint colorLineaLoc = glGetUniformLocation(shaderLinea, "color");

    float tiempoAnterior = (float)glfwGetTime();// para calcular dt (tiempo entre frames)
    //CICLO PRINCIPAL(cada vuelta = 1 frame)
    while (!glfwWindowShouldClose(window))
    {
        //Tiempo y teclado
        float tiempo = (float)glfwGetTime();
        float dt = tiempo - tiempoAnterior;
        tiempoAnterior = tiempo;

        procesarTeclado(window, dt);//Limpiar la pantalla

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Camara orbital alrededor de "objetivo" (que se mueve con el teclado)
        // La posicion se calcula con coordenadas esfericas: yaw, pitch y distancia
        glm::vec3 camaraPos = objetivo + distancia * glm::vec3(
            cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw));// MATRIZ DE VISTA: lookAt(desde donde mira, hacia donde mira, cual es "arriba")
        glm::mat4 view = glm::lookAt(camaraPos, objetivo, glm::vec3(0.0f, 1.0f, 0.0f));

        // Posicion de Mario sobre la curva (ida y vuelta)
         // "ciclo" va de 0 a 2: de 0 a 1 es la ida y de 1 a 2 la vuelta
        float ciclo = fmod(tiempo / duracionRecorrido, 2.0f);
        float t = (ciclo < 1.0f) ? ciclo : 2.0f - ciclo;
        float sentido = (ciclo < 1.0f) ? 1.0f : -1.0f;
        glm::vec3 tangente = tangenteBezier(t) * sentido;
        float angulo = atan2(tangente.x, tangente.z);
        // MATRIZ DE MOVIMIENTO = traslacion al punto de la curva * rotacion
        glm::mat4 movimiento = glm::translate(glm::mat4(1.0f), puntoBezier(t) + glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(glm::mat4(1.0f), angulo + giroExtra, glm::vec3(0.0f, 1.0f, 0.0f));

        // Dibujar a Mario
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));// se usan los shaders de Mario (con Phong)
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(camaraPos));
        glActiveTexture(GL_TEXTURE0);
        // Cada parte de Mario se dibuja por separado
        for (const Malla& m : mallas)
        {
            // MATRIZ MODEL de esta parte (se lee de derecha a izquierda):
              //1) m.transform = posicion de la parte dentro del modelo
        //   2) ajusteModelo = centrar y escalar el modelo
        //   3) movimiento = llevarlo a la curva y girarlo
            glm::mat4 modelMalla = movimiento * ajusteModelo * m.transform;
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMalla));
            glUniform4fv(colorLoc, 1, glm::value_ptr(m.color));
            glBindTexture(GL_TEXTURE_2D, m.textura);
            glBindVertexArray(m.VAO);
            glDrawElements(GL_TRIANGLES, m.numIndices, GL_UNSIGNED_INT, 0);
            // dibuja los triangulos
        }

        // Dibujar la curva de Bezier, su poligono de control y los puntos
      
        glUseProgram(shaderLinea);// ahora se usan los shaders de lineas
        glUniformMatrix4fv(viewLineaLoc, 1, GL_FALSE, glm::value_ptr(view));

        glUniform3f(colorLineaLoc, 0.2f, 1.0f, 0.3f); // curva (verde)
        glBindVertexArray(vaoCurva);
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)puntosCurva.size());

        glUniform3f(colorLineaLoc, 0.6f, 0.6f, 0.6f); // poligono de control (gris)
        glBindVertexArray(vaoControl);
        glDrawArrays(GL_LINE_STRIP, 0, 4);

        glUniform3f(colorLineaLoc, 1.0f, 0.9f, 0.1f); // puntos de control (amarillo)
        glDrawArrays(GL_POINTS, 0, 4);
        //Mostrar el frame
        // Se dibuja en un buffer trasero y luego se intercambia con el delantero,
        // asi nunca se ve el dibujo a medias
        glfwSwapBuffers(window);
        glfwPollEvents();// procesa teclado y mouse (sin esto la ventana se congela)
    }
    //LIMPIEZA al cerrar el programa
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