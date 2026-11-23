

#include <iostream>   
#include <vector>     
#include <cmath>      

#include <glad/glad.h>        
#include <GLFW/glfw3.h>       


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>// Vectores y matrices
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>             
#include <assimp/postprocess.h>       

#include "Shader.h"                


// VENTANA
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;

// CÁMARA
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);  // Posición de la cámara
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f); // Hacia dónde mira
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);  // Qué es "arriba"

// TIEMPO
float deltaTime = 0.0f;   // Tiempo entre un frame y el anterior
float lastFrame = 0.0f;   // Momento del frame anterior

// MOUSE
bool  firstMouse = true;  // Primera vez que se mueve el mouse
float yaw = -90.0f;     // Giro horizontal
float pitch = 0.0f;       // Giro vertical
float lastX = 400.0f;     // Última posición X del mouse
float lastY = 400.0f;     // Última posición Y del mouse

// CURVA DE BÉZIER
// 4 puntos de control: el primero y el último son los extremos de la curva,
// los del medio le dan la forma.
glm::vec3 bezierP[4] = {
    glm::vec3(-2.0f, -1.0f,  0.0f),   // P0: inicio
    glm::vec3(-1.0f,  2.5f, -1.0f),   // P1
    glm::vec3(1.0f, -2.5f, -1.0f),   // P2
    glm::vec3(2.0f,  1.0f,  0.0f)    // P3: final
};

// Punto de la curva en t (t de 0 a 1)
glm::vec3 bezierPoint(float t) {
    float u = 1.0f - t;
    return u * u * u * bezierP[0] + 3.0f * u * u * t * bezierP[1] + 3.0f * u * t * t * bezierP[2] + t * t * t * bezierP[3];
}

// Dirección de la curva en t (hacia dónde avanza)
glm::vec3 bezierTangent(float t) {
    float u = 1.0f - t;
    return 3.0f * u * u * (bezierP[1] - bezierP[0]) + 6.0f * u * t * (bezierP[2] - bezierP[1]) + 3.0f * t * t * (bezierP[3] - bezierP[2]);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);

// MALLA: una parte del modelo con sus datos en la GPU

struct Mesh {
    GLuint VAO, VBO, EBO;   // VAO: cómo leer los datos. VBO: vértices. EBO: índices
    size_t indexCount;      // Cuántos índices dibujar
};

// Convierte una malla de Assimp en una malla de OpenGL
Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        // Posición
        vertices.push_back(mesh->mVertices[i].x);
        vertices.push_back(mesh->mVertices[i].y);
        vertices.push_back(mesh->mVertices[i].z);

        // Normal (si no hay, usa (0, 0, 1))
        if (mesh->mNormals) {
            vertices.push_back(mesh->mNormals[i].x);
            vertices.push_back(mesh->mNormals[i].y);
            vertices.push_back(mesh->mNormals[i].z);
        }
        else {
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
            vertices.push_back(1.0f);
        }

        // Coordenadas de textura (si no hay, usa (0, 0))
        if (mesh->mTextureCoords[0]) {
            vertices.push_back(mesh->mTextureCoords[0][i].x);
            vertices.push_back(mesh->mTextureCoords[0][i].y);
        }
        else {
            vertices.push_back(0.0f);
            vertices.push_back(0.0f);
        }
    }

    // Índices de los triángulos
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    Mesh myMesh;
    myMesh.indexCount = indices.size();

    glGenVertexArrays(1, &myMesh.VAO);
    glGenBuffers(1, &myMesh.VBO);
    glGenBuffers(1, &myMesh.EBO);

    glBindVertexArray(myMesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, myMesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, myMesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Cada vértice: 3 posición + 3 normal + 2 UV = 8 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return myMesh;
}

// Recorre el árbol del modelo y convierte todas sus mallas
void processNode(aiNode* node, const aiScene* scene, std::vector<Mesh>& modelMeshes) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        modelMeshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene, modelMeshes);
    }
}

