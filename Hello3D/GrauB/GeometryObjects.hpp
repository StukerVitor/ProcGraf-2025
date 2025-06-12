#ifndef GEOMETRYOBJECTS_HPP
#define GEOMETRYOBJECTS_HPP

// --- BIBLIOTECAS E INCLUDES ---
// Inclusão da implementação da biblioteca stb_image para carregamento de imagens (texturas).
#define STB_IMAGE_IMPLEMENTATION
#include "../../Common/include/stb_image.h"

// Includes padrão do C++ e da biblioteca GLM para matemática de vetores e matrizes.
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <numeric>
#include <vector>
#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <array>
#include <glad/glad.h> // GLAD para carregar ponteiros de funções do OpenGL.

// ----------------------------------------------------------------------------
// ESTRUTURAS AUXILIARES DE GEOMETRIA
// ----------------------------------------------------------------------------

/**
 * @struct Vec2
 * @brief Representa um vetor 2D, usado primariamente para coordenadas de textura (UV).
 */
struct Vec2 {
    float u, v; // u é a coordenada horizontal (equivalente a X), v é a vertical (equivalente a Y).
    // Sobrecarga do operador de igualdade para permitir comparações, usado em std::find.
    bool operator==(const Vec2& other) const {
        return u == other.u && v == other.v;
    }
};

/**
 * @struct Vec3
 * @brief Representa um vetor 3D, usado para posições de vértice (x,y,z) e vetores normais.
 */
struct Vec3 {
    float x, y, z;
    // Sobrecarga do operador de igualdade para comparações, essencial para a escrita de arquivos .obj.
    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * @struct Face
 * @brief Representa uma face poligonal (tipicamente um triângulo).
 * @details Armazena os dados dos vértices que compõem a face. Importante notar que
 * os dados são armazenados por face, refletindo a estrutura de um arquivo .obj.
 */
struct Face {
    std::vector<Vec3> verts; // Posições dos vértices da face.
    std::vector<Vec3> norms; // Normais correspondentes a cada vértice.
    std::vector<Vec2> texts; // Coordenadas de textura (UV) de cada vértice.

    // Métodos para adicionar dados de um vértice à face.
    void addVert(const Vec3& v) { verts.push_back(v); }
    void addNorm(const Vec3& n) { norms.push_back(n); }
    void addText(const Vec2& t) { texts.push_back(t); }
};

/**
 * @struct Group
 * @brief Representa um grupo de faces que compartilham o mesmo material.
 * @details No formato .obj, a diretiva "usemtl" define o material para as faces subsequentes.
 * Esta struct modela esse comportamento.
 */
struct Group {
    std::string name;     // Nome do grupo (opcional, pode ser o nome do material).
    std::string mtlName;  // Nome do material associado a este grupo.
    std::vector<Face> faces; // Lista de faces que pertencem a este grupo.

    Group(const std::string& _name, const std::string& _mtl)
        : name(_name), mtlName(_mtl) {}

    void addFace(const Face& f) { faces.push_back(f); }
};

// ----------------------------------------------------------------------------
// ESTRUTURA DE VÉRTICE PARA A GPU
// ----------------------------------------------------------------------------

/**
 * @struct Vertex
 * @brief Define o layout de dados de um único vértice como será enviado para a GPU.
 * @details Esta é uma estrutura de dados "intercalada" (interleaved). Em vez de ter VBOs
 * separados para posições, UVs e normais, todos os dados de um vértice
 * são armazenados sequencialmente em memória. Isso geralmente melhora a
 * localidade de cache da GPU, resultando em melhor performance.
 */
struct Vertex {
    // Atributos do vértice
    float x, y, z;    // Posição (location = 0 no shader)
    float s, t;       // Coordenadas de Textura (UV) (location = 1 no shader)
    float nx, ny, nz; // Vetor Normal (location = 3 no shader)
};

// ----------------------------------------------------------------------------
// ESTRUTURA DE MATERIAL
// ----------------------------------------------------------------------------

/**
 * @struct Material
 * @brief Contém todas as propriedades de um material, conforme especificado em arquivos .mtl.
 * @details Estas propriedades definem como a superfície de um objeto interage com a luz.
 */
struct Material {
    // Ka: Coeficiente de reflexão Ambiente (cor base sob luz ambiente)
    float kaR = 0.0f, kaG = 0.0f, kaB = 0.0f;
    // Kd: Coeficiente de reflexão Difusa (cor principal do objeto sob luz direta)
    float kdR = 0.0f, kdG = 0.0f, kdB = 0.0f;
    // Ks: Coeficiente de reflexão Especular (cor do brilho/reflexo)
    float ksR = 0.0f, ksG = 0.0f, ksB = 0.0f;
    // Ns: Expoente especular (shininess). Valores mais altos criam brilhos menores e mais focados.
    float ns = 0.0f;
    // map_Kd: Nome do arquivo da textura que será mapeada na cor difusa.
    std::string textureName;
};

// ----------------------------------------------------------------------------
// FUNÇÕES AUXILIARES DE SETUP (OPENGL)
// ----------------------------------------------------------------------------

/**
 * @brief Carrega as propriedades de um material a partir de um arquivo .mtl.
 * @param path O caminho para o arquivo .mtl.
 * @return Um objeto Material preenchido com os dados do arquivo.
 */
static Material setupMtl(const std::string& path)
{
    Material material{};
    std::ifstream file(path);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Falha ao abrir o arquivo MTL: " << path << std::endl;
        return material;
    }

