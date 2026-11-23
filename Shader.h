#pragma once

// CLASE SHADER

// Esta clase se encarga de:
//   - Compilar el vertex shader y el fragment shader.
//   - Unirlos en un "programa" de OpenGL.
//   - Activar el programa cuando se va a dibujar.
//   - Enviarle matrices (uniforms) al shader.
// Los códigos de los shaders también están aquí, en la sección de abajo.


#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader {
public:
    GLuint ID;   // Número que identifica el programa de shaders en OpenGL

    // Recibe el código del vertex y del fragment shader, los compila y los une
    Shader(const char* vertexSource, const char* fragmentSource) {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);      // Crea el vertex shader
        glShaderSource(vs, 1, &vertexSource, NULL);        // Le pasa su código
        glCompileShader(vs);                               // Lo compila

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);    // Crea el fragment shader
        glShaderSource(fs, 1, &fragmentSource, NULL);
        glCompileShader(fs);

        ID = glCreateProgram();                            // Crea el programa que une ambos shaders
        glAttachShader(ID, vs);
        glAttachShader(ID, fs);
        glLinkProgram(ID);

        glDeleteShader(vs);                                // Ya no hacen falta los shaders sueltos
        glDeleteShader(fs);
    }

    // Activa este programa: a partir de aquí, lo que se dibuje usa estos shaders
    void Use() const {
        glUseProgram(ID);
    }

    // Envía una matriz 4x4 a una variable uniform del shader, por su nombre
    void SetMat4(const char* name, const glm::mat4& mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    // Borra el programa de la memoria de la tarjeta gráfica
    void Delete() const {
        glDeleteProgram(ID);
    }
};

// CÓDIGO DE LOS SHADERS
namespace ShaderSources {

    // Shader del sillón (con iluminación Phong) 

    // Vertex shader: calcula dónde va cada vértice en la pantalla
    static const char* MODEL_VS =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"          // Posición del vértice
        "layout (location = 1) in vec3 aNormal;\n"       // Normal (hacia dónde mira la superficie)
        "layout (location = 2) in vec2 aTexCoord;\n"     // Coordenadas de textura (UV)
        "out vec3 FragPos;\n"                            // Posición en el mundo, para el fragment shader
        "out vec3 Normal;\n"                             // Normal ya transformada
        "out vec2 TexCoord;\n"                           // UV, para el fragment shader
        "uniform mat4 uMVP;\n"                           // Modelo * vista * proyección
        "uniform mat4 uModel;\n"                         // Solo el modelo (ubicación del sillón)
        "void main()\n"
        "{\n"
        "   FragPos = vec3(uModel * vec4(aPos, 1.0));\n"
        "   Normal = mat3(transpose(inverse(uModel))) * aNormal;\n"
        "   TexCoord = aTexCoord;\n"
        "   gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\0";

    // Fragment shader: calcula el color de cada píxel con la luz Phong
    static const char* MODEL_FS =
        "#version 330 core\n"
        "out vec4 FragColor;\n"                          // Color final del píxel
        "in vec3 FragPos;\n"
        "in vec3 Normal;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D ourTexture;\n"                // La textura del sillón
        "uniform mat4 uView;\n"                          // Vista, para saber dónde está la cámara
        "void main()\n"
        "{\n"
        "   vec3 lightPos = vec3(2.0, 3.0, 2.0);\n"      // Posición de la luz
        "   vec3 lightColor = vec3(1.0, 1.0, 1.0);\n"    // Color de la luz
        "   vec3 viewPos = vec3(inverse(uView)[3]);\n"   // Posición de la cámara
        "   vec3 objColor = texture(ourTexture, TexCoord).rgb;\n"
        "   vec3 ambient = 0.2 * lightColor;\n"          // Luz base
        "   vec3 norm = normalize(Normal);\n"
        "   vec3 lightDir = normalize(lightPos - FragPos);\n"
        "   float diff = max(dot(norm, lightDir), 0.0);\n"
        "   vec3 diffuse = diff * lightColor;\n"         // Luz difusa (mate)
        "   vec3 viewDir = normalize(viewPos - FragPos);\n"
        "   vec3 reflectDir = reflect(-lightDir, norm);\n"
        "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);\n"
        "   vec3 specular = 0.5 * spec * lightColor;\n"  // Luz especular (brillo)
        "   FragColor = vec4((ambient + diffuse + specular) * objColor, 1.0);\n"
        "}\0";

    //  Shader de la curva (color fijo, sin luz) 

    static const char* CURVE_VS =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "uniform mat4 uMVP;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\0";

    static const char* CURVE_FS =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = vec4(1.0, 0.8, 0.2, 1.0);\n"     // Amarillo
        "}\0";
}
