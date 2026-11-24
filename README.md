# MarioOpenGL

Proyecto de graficación en C++ con OpenGL 3.3. Carga el modelo 3D de Mario (archivo .glb), lo ilumina con el modelo de Phong y lo hace caminar sobre una curva de Bézier cúbica. La cámara se controla con el mouse y el teclado.

**Autora:** Maria Paula Amaya Prieto

Código: 6014438

Archivos principales:

- hola.cpp: programa principal (ventana, cámara, curva de Bézier, carga del modelo y ciclo de dibujo).
- ShaderClass.cpp y ShaderClass.h: leen los shaders desde archivos, los compilan y crean el programa.
- VAO, VBO y EBO (.cpp y .h): clases que manejan los buffers de OpenGL.
- shaders/modelo.vert y shaders/modelo.frag: shaders de Mario con iluminación Phong.
- shaders/linea.vert y shaders/linea.frag: shaders de la curva de Bézier y sus puntos.
- Mario.glb: modelo 3D del personaje.

Controles

- W / S o flechas arriba / abajo: mover la cámara adelante y atrás.
- A / D: mover la cámara a los lados.
- Flechas izquierda / derecha: girar la cámara.
- Q / E: girar a Mario sobre su eje.
- Mouse (click izquierdo y arrastrar): orbitar la cámara.
- Rueda del mouse: acercar o alejar.

Arquitectura del software

Librerías usadas:

- GLFW: crea la ventana y recibe teclado y mouse.
- GLAD: carga las funciones de OpenGL.
- GLM: matrices y vectores (traslación, rotación, escala, perspectiva).
- stb_image: lectura de texturas.
- tinygltf: lectura de archivos .glb / .gltf.

Módulos del programa:

1. Shaders: se cargan desde archivos externos. El vertex shader calcula la posición final de cada vértice con las matrices model, view y projection. El fragment shader calcula el color con iluminación de Phong (ambiente + difusa + especular).
2. Cámara: orbital con matriz de vista glm::lookAt. El teclado traslada el punto al que mira y el mouse la rota.
3. Carga de glTF: recorre los nodos del modelo, lee posiciones, coordenadas de textura y normales, y crea una malla (VAO, VBO, EBO y textura) por cada parte.
4. Curva de Bézier: curva cúbica con 4 puntos de control. Mario recorre la curva de ida y vuelta, y su rotación se calcula con la tangente de la curva.
5. Ciclo principal: en cada frame lee el teclado, actualiza la cámara, mueve a Mario, dibuja el modelo y después la curva con sus puntos de control.

En conclusion:

Mi proyecto está desarrollado en C++ utilizando principalmente programación estructurada o procedimental. Organicé el código mediante funciones para separar las tareas, como la carga del modelo 3D, el procesamiento de sus mallas, el control de la cámara, la lectura del teclado y el cálculo de la curva de Bézier. También utilicé una estructura llamada Malla para agrupar los atributos de cada parte del modelo. Además, aproveché clases y estructuras de bibliotecas externas como GLM, GLFW y TinyGLTF. No implementé clases propias, por lo que no es un proyecto basado principalmente en programación orientada a objetos
