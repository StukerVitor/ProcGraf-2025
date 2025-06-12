#pragma once // Garante que este arquivo de cabeçalho seja incluído apenas uma vez durante a compilação.

#include <string>      // Necessário para usar std::string.
#include <glad/glad.h> // Inclui a biblioteca GLAD para funcionalidades OpenGL (como GLuint).

/**
 * @class Shader
 * @brief Uma classe que encapsula a lógica de compilação e linkagem de shaders do OpenGL.
 * @details Esta classe abstrai o processo de ler o código-fonte de shaders (de arquivos ou
 * diretamente de strings), compilá-los, linká-los em um programa de shader
 * e gerenciar seu ID. Isso limpa o código principal e promove a reutilização.
 */
class Shader
{
private:
    // O ID do programa shader OpenGL. Um programa é a combinação de um vertex shader,
    // um fragment shader e, opcionalmente, outros shaders (geometria, tesselação).
    GLuint id;

public:
    /**
     * @brief Construtor que lê, compila e linka shaders a partir de arquivos.
     * @param vertexShaderPath O caminho do sistema de arquivos para o código-fonte do vertex shader.
     * @param fragmentShaderPath O caminho do sistema de arquivos para o código-fonte do fragment shader.
     */
    Shader(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

    /**
     * @brief Construtor sobrecarregado que compila e linka shaders a partir de código-fonte embutido.
     * @param vertexShaderCode Uma string C contendo o código-fonte do vertex shader.
     * @param fragmentShaderCode Uma string C contendo o código-fonte do fragment shader.
     * @param inlineCode Um booleano para diferenciar este construtor do construtor por arquivo.
     */
    Shader(const char* vertexShaderCode, const char* fragmentShaderCode, bool inlineCode = false);

    /**
     * @brief Define a uniforme da textura no shader.
     * @details Configura a uniforme do tipo 'sampler2D' para usar a unidade de textura 0.
     * Deve ser chamada após ativar o shader com glUseProgram.
     */
    void setTextureUniform();

    /**
     * @brief Getter para obter o ID do programa shader.
     * @return O ID (GLuint) do programa shader, para ser usado com glUseProgram.
     */
    GLuint getId() { return id; }
};