// FUNCIÓN PRINCIPAL
int main()
{
    // 1. Ventana
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Sillon 3D - Control Libre y Giro", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Fallo al crear la ventana de GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Error al inicializar GLAD" << std::endl;
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glEnable(GL_DEPTH_TEST);

    //  2. Shaders (usando la clase Shader) 
    Shader modelShader(ShaderSources::MODEL_VS, ShaderSources::MODEL_FS);   // Shader del sillón
    Shader curveShader(ShaderSources::CURVE_VS, ShaderSources::CURVE_FS);   // Shader de la curva

    //  3. Puntos de la curva 
    const int CURVE_SEGMENTS = 100;
    std::vector<float> curvePts;
    for (int i = 0; i <= CURVE_SEGMENTS; i++) {
        glm::vec3 p = bezierPoint(i / (float)CURVE_SEGMENTS);
        curvePts.push_back(p.x);
        curvePts.push_back(p.y);
        curvePts.push_back(p.z);
    }

    GLuint curveVAO, curveVBO;
    glGenVertexArrays(1, &curveVAO);
    glGenBuffers(1, &curveVBO);
    glBindVertexArray(curveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, curveVBO);
    glBufferData(GL_ARRAY_BUFFER, curvePts.size() * sizeof(float), curvePts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //  4. Textura 
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Assimp ya voltea las UVs, así que stb no debe voltear la imagen
    stbi_set_flip_vertically_on_load(false);
    int width, height, nrChannels;
    unsigned char* data = stbi_load("textures/sillon.jpeg", &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        std::cout << "Textura cargada con exito." << std::endl;
    }
    else
    {
        std::cout << "Advertencia: No se encontro textures/sillon.jpeg, usando respaldo." << std::endl;
        unsigned char fallback[] = { 180, 100, 50 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fallback);
    }

    //  5. Cargar el modelo 
    std::vector<Mesh> modelMeshes;
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("scene.gltf", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_FixInfacingNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "Error Assimp: " << importer.GetErrorString() << std::endl;
    }
    else {
        processNode(scene->mRootNode, scene, modelMeshes);
    }

    //  6. Bucle principal (un ciclo por cada frame) 
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.12f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matrices de proyección y vista
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // Posición del sillón sobre la curva (10 segundos por recorrido)
        float t = std::fmod(currentFrame * 0.1f, 1.0f);
        glm::vec3 pos = bezierPoint(t);
        glm::vec3 tangent = bezierTangent(t);
        float heading = std::atan2(tangent.x, tangent.z);

        // Matriz de modelo: el orden va de adentro hacia afuera
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);                                              // 1. Lo mueve a la curva
        model = glm::rotate(model, heading, glm::vec3(0.0f, 1.0f, 0.0f));                // 2. Lo orienta
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));   // 3. Corrige la posición inicial
        model = glm::rotate(model, currentFrame * 0.4f, glm::vec3(0.0f, 0.0f, 1.0f));    // 4. Lo hace girar

        glm::mat4 mvp = projection * view * model;

        //  Dibujar el sillón 
        modelShader.Use();
        glBindTexture(GL_TEXTURE_2D, texture);
        modelShader.SetMat4("uMVP", mvp);
        modelShader.SetMat4("uModel", model);
        modelShader.SetMat4("uView", view);

        for (const Mesh& mesh : modelMeshes)
        {
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount), GL_UNSIGNED_INT, nullptr);
        }

        //  Dibujar la curva 
        curveShader.Use();
        curveShader.SetMat4("uMVP", projection * view);
        glBindVertexArray(curveVAO);
        glDrawArrays(GL_LINE_STRIP, 0, CURVE_SEGMENTS + 1);

        glBindVertexArray(0);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //  7. Liberar memoria 
    for (const Mesh& mesh : modelMeshes)
    {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
    }

    glDeleteVertexArrays(1, &curveVAO);
    glDeleteBuffers(1, &curveVBO);
    curveShader.Delete();
    modelShader.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// TECLADO: mueve la cámara
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

// MOUSE: gira la cámara
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}