    // Lê o arquivo linha por linha
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string type;
        ss >> type; // O primeiro token da linha define o tipo de dado (e.g., "Ka", "Kd").

        // Preenche a struct material com base no tipo
        if (type == "Ka")
            ss >> material.kaR >> material.kaG >> material.kaB;
        else if (type == "Kd")
            ss >> material.kdR >> material.kdG >> material.kdB;
        else if (type == "Ks")
            ss >> material.ksR >> material.ksG >> material.ksB;
        else if (type == "Ns")
            ss >> material.ns;
        else if (type == "map_Kd")
            ss >> material.textureName; // Armazena o nome do arquivo da textura
    }

    file.close();
    return material;
}

/**
 * @brief Carrega um arquivo de imagem e cria uma textura OpenGL.
 * @param filename O caminho para o arquivo de imagem.
 * @return O ID da textura OpenGL gerada.
 */
static GLuint setupTexture(const std::string& filename)
{
    GLuint texId;
    glGenTextures(1, &texId); // Gera um ID de textura
    glBindTexture(GL_TEXTURE_2D, texId); // Ativa a textura para configuração

    // Define os parâmetros de wrapping (como a textura se comporta fora do intervalo [0,1])
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Define os parâmetros de filtering (como a textura é amostrada ao ser aumentada ou diminuída)
    // GL_LINEAR_MIPMAP_LINEAR usa mipmaps para minificação (melhor qualidade) e interpolação linear para magnificação.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // A biblioteca stb_image carrega imagens com a origem no canto superior esquerdo,
    // enquanto o OpenGL espera a origem no canto inferior esquerdo. Esta linha corrige isso.
    stbi_set_flip_vertically_on_load(true);

    int w, h, channels;
    // Carrega os dados da imagem do arquivo para a memória RAM.
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &channels, 0);
    if (data) {
        // Determina o formato da imagem (RGB ou RGBA)
        GLenum fmt = (channels == 3 ? GL_RGB : GL_RGBA);
        // Envia os dados da imagem da RAM para a VRAM (memória da GPU)
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        // Gera mipmaps automaticamente para a textura.
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Falha ao carregar a textura: " << filename << std::endl;
    }
    // Libera a memória da RAM usada para carregar a imagem.
    stbi_image_free(data);
    return texId;
}

/**
 * @brief Cria e configura um Vertex Array Object (VAO) e um Vertex Buffer Object (VBO).
 * @param vertices Um vetor de vértices com dados intercalados (posição, UV, normal).
 * @return O ID do VAO configurado.
 */
