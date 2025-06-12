#include "Shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

/**
 * @brief Implementação do construtor que lê shaders de arquivos.
 */
Shader::Shader(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    // Garante que ifstream possa lançar exceções em caso de falha.
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // Abre os arquivos.
        vShaderFile.open(vertexShaderPath);
        fShaderFile.open(fragmentShaderPath);
        std::stringstream vShaderStream, fShaderStream;

        // Lê o conteúdo dos arquivos para os stringstreams.
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        // Fecha os arquivos.
        vShaderFile.close();
        fShaderFile.close();

        // Converte os stringstreams para strings.
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();
    
    GLuint vertex, fragment;
    // --- COMPILAÇÃO DO VERTEX SHADER ---
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    // Checa por erros de compilação.
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // --- COMPILAÇÃO DO FRAGMENT SHADER ---
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    // Checa por erros de compilação.
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // --- CRIAÇÃO E LINKAGEM DO PROGRAMA SHADER ---
    this->id = glCreateProgram(); // Cria um programa vazio.
    glAttachShader(id, vertex);     // Anexa o vertex shader.
    glAttachShader(id, fragment);   // Anexa o fragment shader.
    glLinkProgram(id);              // Linka os shaders anexados.
    // Checa por erros de linkagem.
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(id, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Após a linkagem bem-sucedida, os objetos de shader individuais não são mais necessários.
    // Eles já foram compilados e incorporados ao programa.
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

/**
 * @brief Implementação do construtor que usa código-fonte inline.
 */
Shader::Shader(const char* vertexShaderCode, const char* fragmentShaderCode, bool inlineCode) {
    // Este `if` é uma maneira de reutilizar o construtor por arquivo se a flag `inlineCode` for falsa.
    // No entanto, a implementação atual não depende disso.
    if (!inlineCode) {
        *this = Shader(std::string(vertexShaderCode), std::string(fragmentShaderCode));
        return;
    }

    const GLchar* vShaderCode = vertexShaderCode;
    const GLchar* fShaderCode = fragmentShaderCode;

    GLuint vertex, fragment;
    // --- COMPILAÇÃO DO VERTEX SHADER ---
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // --- COMPILAÇÃO DO FRAGMENT SHADER ---
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    // --- CRIAÇÃO E LINKAGEM DO PROGRAMA SHADER ---
    this->id = glCreateProgram();
    glAttachShader(id, vertex);
    glAttachShader(id, fragment);
    glLinkProgram(id);
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(id, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Libera os recursos dos shaders individuais.
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

/**
 * @brief Implementação do método para definir a uniforme de textura.
 */
void Shader::setTextureUniform() {
    // Obtém a localização da uniforme "tex" no shader e a configura para o valor 0.
    // Isso significa que esta uniforme sampler2D usará a unidade de textura GL_TEXTURE0.
    glUniform1i(glGetUniformLocation(this->id, "tex"), 0);
}