static GLuint setupGeometry(const std::vector<Vertex>& vertices)
{
    GLuint VBO, VAO;
    // 1. Criar e preencher o VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Envia os dados do vetor de vértices para o VBO na GPU.
    // GL_STATIC_DRAW significa que os dados não serão modificados frequentemente.
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // 2. Criar e configurar o VAO
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // 3. Configurar os ponteiros de atributos de vértice (Vertex Attribute Pointers)
    // Isso informa ao OpenGL como interpretar os dados brutos dentro do VBO.

    // Atributo de Posição (x, y, z) -> location 0 no shader
    // - Índice do atributo: 0
    // - Número de componentes: 3 (vec3)
    // - Tipo dos componentes: GL_FLOAT
    // - Normalizado: GL_FALSE
    // - Stride (passo): sizeof(Vertex) - distância em bytes entre vértices consecutivos.
    // - Offset (deslocamento): (GLvoid*)0 - onde o atributo começa dentro da struct.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // Atributo de Coordenada de Textura (s, t) -> location 1 no shader
    // - Offset: 3 floats (o tamanho da posição) a partir do início da struct.
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Atributo de Normal (nx, ny, nz) -> location 3 no shader
    // - Offset: 5 floats (posição + UV) a partir do início da struct.
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);

    // Desvincula o VBO e o VAO para evitar modificações acidentais.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return VAO;
}

// ----------------------------------------------------------------------------
// ESTRUTURA MESH (MALHA GEOMÉTRICA)
// ----------------------------------------------------------------------------
/**
 * @struct Mesh
 * @brief Representa a geometria "pura" de um objeto 3D.
 * @details Contém os dados dos vértices, normais, coordenadas de textura, e a forma
 * como eles se conectam (grupos e faces). Também gerencia o VAO associado.
 */
struct Mesh {
    // Vetores paralelos que armazenam todos os dados únicos de vértices, UVs e normais da malha.
    std::vector<Vec3>   vertices;
    std::vector<Vec2>   mappings;
    std::vector<Vec3>   normals;
    // Lista de grupos de faces, que associam partes da malha a materiais.
    std::vector<Group>  groups;
    // ID do Vertex Array Object que encapsula o estado de renderização desta malha.
    GLuint              VAO = 0;

    Mesh() = default;

    /**
     * @brief Construtor que monta a malha a partir de dados brutos e separados.
     * @details Este construtor é ideal para quando os dados são lidos de um formato
     * como .obj, onde vértices, normais e UVs são listados separadamente.
     * @param verts Vetor de posições de vértices.
     * @param maps Vetor de coordenadas de textura.
     * @param norms Vetor de normais.
     * @param groups_ Vetor de grupos de faces.
     */
    Mesh(const std::vector<Vec3>& verts,
        const std::vector<Vec2>& maps,
        const std::vector<Vec3>& norms,
        const std::vector<Group>& groups_)
        : vertices(verts), mappings(maps), normals(norms), groups(groups_)
    {
        // Para configurar o VAO, precisamos de um vetor único e intercalado.
        // Este loop cria esse vetor 'interleaved' a partir dos vetores 'paralelos'.
        std::vector<Vertex> interleaved;
        interleaved.reserve(vertices.size()); // Pre-aloca memória para eficiência.
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vertex v;
            v.x = vertices[i].x;
            v.y = vertices[i].y;
            v.z = vertices[i].z;
            v.s = mappings[i].u;
            v.t = mappings[i].v;
            v.nx = normals[i].x;
            v.ny = normals[i].y;
            v.nz = normals[i].z;
            interleaved.push_back(v);
        }
        // Com o vetor intercalado pronto, chama a função para criar o VAO/VBO.
        VAO = setupGeometry(interleaved);
    }

    /**
     * @brief Construtor que monta a malha a partir de um vetor já intercalado e índices.
     * @details Usado para geometria gerada proceduralmente, como a pista de corrida,
     * onde é mais fácil gerar os dados já no formato 'Vertex'.
     * @param interleavedVerts Vetor de vértices já no formato intercalado para a GPU.
     * @param indices Vetor de índices que definem os triângulos.
     * @param groupName Nome para o grupo padrão a ser criado.
     * @param mtlName Nome do material para o grupo padrão.
     */
    Mesh(const std::vector<Vertex>& interleavedVerts,
        const std::vector<unsigned int>& indices,
        const std::string& groupName = "",
        const std::string& mtlName = "")
    {
        // 1) Embora os dados já estejam intercalados para a GPU, a estrutura Mesh também
        //    mantém vetores paralelos. Este bloco os preenche.
        vertices.reserve(interleavedVerts.size());
        mappings.reserve(interleavedVerts.size());
        normals.reserve(interleavedVerts.size());
        for (const auto& v : interleavedVerts) {
            vertices.push_back(Vec3{ v.x,  v.y,  v.z });
            mappings.push_back(Vec2{ v.s,  v.t });
            normals.push_back(Vec3{ v.nx, v.ny, v.nz });
        }

        // 2) Se não houver um buffer de índices (EBO) fornecido, assume-se que os vértices
        //    estão ordenados como uma lista de triângulos.
        std::vector<unsigned int> idxs = indices;
        if (idxs.empty()) {
            idxs.resize(interleavedVerts.size());
            std::iota(idxs.begin(), idxs.end(), 0u); // Preenche com 0, 1, 2, 3, ...
        }

        // 3) Cria um único grupo para conter todas as faces da malha.
        groups.emplace_back(groupName, mtlName);
        Group* currentGroup = &groups.back();

        // 4) Itera sobre os índices, agrupando-os de 3 em 3 para formar triângulos.
        //    Cada triângulo é armazenado como um objeto 'Face'.
        for (size_t i = 0; i + 2 < idxs.size(); i += 3) {
            Face face;
            for (int k = 0; k < 3; ++k) {
                unsigned int idx = idxs[i + k];
                face.addVert(vertices[idx]);
                face.addText(mappings[idx]);
                face.addNorm(normals[idx]);
            }
            currentGroup->addFace(face);
        }

        // 5) Monta o VAO a partir do vetor intercalado recebido.
        VAO = setupGeometry(interleavedVerts);
    }
};

// ----------------------------------------------------------------------------
// ESTRUTURA OBJECT3D (OBJETO COMPLETO NA CENA)
// ----------------------------------------------------------------------------
/**
 * @struct Object3D
 * @brief Representa um objeto completo na cena, combinando geometria (Mesh),
 * material, textura e propriedades de transformação (posição, rotação, escala).
 */
struct Object3D {
    std::string            name;          // Nome do objeto (ex: "Carro", "Pista").
    std::string            objFilePath;   // Caminho para o arquivo .obj.
    std::string            mtlFilePath;   // Caminho para o arquivo .mtl.
    Mesh                   mesh;          // A malha geométrica do objeto.
    glm::vec3              scale;      // Fator de escala.
    glm::vec3              position;      // Posição no mundo.
    glm::vec3              rotation;      // Eixo de rotação.
    glm::vec3              angle;         // Ângulo de rotação em graus.
    GLuint                 incrementalAngle; // Flag para rotação incremental (não usado neste projeto).
    Material               material;      // Propriedades de material.
    GLuint                 textureID = 0; // ID da textura OpenGL.
    // Vetor de pontos que definem a trajetória de animação do objeto.
    std::vector<glm::vec3> animationPositions;

    Object3D() = default;

    /**
     * @brief Construtor "tudo-em-um" que carrega um objeto a partir de arquivos .obj e .mtl.
     * @details Este construtor realiza o parsing completo do arquivo .obj, organiza os dados,
     * constrói a Mesh correspondente, carrega o material e a textura.
     */
    Object3D(const std::string& _name,
        const std::string& objPath,
        const std::string& mtlPath,
        const glm::vec3& scale_ = glm::vec3(1.0f),
        const glm::vec3& pos_ = glm::vec3(0.0f),
        const glm::vec3& rot_ = glm::vec3(0.0f),
        const glm::vec3& ang_ = glm::vec3(0.0f),
        GLuint              incAng = 0)
        : name(_name), objFilePath(objPath), mtlFilePath(mtlPath), scale(scale_),
          position(pos_), rotation(rot_), angle(ang_), incrementalAngle(incAng)
    {
        // --- PARSING DO ARQUIVO .OBJ ---

        std::ifstream file(objFilePath);
        if (!file.is_open()) {
            std::cerr << "Falha ao abrir o arquivo OBJ: " << objFilePath << std::endl;
            return;
        }

        // Buffers temporários para ler todos os vértices, UVs e normais do arquivo.
        // Eles são chamados de "raw" porque estão na ordem em que aparecem no arquivo.
        std::string line;
        std::vector<glm::vec3> raw_positions;
        std::vector<glm::vec2> raw_texcoords;
        std::vector<glm::vec3> raw_normals;

        // Vetores finais que serão usados para construir a Mesh. Estes vetores
        // terão seus dados alinhados, ou seja, a posição i, UV i e normal i
        // correspondem ao mesmo vértice final na GPU.
        std::vector<Vec3> positions;
        std::vector<Vec2> texcoords;
        std::vector<Vec3> normals;
        std::vector<Group> groups;

        // 1 Começamos com um grupo padrão. Se o .obj não especificar nenhum `usemtl`,
        //    todas as faces pertencerão a este grupo.
        groups.emplace_back(name, "");
        Group* currentGroup = &groups.back();

        // 2 Leitura do arquivo .obj linha por linha.
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string type;
            ss >> type; // O primeiro token da linha define o tipo de dado.

            if (type == "v") { // Posição do vértice
                glm::vec3 pos;
                ss >> pos.x >> pos.y >> pos.z;
                raw_positions.push_back(pos);
            }
            else if (type == "vt") { // Coordenada de textura
                glm::vec2 uv;
                ss >> uv.x >> uv.y;
                raw_texcoords.push_back(uv);
            }
            else if (type == "vn") { // Vetor normal
                glm::vec3 n;
                ss >> n.x >> n.y >> n.z;
                raw_normals.push_back(n);
            }
            else if (type == "usemtl") { // Uso de material
                std::string mtl;
                ss >> mtl;
                // Cria um novo grupo para o material especificado.
                groups.emplace_back(mtl, mtl);
                currentGroup = &groups.back();
            }
            else if (type == "f") { // Definição de face
                // A lógica aqui lida com polígonos de N vértices, triangulando-os
                // usando uma abordagem de "triangle fan" a partir do primeiro vértice.
                std::vector<std::string> tokens;
                std::string tok;
                while (ss >> tok) tokens.push_back(tok);
                if (tokens.size() < 3) continue; // Pula se não for pelo menos um triângulo.

                for (size_t i = 1; i + 1 < tokens.size(); ++i) {
                    Face face;
                    // Monta um triângulo com o primeiro vértice, o vértice i, e o vértice i+1.
                    std::array<std::string, 3> idxs = { tokens[0], tokens[i], tokens[i + 1] };

                    for (int k = 0; k < 3; ++k) {
                        const std::string& vertStr = idxs[k]; // Ex: "1/2/3"
                        int vIdx = -1, tIdx = -1, nIdx = -1; // Índices de posição, textura e normal
                        
                        // --- Parsing do formato de face "v/t/n" ---
                        // O código a seguir é robusto para lidar com os diferentes formatos
                        // que uma face pode ter em um arquivo .obj (v, v/t, v//n, v/t/n).
                        size_t firstSlash = vertStr.find('/');
                        size_t secondSlash = (firstSlash == std::string::npos
                            ? std::string::npos
                            : vertStr.find('/', firstSlash + 1));

                        if (firstSlash == std::string::npos) { // Formato: "v"
                            vIdx = std::stoi(vertStr) - 1;
                        }
                        else if (secondSlash == std::string::npos) { // Formato: "v/t"
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            tIdx = std::stoi(vertStr.substr(firstSlash + 1)) - 1;
                        }
                        else if (secondSlash == firstSlash + 1) { // Formato: "v//n"
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            nIdx = std::stoi(vertStr.substr(secondSlash + 1)) - 1;
                        }
                        else { // Formato: "v/t/n"
                            vIdx = std::stoi(vertStr.substr(0, firstSlash)) - 1;
                            tIdx = std::stoi(vertStr.substr(firstSlash + 1, secondSlash - firstSlash - 1)) - 1;
                            nIdx = std::stoi(vertStr.substr(secondSlash + 1)) - 1;
                        }

                        // Com os índices extraídos, busca os dados nos vetores "raw".
                        Vec3 pV{ 0.0f, 0.0f, 0.0f };
                        Vec2 tV{ 0.0f, 0.0f };
                        Vec3 nV{ 0.0f, 0.0f, 0.0f };

                        if (vIdx >= 0) pV = Vec3{ raw_positions[vIdx].x, raw_positions[vIdx].y, raw_positions[vIdx].z };
                        if (tIdx >= 0) tV = Vec2{ raw_texcoords[tIdx].x, raw_texcoords[tIdx].y };
                        if (nIdx >= 0) nV = Vec3{ raw_normals[nIdx].x, raw_normals[nIdx].y, raw_normals[nIdx].z };
                        
                        // Adiciona o vértice completo (posição, UV, normal) aos vetores finais alinhados.
                        positions.push_back(pV);
                        texcoords.push_back(tV);
                        normals.push_back(nV);

                        // Adiciona os dados também à struct Face.
                        face.addVert(pV);
                        face.addText(tV);
                        face.addNorm(nV);
                    }

                    // Adiciona a face recém-criada ao grupo atual.
                    currentGroup->addFace(face);
                }
            }
        }

        file.close();

        // 3 Com os vetores alinhados e os grupos preenchidos, constrói o objeto Mesh.
        //    Isso também irá gerar o VAO/VBO.
        mesh = Mesh(positions, texcoords, normals, groups);

        // 4 Carrega o arquivo de material e a textura associada.
        material = setupMtl(mtlFilePath);
        if (!material.textureName.empty()) {
            textureID = setupTexture(material.textureName);
        }
    }

    // Métodos para obter acesso à malha.
    Mesh& getMesh() { return mesh; }
    const Mesh& getMesh() const { return mesh; }
};

// ----------------------------------------------------------------------------
// CLASSES PARA ESCRITA DE ARQUIVOS .OBJ
// ----------------------------------------------------------------------------

/**
 * @class OBJWriter
 * @brief Escreve o conteúdo de um objeto Mesh para um arquivo no formato .obj.
 */
class OBJWriter {
public:
    /**
     * @brief Realiza a escrita da malha para um arquivo.
     * @param mesh A malha a ser salva.
     * @param filename O nome do arquivo .obj a ser criado.
     */
    void write(const Mesh& mesh, const std::string& filename) {
        std::ofstream f(filename);
        if (!f.is_open()) {
            assert(false && "Não conseguiu criar .obj");
            return;
        }

        // 1) Escreve todas as posições de vértice (v x y z)
        for (const auto& v : mesh.vertices) {
            f << "v " << v.x << " " << v.y << " " << v.z << "\n";
        }
        // 2) Escreve todas as coordenadas de textura (vt u v)
        for (const auto& uv : mesh.mappings) {
            f << "vt " << uv.u << " " << uv.v << "\n";
        }
        // 3) Escreve todas as normais (vn nx ny nz)
        for (const auto& n : mesh.normals) {
            f << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        }

        // 4) Itera sobre cada grupo da malha
        for (const auto& grp : mesh.groups) {
            // Escreve a diretiva "usemtl" se houver um material associado
            if (!grp.mtlName.empty()) {
                f << "usemtl " << grp.mtlName << "\n";
            }
            // Itera sobre cada face do grupo
            for (const auto& face : grp.faces) {
                f << "f ";
                for (size_t k = 0; k < face.verts.size(); ++k) {
                    // Para escrever uma face no formato "f v/t/n", precisamos dos ÍNDICES
                    // da posição, UV e normal, não dos valores em si.
                    // A lógica abaixo usa std::find para encontrar o índice de cada componente
                    // nos vetores globais da malha.
                    // NOTA: std::find pode ser lento para malhas muito grandes. Uma otimização
                    // seria usar um std::unordered_map para mapear valores para índices.
                    int vi = static_cast<int>(std::find(mesh.vertices.begin(), mesh.vertices.end(), face.verts[k]) - mesh.vertices.begin()) + 1;
                    int ti = static_cast<int>(std::find(mesh.mappings.begin(), mesh.mappings.end(), face.texts[k]) - mesh.mappings.begin()) + 1;
                    int ni = static_cast<int>(std::find(mesh.normals.begin(), mesh.normals.end(), face.norms[k]) - mesh.normals.begin()) + 1;

                    // Escreve os índices no formato 1-based (padrão .obj).
                    f << vi << "/" << ti << "/" << ni << " ";
                }
                f << "\n";
            }
        }

        f.close();
    }
};

/**
 * @class Object3DWriter
 * @brief Uma classe de conveniência para salvar um Object3D.
 */
class Object3DWriter {
public:
    void write(Object3D& obj) {
        // Gera um nome de arquivo a partir do nome do objeto.
        std::string outObj = obj.name + ".obj";

        // Extrai a malha do Object3D e a passa para o OBJWriter.
        OBJWriter w;
        w.write(obj.getMesh(), outObj);
    }
};


#endif // GEOMETRYOBJECTS_HPP