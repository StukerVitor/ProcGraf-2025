// ARQUIVO PRINCIPAL: Origem.cpp
// Contém a função main, o loop de renderização e toda a lógica da aplicação,
// que se divide em um modo editor 2D e um modo visualizador 3D.

// --- INCLUDES ---
#include "GeometryObjects.hpp" // Estruturas para objetos 3D, parsing de .obj e setup de geometria.
#include "Shader.h"           // Classe que abstrai a compilação e linkagem de shaders.

// Bibliotecas padrão do C++
#include <iostream>
#include <string>
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

// Bibliotecas de Gráficos
#include <glad/glad.h>   // Carregador de funções do OpenGL. Deve ser incluído antes de GLFW.
#include <GLFW/glfw3.h>  // Biblioteca para criação de janelas, contextos OpenGL e gerenciamento de input.


// ============================================================================
// ESTRUTURAS DE DADOS AUXILIARES (para renderização e lógica do aplicativo)
// ============================================================================

/**
 * @struct GlobalConfig
 * @brief Centraliza todas as configurações globais da cena que podem ser carregadas
 * de um arquivo ou definidas como padrão. Facilita o gerenciamento do estado da renderização.
 */
// Configuração global do aplicativo (luz, câmera, parâmetros de fog, etc.)
struct GlobalConfig {
    // Configurações de iluminação
    glm::vec3 lightPos, lightColor;   // Posição e cor da luz
    // Configurações da câmera
    glm::vec3 cameraPos, cameraFront; // Posição e direção da câmera
    // Configurações de projeção (Perspective)
    GLfloat   fov, nearPlane, farPlane; // Projeção perspectiva
    // Configurações de controle da câmera
    GLfloat   sensitivity, cameraSpeed; // Sensibilidade e velocidade da câmera

    // Requisito 1: Parâmetros de atenuação e fog
    float   attConstant, attLinear, attQuadratic; // Fatores para a equação de atenuação.
    glm::vec3 fogColor;                            // Cor do nevoeiro
    float   fogStart, fogEnd;                      // Distâncias de início e fim do nevoeiro
};

/**
 * @struct BSplineCurve
 * @brief Armazena os dados de uma curva B-Spline, incluindo seus pontos de controle,
 * os pontos da curva gerada e os VAOs para renderização.
 */
// Estrutura para curvas B-Spline
struct BSplineCurve {
    std::string name;
    std::vector<glm::vec3> controlPoints; // Pontos clicados pelo usuário ou lidos do arquivo.
    std::vector<glm::vec3> curvePoints;   // Pontos da curva, gerados a partir dos pontos de controle.
    GLuint pointsPerSegment;              // Densidade da curva.
    glm::vec4 color;                      // Cor para renderizar a curva.
    GLuint VAO;                           // VAO para desenhar a linha da curva.
    GLuint controlPointsVAO;              // VAO para desenhar os pontos de controle.
};

// ============================================================================
// PROTÓTIPOS DE FUNÇÕES
// ============================================================================

// Callbacks para input do GLFW
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// Funções de lógica da aplicação
void readSceneFile(const std::string&,
    std::unordered_map<std::string, Object3D>*,
    std::vector<std::string>*,
    std::unordered_map<std::string, BSplineCurve>*,
    GlobalConfig*);
std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3>& controlPoints, int pointsPerSegment);
GLuint generateControlPointsBuffer(std::vector<glm::vec3> controlPoints);
BSplineCurve createBSplineCurve(std::vector<glm::vec3> controlPoints, int pointsPerSegment);
void generateTrackMesh(const std::vector<glm::vec3> centerPoints, float trackWidth,
    std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
void exportAnimationPoints(const std::vector<glm::vec3>& points, const std::string& filename);
void generateSceneFile(const std::string& trackObj, const std::string& carObj,
    const std::string& animFile, const std::string& sceneFile,
    const std::vector<glm::vec3>& controlPoints);

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================
// O uso de variáveis globais neste projeto simplifica o compartilhamento de estado
// entre as funções de callback do GLFW e o loop de renderização principal.

// --- Configurações da Aplicação e Janela ---
GlobalConfig globalConfig; // Instância única das configurações globais da cena.
const GLuint WIDTH = 1000, HEIGHT = 1000; // Dimensões da janela.

// --- Controle da Câmera ---
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f); // Vetor "para cima" do mundo, usado no cálculo da matriz View.
bool      firstMouse = true; // Flag para o primeiro movimento do mouse, evita um "salto" inicial da câmera.
float     lastX, lastY, pitch = 0.0f, yaw = -90.0f; // Variáveis para a câmera mouselook. Yaw=-90 para apontar para -Z.
bool      moveW = false, moveA = false, moveS = false, moveD = false; // Flags de movimento, ativadas/desativadas pelos callbacks.

// --- Estado do Editor e Animação ---
bool      editorMode = true;                     // Requisito 2a: A aplicação começa em modo editor.
std::vector<glm::vec3> editorControlPoints;      // Requisito 2a: Armazena os pontos de controle clicados pelo usuário no editor.
int       animationIndex = 0;                    // Índice atual na lista de pontos de animação do carro.
float     trackWidth = 1.0f;                     // Largura da pista a ser gerada proceduralmente.
GLuint    showCurves = 1;                        // Flag para exibir ou não as curvas de debug no modo visualizador.

// --- Controle de Altura da Pista no Editor ---
// per-point “yellow level” (0→no yellow, 1→full yellow) & step size
std::vector<float>     editorPointYellowLevels;  // Armazena a altura (representada pelo "nível de amarelo") de cada ponto de controle.
float                  currentYellowLevel = 0.5f;  // Altura inicial/padrão para novos pontos.
const float            yellowStep = 0.3f;        // Incremento/decremento de altura ao pressionar +/-.
const float            maxHeight = 5.0f;    // track max height // Altura máxima permitida para a pista.

// ---------------------------------------------------------------------------
//  GLOBAIS para o VAO/VBO usados pelos pontos do editor
// ---------------------------------------------------------------------------
// Usar um VAO/VBO global para os pontos de controle é eficiente. Ele é criado uma vez
// e seus dados são atualizados dinamicamente (com GL_DYNAMIC_DRAW), evitando
// a sobrecarga de recriar os buffers a cada clique do mouse.
static GLuint gCtrlPtsVAO = 0;
static GLuint gCtrlPtsVBO = 0;


// --- Contêineres de Dados da Cena ---
// Usar std::unordered_map permite acesso rápido a objetos e curvas por nome.
std::unordered_map<std::string, Object3D> meshes;                // Armazena Object3D
std::vector<std::string>                  meshList;              
std::unordered_map<std::string, BSplineCurve> bSplineCurves;      

// --- Controle de Tempo da Animação ---
double lastFrameTime = 0.0;
float  animAccumulator = 0.0f;              // Acumula o tempo delta para desacoplar a animação do framerate.
const float STEP_TIME = 1.0f / 30.0f;       // Garante que a animação rode a 30 "passos" por segundo, independente da taxa de quadros.

// ============================================================================
// SHADERS (modelo de iluminação completo: ambiente + difusa + especular + atenuação + fog)
// ============================================================================

/**
 * @brief Vertex Shader para os objetos 3D.
 * @details Responsável por transformar as posições dos vértices do espaço do modelo para o
 * espaço de clipe (o resultado final é `gl_Position`). Também prepara e passa dados
 * (como a posição no mundo e a normal) para o Fragment Shader.
 */
const char* vertexShaderSource = R"glsl(
#version 450 core
// Atributos de vértice recebidos do VAO, definidos por `glVertexAttribPointer`.
layout (location = 0) in vec3 aPos;      // Posição do vértice em espaço de modelo.
layout (location = 1) in vec2 aTexCoord; // Coordenada de textura (UV).
layout (location = 3) in vec3 aNormal;   // Vetor normal do vértice.

// Saídas (out), que serão interpoladas e se tornarão entradas (in) no Fragment Shader.
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

// Uniforms (variáveis globais no shader, definidas por `glUniform...` no C++).
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Transformação padrão: Vértice(local) -> Mundo -> Câmera -> Projeção -> Clipe.
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    // Calcula a posição do fragmento no espaço do mundo para cálculos de iluminação.
    FragPos     = vec3(model * vec4(aPos, 1.0));
    // Transforma o vetor normal para o espaço do mundo. A multiplicação pela
    // transposta da inversa da matriz de modelo corrige deformações na normal
    // causadas por escalas não uniformes no modelo.
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    // Passa a coordenada de textura diretamente para o Fragment Shader.
    TexCoord    = aTexCoord;
}
)glsl";

/**
 * @brief Fragment Shader para os objetos 3D.
 * @details Calcula a cor final de cada fragmento (pixel). Implementa um modelo de
 * iluminação Phong completo, com componentes ambiente, difuso e especular,
 * além de atenuação da luz com a distância e um efeito de nevoeiro (fog).
 */
const char* fragmentShaderSource = R"glsl(
#version 450 core
out vec4 FragColor; // A cor final que será escrita no framebuffer.

// Entradas (in) recebidas (e interpoladas) do Vertex Shader.
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

// --- Uniforms de Iluminação e Material ---
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;
// Coeficientes do material (Ka, Kd, Ks, Ns) que definem como a superfície reage à luz.
uniform float kaR, kaG, kaB; // Ambiente
uniform float kdR, kdG, kdB; // Difusa
uniform float ksR, ksG, ksB; // Especular
uniform float ns;            // Expoente especular (shininess)

// --- Uniforms de Fog e Atenuação ---
uniform vec3 fogColor;
uniform float fogStart, fogEnd;
uniform float attConstant, attLinear, attQuadratic;

// --- Uniform de Textura ---
uniform sampler2D tex; // A textura do objeto (unidade de textura 0).

void main() {
    // --- CÁLCULO DE ILUMINAÇÃO (MODELO DE PHONG) ---

    // 1. Componente Ambiente: Cor base do objeto, iluminado pela luz ambiente global.
    vec3 ambient = vec3(kaR, kaG, kaB) * lightColor;

    // 2. Componente Difusa: Representa a luz que é refletida igualmente em todas as direções.
    //    Depende do ângulo entre a normal da superfície e a direção da luz.
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0); // `max` garante que não haja luz "negativa" em superfícies não viradas para a luz.
    vec3 diffuse  = vec3(kdR, kdG, kdB) * diff * lightColor;

    // 3. Componente Especular: Simula o brilho/reflexo, depende da posição da câmera.
    vec3 viewDir    = normalize(cameraPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm); // Calcula o vetor de reflexão da luz.
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), ns); // `ns` controla o tamanho e a intensidade do brilho.
    vec3 specular   = vec3(ksR, ksG, ksB) * spec * lightColor;

    // --- CÁLCULO DE ATENUAÇÃO ---
    // Simula a perda de intensidade da luz com a distância, usando uma equação quadrática.
    float distance    = length(lightPos - FragPos);
    float attenuation = 1.0 / (attConstant + attLinear * distance + attQuadratic * distance * distance);

    // Combina os três componentes de iluminação e aplica a atenuação.
    vec3 lighting = (ambient + diffuse + specular) * attenuation;

    // Obtém a cor da textura na coordenada correspondente (amostragem).
    vec4 texColor = texture(tex, TexCoord);

    // --- CÁLCULO DE FOG (NEVOEIRO) ---
    // Interpola a cor do objeto iluminado com a cor do fog com base na distância da câmera.
    float distToCamera = length(cameraPos - FragPos);
    // `clamp` garante que o fator de mistura do fog fique entre 0.0 e 1.0.
    float fogFactor    = clamp((distToCamera - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    // `mix()` faz a interpolação linear: mix(cor_final, cor_inicial, fator).
    vec3 finalColor    = mix(lighting * texColor.rgb, fogColor, fogFactor);

    // Define a cor final do fragmento, combinando a cor calculada com a transparência da textura.
    FragColor = vec4(finalColor, texColor.a);
}
)glsl";

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================

int main() {
    // --- INICIALIZAÇÃO DO AMBIENTE GRÁFICO ---
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Modelador de Pistas e Visualizador 3D", nullptr, nullptr);
    assert(window && "Falha ao criar janela GLFW");
    glfwMakeContextCurrent(window);

    // Define as funções de callback que o GLFW chamará para tratar inputs.
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    // Requisito 2a: Cursor visível e normal no modo editor.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Inicializa o GLAD para carregar as funções do OpenGL. Essencial para usar OpenGL moderno.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Falha ao inicializar GLAD\n";
        return -1;
    }
    // Define a área de renderização para cobrir toda a janela.
    glViewport(0, 0, WIDTH, HEIGHT);
    // Habilita o teste de profundidade, para que objetos mais próximos cubram os mais distantes.
    glEnable(GL_DEPTH_TEST);

    // --- COMPILAÇÃO DOS SHADERS ---
    // Instancia os shaders usando a classe wrapper.
    Shader objectShader(vertexShaderSource, fragmentShaderSource, true); // Shader para os objetos 3D.
    Shader lineShader("../shaders/Line.vs", "../shaders/Line.fs");       // Shader simples para linhas e pontos.

    // --- CONFIGURAÇÃO INICIAL DA CENA ---
    // Define valores padrão para câmera, luz e outros parâmetros.
    // Estes valores podem ser sobrescritos ao carregar um arquivo de cena.
    globalConfig.cameraPos = glm::vec3(0.0f, 5.0f, 10.0f);
    globalConfig.cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    globalConfig.lightPos = glm::vec3(10.0f, 10.0f, 10.0f);
    globalConfig.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    globalConfig.fov = 45.0f;
    globalConfig.nearPlane = 0.1f;
    globalConfig.farPlane = 100.0f;
    globalConfig.sensitivity = 0.1f;
    globalConfig.cameraSpeed = 0.05f;
    globalConfig.attConstant = 1.0f;
    globalConfig.attLinear = 0.09f;
    globalConfig.attQuadratic = 0.032f;
    globalConfig.fogColor = glm::vec3(0.5f, 0.5f, 0.5f);
    globalConfig.fogStart = 5.0f;
    globalConfig.fogEnd = 50.0f;

    lastFrameTime = glfwGetTime();
    animAccumulator = 0.0f;

    // --- LOOP DE RENDERIZAÇÃO ---
    // O coração da aplicação. Roda continuamente até que a janela seja fechada.
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // Processa eventos de input (teclado, mouse).

        // Limpa os buffers de cor e profundidade a cada novo frame.
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPointSize(10); // Define o tamanho dos pontos a serem renderizados (para os pontos de controle).

        // --- CÁLCULO DE TEMPO ---
        double now = glfwGetTime();
        float  deltaTime = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;
        animAccumulator += deltaTime; // Acumula o tempo para a animação.

        // --- ATUALIZAÇÃO DAS MATRIZES DE CÂMERA ---
        // A matriz 'view' transforma as coordenadas do mundo para o espaço da câmera.
        glm::mat4 view = glm::lookAt(
            globalConfig.cameraPos,                             // eye: posição da câmera no mundo
            globalConfig.cameraPos + globalConfig.cameraFront,  // center: ponto para onde a câmera está olhando
            cameraUp                                            // up: vetor que define a direção “para cima”
        );

        // A matriz 'projection' define o frustum de visão (perspectiva).
        // View frustum (ou simplesmente frustum de visão) é o volume em forma de pirâmide truncada que define tudo o que a câmera “vê” na cena 3D. Ele é limitado por:
        // Near plane (plano próximo): o plano mais próximo da câmera onde a renderização começa (distância mínima).
        // Far plane (plano distante): o plano mais afastado da câmera onde a renderização termina (distância máxima).
        glm::mat4 projection = glm::perspective(
            glm::radians(globalConfig.fov),           // fov: campo de visão vertical da câmera (em graus convertido para radianos)
            static_cast<float>(WIDTH) / HEIGHT,       // aspect ratio: proporção entre largura e altura da viewport
            globalConfig.nearPlane,                   // nearPlane: distância mínima do plano de corte próximo do frustum
            globalConfig.farPlane                     // farPlane: distância máxima do plano de corte distante do frustum
        );

        // --- LÓGICA DE RENDERIZAÇÃO CONDICIONAL (EDITOR vs. VISUALIZADOR) ---
        if (editorMode) {
            // --- MODO EDITOR ---
            // Renderiza apenas os pontos de controle da pista.
            glUseProgram(lineShader.getId());
            // Envia as matrizes de câmera para o shader de linhas.
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            if (!editorControlPoints.empty()) {
                // Gera/atualiza o buffer com os pontos de controle e o desenha.
                GLuint VAO = generateControlPointsBuffer(editorControlPoints);
                glBindVertexArray(VAO);

                // Desenha cada ponto de controle com uma cor baseada na sua altura ("nível de amarelo").
                for (size_t i = 0; i < editorControlPoints.size(); ++i) {
                    float yl = editorPointYellowLevels[i];
                    float t = glm::clamp(yl / maxHeight, 0.0f, 1.0f); // Normaliza a altura para o intervalo [0,1].
                    float brightness = 0.2f + 0.8f * t; // Mapeia a altura para um brilho entre 0.2 e 1.0.
                    glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), brightness, brightness, 0.0f, 1.0f);
                    glDrawArrays(GL_POINTS, (GLint)i, 1); // Desenha um único ponto.
                }

                glBindVertexArray(0);
            }
        }
        else {
            // --- MODO VISUALIZADOR ---
            // Renderiza a cena 3D completa.

            // Movimentação da câmera baseada nas flags de input.
            if (moveW)
                globalConfig.cameraPos += globalConfig.cameraFront * globalConfig.cameraSpeed;
            if (moveA)
                globalConfig.cameraPos -= glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;
            if (moveS)
                globalConfig.cameraPos -= globalConfig.cameraFront * globalConfig.cameraSpeed;
            if (moveD)
                globalConfig.cameraPos += glm::normalize(glm::cross(globalConfig.cameraFront, cameraUp)) * globalConfig.cameraSpeed;

            // Requisito 3: Visualizador 3D
            // Ativa o shader principal e envia todas as uniforms globais (luz, fog, câmera, etc.).
            glUseProgram(objectShader.getId());
            glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightPos"), 1, glm::value_ptr(globalConfig.lightPos));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "lightColor"), 1, glm::value_ptr(globalConfig.lightColor));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "cameraPos"), 1, glm::value_ptr(globalConfig.cameraPos));
            glUniform3fv(glGetUniformLocation(objectShader.getId(), "fogColor"), 1, glm::value_ptr(globalConfig.fogColor));
            glUniform1f(glGetUniformLocation(objectShader.getId(), "fogStart"), globalConfig.fogStart);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "fogEnd"), globalConfig.fogEnd);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attConstant"), globalConfig.attConstant);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attLinear"), globalConfig.attLinear);
            glUniform1f(glGetUniformLocation(objectShader.getId(), "attQuadratic"), globalConfig.attQuadratic);

            // Desenha todas as Object3D carregadas na cena.
            float lastCarYaw = 0.0f;
            for (auto& pair : meshes) {
                Object3D& obj = pair.second;
                glm::mat4  model(1.0f);

                // Requisito 3d: Animação do carro
                // Lógica especial para o objeto chamado "Carro".
                if (obj.name == "Carro" && obj.animationPositions.size() >= 3) {
                    int N = static_cast<int>(obj.animationPositions.size());
                    int idx = animationIndex % N;
                    int prevIdx = (idx - 1 + N) % N; // Índice anterior com wrap-around (evita valores negativos).
                    int nextIdx = (idx + 1) % N;     // Próximo índice com wrap-around.

                    glm::vec3 P = obj.animationPositions[prevIdx];  // Ponto anterior
                    glm::vec3 C = obj.animationPositions[idx];      // Ponto atual (posição do carro)
                    glm::vec3 Np = obj.animationPositions[nextIdx]; // Próximo ponto

                    // A direção do carro é a tangente à curva, aproximada pelo vetor entre o ponto seguinte e o anterior.
                    glm::vec3 dir = glm::normalize(Np - P);

                    // --- CONSTRUÇÃO DE UMA BASE ORTONORMAL ---
                    // Para orientar o carro corretamente, precisamos de 3 eixos: frente, direita e cima.
                    glm::vec3 forward = dir;
                    // O vetor 'direita' é perpendicular ao 'frente' e ao 'cima' do mundo (Y-up).
                    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
                    // O vetor 'cima' do carro é perpendicular ao 'frente' e ao 'direita'. Isso permite o "banking" (inclinação) do carro nas curvas e subidas.
                    glm::vec3 upAxis = glm::cross(right, forward);

                    // A matriz de rotação pode ser construída diretamente com os vetores da base ortonormal como suas colunas.
                    glm::mat4 rot(1.0f);
                    rot[0] = glm::vec4(right, 0.0f);
                    rot[1] = glm::vec4(upAxis, 0.0f);
                    rot[2] = glm::vec4(forward, 0.0f);

                    // A matriz de modelo final é a composição de Escala -> Rotação -> Translação.
                    // A ordem é importante: primeiro escalamos o objeto em sua origem, depois rotacionamos, e por fim transladamos para a posição final.
                    model = glm::translate(glm::mat4(1.0f), C)
                        * rot
                        * glm::scale(glm::mat4(1.0f), obj.scale);
                }
                else {
                    // Para objetos estáticos (como a pista), aplica apenas a translação definida.
                    model = glm::translate(glm::mat4(1.0f), obj.position);
                }

                // Aplica rotações e escala adicionais (se houver, para objetos estáticos).
                model = glm::rotate(model, glm::radians(obj.angle.x), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(obj.angle.y), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(obj.angle.z), glm::vec3(0.0f, 0.0f, 1.0f));
                // Aplica escala
                model = glm::scale(model, obj.scale);

                // Envia a matriz de modelo e as propriedades do material do objeto para o shader.
                glUniformMatrix4fv(glGetUniformLocation(objectShader.getId(), "model"), 1, GL_FALSE, glm::value_ptr(model));
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaR"), obj.material.kaR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaG"), obj.material.kaG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kaB"), obj.material.kaB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdR"), obj.material.kdR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdG"), obj.material.kdG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "kdB"), obj.material.kdB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksR"), obj.material.ksR);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksG"), obj.material.ksG);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ksB"), obj.material.ksB);
                glUniform1f(glGetUniformLocation(objectShader.getId(), "ns"), obj.material.ns);

                // --- RENDERIZAÇÃO DO OBJETO ---
                // O VAO contém todas as informações de buffer (VBO) e layout de atributos.
                GLuint vao = obj.getMesh().VAO;
                size_t vertCount = obj.getMesh().vertices.size();
                glBindVertexArray(vao); // Ativa o VAO do objeto.
                glActiveTexture(GL_TEXTURE0);         // Ativa a unidade de textura 0.
                glBindTexture(GL_TEXTURE_2D, obj.textureID); // Vincula a textura do objeto a essa unidade.
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertCount)); // Desenha!
                glBindVertexArray(0); // Desvincula o VAO para evitar modificações acidentais.
            }

            // Desenha curvas B-Spline (para debug).
            if (showCurves) {
                glUseProgram(lineShader.getId());
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "view"), 1, GL_FALSE, glm::value_ptr(view));
                glUniformMatrix4fv(glGetUniformLocation(lineShader.getId(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));
                for (const auto& pair : bSplineCurves) {
                    const BSplineCurve& bc = pair.second;
                    // Desenha a linha da curva
                    glUniform4fv(glGetUniformLocation(lineShader.getId(), "finalColor"), 1, glm::value_ptr(bc.color));
                    glBindVertexArray(bc.VAO);
                    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bc.curvePoints.size()));
                    glBindVertexArray(0);

                    // Desenha os pontos de controle em amarelo
                    glUniform4f(glGetUniformLocation(lineShader.getId(), "finalColor"), 1.0f, 1.0f, 0.0f, 1.0f);
                    glBindVertexArray(bc.controlPointsVAO);
                    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bc.controlPoints.size()));
                    glBindVertexArray(0);
                }
            }

            // Atualiza a animação do carro usando o acumulador de tempo.
            if (!meshes["Carro"].animationPositions.empty()) {
                while (animAccumulator >= STEP_TIME) {
                    animationIndex = (animationIndex + 1) % meshes["Carro"].animationPositions.size();
                    animAccumulator -= STEP_TIME;
                }
            }
        }

        glfwSwapBuffers(window); // Troca o buffer de fundo (onde desenhamos) com o buffer da frente (o que é exibido).
    }

    // --- LIBERAÇÃO DE RECURSOS ---
    for (const auto& pair : meshes) {
        glDeleteVertexArrays(1, &pair.second.getMesh().VAO);
        // Os VBOs são geralmente gerenciados pelo VAO, mas poderiam ser deletados aqui também se gerenciados separadamente.
    }
    for (const auto& pair : bSplineCurves) {
        glDeleteVertexArrays(1, &pair.second.VAO);
        glDeleteVertexArrays(1, &pair.second.controlPointsVAO);
    }

    // --- libera o VAO/VBO dos pontos do editor ----------------------------
    if (gCtrlPtsVBO) glDeleteBuffers(1, &gCtrlPtsVBO);
    if (gCtrlPtsVAO) glDeleteVertexArrays(1, &gCtrlPtsVAO);

    glfwTerminate(); // Finaliza o GLFW, liberando todos os seus recursos.
    return 0;
}



// ============================================================================
// CALLBACKS DE INPUT E LÓGICA DE MOUSE
// ============================================================================
/**
 * @brief Callback para eventos de teclado. Gerencia todos os atalhos.
 */
// Função key_callback ajustada
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    // --- Controles do Modo Editor ---
    // — adjust yellow/height in editor:
    if (editorMode && action == GLFW_PRESS) {
        // Teclas +/- para aumentar/diminuir a altura do último ponto de controle adicionado.
        if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
            currentYellowLevel =
                glm::clamp(currentYellowLevel + yellowStep, 0.0f, maxHeight);
            if (!editorPointYellowLevels.empty())
                editorPointYellowLevels.back() = currentYellowLevel;
        }
        else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) {
            currentYellowLevel =
                glm::clamp(currentYellowLevel - yellowStep, 0.0f, maxHeight);
            if (!editorPointYellowLevels.empty())
                editorPointYellowLevels.back() = currentYellowLevel;
        }
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    // --- Controles de Movimento (Modo Visualizador) ---
    // Usar `action != GLFW_RELEASE` permite segurar a tecla para movimento contínuo.
    if (key == GLFW_KEY_W && action == GLFW_PRESS)
        moveW = true;
    else if (key == GLFW_KEY_W && action == GLFW_RELEASE)
        moveW = false;
    if (key == GLFW_KEY_A && action == GLFW_PRESS)
        moveA = true;
    else if (key == GLFW_KEY_A && action == GLFW_RELEASE)
        moveA = false;
    if (key == GLFW_KEY_S && action == GLFW_PRESS)
        moveS = true;
    else if (key == GLFW_KEY_S && action == GLFW_RELEASE)
        moveS = false;
    if (key == GLFW_KEY_D && action == GLFW_PRESS)
        moveD = true;
    else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
        moveD = false;
    
    // Requisito 2: Alternar entre editor e visualizador
    // --- MUDANÇA DE MODO (EDITOR -> VISUALIZADOR) ---
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        if (editorMode && !editorControlPoints.empty()) {
            // --- PIPELINE DE GERAÇÃO DE CENA ---
            // Esta é a sequência de eventos que transforma os cliques do usuário em uma cena 3D.
            
            // 1. Converte os pontos de controle 2D + altura (armazenada em YL) em pontos 3D.
            // build 3D ctrl‐points with height in .z:
            std::vector<glm::vec3> ctrlPoints3D;
            ctrlPoints3D.reserve(editorControlPoints.size());
            for (size_t i = 0; i < editorControlPoints.size(); ++i) {
                const auto& p = editorControlPoints[i];
                float yl = editorPointYellowLevels[i];
                ctrlPoints3D.emplace_back(p.x, p.y, yl);
            }
            
            // 2. Gera a curva B-Spline suave e a malha da pista a partir dela.
            // spline & mesh:
            auto curvePoints = generateBSplinePoints(ctrlPoints3D, 50);
            std::vector<Vertex>      vertices;
            std::vector<unsigned int> indices;
            generateTrackMesh(curvePoints, trackWidth, vertices, indices);

            // 3. CRÍTICO: Transforma a pista do plano XY (editor) para o plano XZ (visualizador).
            // Cria um novo vetor de Vertex “trackVerts” aplicando Y↔Z
            std::vector<Vertex> trackVerts;
            trackVerts.reserve(vertices.size());
            for (const auto& v : vertices) {
                // Posição: (x, y, z) -> (v.x, v.z, v.y)
                // Normal: (nx, ny, nz) -> (v.nx, v.nz, v.ny)
                trackVerts.push_back({
                    v.x, v.z, v.y,    // swapped y<->z
                    v.s, v.t,
                    v.nx, v.nz, v.ny  // swapped normal y<->z
                    });
            }
            
            // 4. Constrói o objeto Mesh da pista e o salva em "track.obj".
            // Constrói diretamente o Mesh de “track”:
            Mesh trackMesh(trackVerts, indices, /*groupName=*/"track", /*mtlName=*/"");

            // Grava “track.obj” com OBJWriter:
            OBJWriter objWriter;
            objWriter.write(trackMesh, "track.obj");

            // 5. Exporta os pontos da curva para a animação e gera o arquivo de cena completo.
            exportAnimationPoints(curvePoints, "animation.txt");
            generateSceneFile("track.obj", "car.obj", "animation.txt", "Scene.txt", editorControlPoints);
            
            // 6. Lê o arquivo de cena recém-criado para popular o modo visualizador.
            readSceneFile("Scene.txt", &meshes, &meshList, &bSplineCurves, &globalConfig); // Agora acessa as globais
            
            // 7. Alterna para o modo visualizador e captura o cursor do mouse para a câmera mouselook.
            editorMode = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}

/**
 * @brief Callback para eventos de clique do mouse.
 * @details No modo editor, captura cliques do botão esquerdo para adicionar novos pontos de controle.
 * Realiza o processo de "unprojection" para converter as coordenadas 2D da tela
 * em coordenadas 3D do mundo.
 */
// Requisito 2a: Captura de cliques do mouse para pontos de controle no editor 2D
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (editorMode && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // --- CONVERSÃO DE COORDENADAS DE TELA PARA MUNDO (UNPROJECTION) ---
        // Este é o processo inverso ao pipeline de transformação do vértice.
        // Obtem a matriz de projeção e view atuais
        glm::mat4 view = glm::lookAt(
            globalConfig.cameraPos,                            // eye: posição da câmera no mundo
            globalConfig.cameraPos + globalConfig.cameraFront, // center: ponto para onde a câmera está olhando
            cameraUp);                                         // up: vetor que define a direção “para cima”
        glm::mat4 projection = glm::perspective(glm::radians(globalConfig.fov), // fov: campo de visão vertical da câmera (em graus convertido para radianos)
            static_cast<float>(WIDTH) / HEIGHT,                                 // aspect ratio: proporção entre largura e altura da viewport
            globalConfig.nearPlane,                                             // nearPlane: distância mínima do plano de corte próximo do frustum
            globalConfig.farPlane);                                             // farPlane: distância máxima do plano de corte distante do frustum

        // 1. Tela (pixels) -> NDC (Normalized Device Coordinates: [-1, 1] para X e Y)
        float x_ndc = (2.0f * static_cast<float>(xpos)) / WIDTH - 1.0f;
        float y_ndc = 1.0f - (2.0f * static_cast<float>(ypos)) / HEIGHT; // Y é invertido (origem no topo da janela).

        // 2. NDC -> Espaço de Clipe (Homogêneo)
        // Z=-1 para apontar para o início do frustum de visão. W=1 para ser um ponto.
        glm::vec4 clipCoords = glm::vec4(x_ndc, y_ndc, -1.0f, 1.0f);

        // 3. Clipe -> Espaço da Câmera (Eye Space)
        // Aplicamos a inversa da matriz de projeção.
        glm::vec4 eyeCoords = glm::inverse(projection) * clipCoords;
        eyeCoords.z = -1.0f; // Ignoramos o Z e W calculados, pois queremos um VETOR de direção.
        eyeCoords.w = 0.0f;

        // 4. Câmera -> Espaço do Mundo
        // Aplicamos a inversa da matriz de visão para obter a direção do raio no mundo.
        glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(view) * eyeCoords));
        glm::vec3 rayOrigin = globalConfig.cameraPos;

        // 5. Interseção do raio com o plano XY (z = 0), onde a pista é desenhada no editor.
        //    A equação do plano é P.N + d = 0. Para o plano XY, N=(0,0,1), d=0.
        //    A equação do raio é R(t) = Origem + t*Direção.
        //    Substituindo R(t) em P, temos: (Origem_z + t*Direção_z) = 0 => t = -Origem_z / Direção_z
        float t = -rayOrigin.z / rayDir.z;
        glm::vec3 intersectPoint = rayOrigin + t * rayDir;

        // Adiciona o ponto de controle calculado à lista.
        editorControlPoints.emplace_back(intersectPoint.x,
            intersectPoint.y,
            0.0f);
        // same “yellow” as last point
        // Associa a altura atual ao novo ponto.
        editorPointYellowLevels.push_back(currentYellowLevel);
    }
}

/**
 * @brief Callback para movimento do mouse.
 * @details No modo visualizador, implementa a câmera "mouselook", calculando os ângulos
 * de yaw e pitch para determinar a nova direção da câmera.
 */
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!editorMode)
    {
        // Flag para o primeiro movimento do mouse, evita um "salto" inicial da câmera.
        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        // Calcula o deslocamento do mouse desde o último frame.
        float offsetX = static_cast<float>(xpos - lastX) * globalConfig.sensitivity;
        float offsetY = static_cast<float>(lastY - ypos) * globalConfig.sensitivity; // Invertido, pois Y aumenta de cima para baixo.
        lastX = xpos;
        lastY = ypos;

        // Atualiza os ângulos de yaw e pitch.
        pitch += offsetY;
        yaw += offsetX;

        // Limita o pitch para evitar que a câmera vire de cabeça para baixo.
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        // Calcula o novo vetor "frente" da câmera a partir dos ângulos de yaw e pitch usando trigonometria.
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        globalConfig.cameraFront = glm::normalize(front);
    }
}
// ============================================================================
// ============================================================================
// ============================================================================



// ============================================================================
// BSpline
// ============================================================================
/**
 * @brief Cria a estrutura BSplineCurve com seu VAO/VBO correspondente para renderização.
 */
BSplineCurve createBSplineCurve(std::vector<glm::vec3> controlPoints, int pointsPerSegment)
{
    BSplineCurve bc;
    // Gera os pontos da curva.
    bc.curvePoints = generateBSplinePoints(controlPoints, pointsPerSegment);
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Usa GL_STATIC_DRAW como dica para o OpenGL, pois a curva, uma vez gerada, não muda.
    glBufferData(GL_ARRAY_BUFFER, bc.curvePoints.size() * sizeof(glm::vec3), bc.curvePoints.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    bc.VAO = VAO;
    return bc;
}

/**
 * @brief Gera os pontos de uma curva B-Spline cúbica uniforme.
 * @param controlPoints Vetor de pontos de controle.
 * @param pointsPerSegment Número de pontos a serem gerados para cada segmento da curva.
 * @return Um vetor de pontos que formam a curva suave.
 */
// Requisito 2b: Geração de curva B-Spline a partir dos pontos de controle
std::vector<glm::vec3> generateBSplinePoints(const std::vector<glm::vec3>& controlPoints, int pointsPerSegment) {
    std::vector<glm::vec3> curvePoints;
    if (controlPoints.size() < 4)
        return curvePoints; // Mínimo 4 pontos para B-Spline cúbica

    // Matriz base da B-Spline cúbica. Esta matriz pré-calculada define a forma da curva.
    const glm::mat4 BSplineMatrix(
        -1.0f / 6.0f, 3.0f / 6.0f, -3.0f / 6.0f, 1.0f / 6.0f,
        3.0f / 6.0f, -6.0f / 6.0f, 3.0f / 6.0f, 0.0f,
        -3.0f / 6.0f, 0.0f, 3.0f / 6.0f, 0.0f,
        1.0f / 6.0f, 4.0f / 6.0f, 1.0f / 6.0f, 0.0f
    );

    float step = 1.0f / pointsPerSegment;
    // Itera sobre os segmentos da curva (cada segmento é definido por 4 pontos de controle consecutivos).
    for (size_t i = 0; i + 3 < controlPoints.size(); ++i) {
        // Para cada segmento, calcula os pontos da curva interpolando o parâmetro t de 0 a 1.
        for (float t = 0.0f; t <= 1.0f; t += step) {
            glm::vec4 T(t * t * t, t * t, t, 1.0f); // Vetor de base de potência [t^3, t^2, t, 1]
            // Matriz de geometria com os 4 pontos de controle do segmento.
            glm::mat4x3 G(controlPoints[i],
                controlPoints[i + 1],
                controlPoints[i + 2],
                controlPoints[i + 3]);
            // A equação da B-Spline em forma matricial: P(t) = G * M * T
            curvePoints.push_back(G * BSplineMatrix * T);
        }
    }
    return curvePoints;
}
// ============================================================================
// ============================================================================
// ============================================================================



// ============================================================================
// Generate TrackMesh, AnimationPoints and SceneFile
// ============================================================================
/**
 * @brief Gera a malha 3D da pista a partir da linha central.
 * @details "Extruda" a linha central para os lados, calculando vetores perpendiculares
 * em cada ponto para determinar as bordas interna e externa da pista.
 * Em seguida, conecta essas bordas para formar triângulos.
 * @param centerPoints Pontos da curva B-Spline que formam o centro da pista.
 * @param trackWidth A largura da pista.
 * @param[out] vertices Vetor de vértices da malha da pista a ser preenchido.
 * @param[out] indices Vetor de índices da malha da pista a ser preenchido.
 */
// Requisito 2c, 2d: Geração das curvas interna e externa da pista
void generateTrackMesh(const std::vector<glm::vec3> centerPoints,
                       float trackWidth,
                       std::vector<Vertex>& vertices,
                       std::vector<unsigned int>& indices)
{
    vertices.clear();
    indices.clear();
    const float halfWidth = trackWidth * 0.5f;
    size_t n = centerPoints.size();
    if (n < 2) return;

    // 1) Pre‐compute “inner” e “outer” offsets no plano XY
    std::vector<glm::vec3> innerPoints(n), outerPoints(n);
    for (size_t i = 0; i < n; ++i) {
        glm::vec3 A = centerPoints[i];
        glm::vec3 B = centerPoints[(i + 1) % n]; // Pega o próximo ponto (com wrap around para fechar a pista)

        float dx = B.x - A.x;
        float dy = B.y - A.y;
        float theta = atan2(dy, dx); // Ângulo do segmento AB.
        // ângulo perpendicular em relação ao segmento AB
        // A lógica de `dx < 0` tenta corrigir problemas em diferentes quadrantes, embora
        // um método mais simples seja usar um vetor perpendicular fixo (dy, -dx).
        float alpha = (dx < 0.0f)
            ? theta - glm::radians(90.0f)
            : theta + glm::radians(90.0f);

        float cosA = cos(alpha);
        float sinA = sin(alpha);
        glm::vec3 offset(cosA * halfWidth, sinA * halfWidth, 0.0f);

        // Os pontos da borda são o ponto central deslocado ao longo do vetor perpendicular.
        // A altura (Z) é herdada do ponto central.
        innerPoints[i] = A + offset;  // borda interna
        outerPoints[i] = A - offset;  // borda externa
    }

    // 2) Para cada segmento, crie um “quad” com 4 vértices (v0,v1,v2,v3)
    //    e 2 triângulos, usando sempre as mesmas UVs: (0,0),(1,0),(1,1),(0,1).
    //    Normal fixa = (0,0,1).
    for (size_t i = 0; i < n; ++i) {
        size_t next = (i + 1) % n;

        // Pega os 4 pontos que formam um "quad" (retângulo) da pista.
        glm::vec3 A = outerPoints[i];
        glm::vec3 B = outerPoints[next];
        glm::vec3 C = innerPoints[i];
        glm::vec3 D = innerPoints[next];

        // normal “para cima” (já que track está em Z=0)
        glm::vec3 upNormal(0.0f, 0.0f, 1.0f);

        // constrói 4 vértices do quad (sempre na ordem C, A, B, D):
        Vertex v0 = { C.x, C.y, C.z,  0.0f, 0.0f,  upNormal.x, upNormal.y, upNormal.z }; // inner[i]
        Vertex v1 = { A.x, A.y, A.z,  1.0f, 0.0f,  upNormal.x, upNormal.y, upNormal.z }; // outer[i]
        Vertex v2 = { B.x, B.y, B.z,  1.0f, 1.0f,  upNormal.x, upNormal.y, upNormal.z }; // outer[i+1]
        Vertex v3 = { D.x, D.y, D.z,  0.0f, 1.0f,  upNormal.x, upNormal.y, upNormal.z }; // inner[i+1]

        // índice base antes de inserir esses 4 vértices
        unsigned int base = static_cast<unsigned int>(vertices.size());
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);

        // Cria dois triângulos a partir dos 4 vértices para formar o quad.
        // A ordem dos índices define a orientação da face (sentido anti-horário é face frontal).
        // 1º triângulo: (v0=C, v3=D, v1=A)
        indices.push_back(base + 0);
        indices.push_back(base + 3);
        indices.push_back(base + 1);

        // 2º triângulo: (v1=A, v3=D, v2=B)
        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }
}

/**
 * @brief Exporta os pontos de animação (a linha central da pista) para um arquivo de texto.
 * @details Realiza a importante troca de coordenadas Y e Z para alinhar com o
 * sistema de coordenadas do visualizador 3D, onde Y é a altura.
 */
// Requisito 2i: Exportação dos pontos de animação
void exportAnimationPoints(const std::vector<glm::vec3>& points, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Falha ao criar " << filename << '\n';
        return;
    }

    for (const auto& p : points) {
        // O editor cria a pista no plano XY (p.x, p.y), com a altura em p.z.
        // O visualizador espera a pista no plano XZ, com a altura em Y.
        // Portanto, gravamos: X=p.x, Y=p.z (altura), Z=p.y.
        // Troca Y↔Z para alinhar com o chão XZ, mas grava p.z como altura
        file << p.x << ' '    // X
            << p.z << ' '    // Y = interpolação de altura
            << p.y << '\n';  // Z
    }
    file.close();
}

/**
 * @brief Gera o arquivo de cena (.txt) que descreve toda a cena para o visualizador,
 * incluindo objetos, materiais, configurações globais e curvas de debug.
 */
// Requisito 2j: Geração do arquivo de cena
void generateSceneFile(const std::string& trackObj, const std::string& carObj, const std::string& animFile,
    const std::string& sceneFile, const std::vector<glm::vec3>& controlPoints)
{
    std::ofstream file(sceneFile);
    // Escreve o bloco de configurações globais.
    file << "Type GlobalConfig Config\n"
        << "LightPos 2.0 10.0 2.0\n"
        << "LightColor 1.0 1.0 1.0\n"
        << "CameraPos 0.0 5.0 10.0\n"
        << "CameraFront 0.0 0.0 -1.0\n"
        << "Fov 45.0\n"
        << "NearPlane 0.1\n"
        << "FarPlane 100.0\n"
        << "Sensitivity 0.1\n"
        << "CameraSpeed 0.008\n"
        << "AttConstant 0.2\n"
        << "AttLinear 0.02\n"
        << "AttQuadratic 0.005\n"
        << "FogColor 0.5 0.5 0.5\n"
        << "FogStart 5.0\n"
        << "FogEnd 50.0\n"
        << "End\n";
    // Escreve a definição do objeto Pista.
    file << "Type Mesh Track\n"
        << "Obj " << trackObj << "\n"
        << "Mtl track.mtl\n"
        << "Scale 1.0 1.0 1.0\n"
        << "Position 0.0 0.0 0.0\n"
        << "Rotation 0.0 1.0 0.0\n"
        << "Angle 0.0 0.0 0.0\n"
        << "IncrementalAngle 0\n"
        << "End\n";
    // Escreve a definição do objeto Carro, incluindo a referência ao arquivo de animação.
    file << "Type Mesh Carro\n"
        << "Obj " << carObj << "\n"
        << "Mtl car.mtl\n"
        << "Scale 0.5 0.5 0.5\n"
        << "Position 0.0 0.0 0.0\n"
        << "Rotation 0.0 1.0 0.0\n"
        << "Angle 0.0 0.0 0.0\n"
        << "IncrementalAngle 0\n"
        << "AnimationFile " << animFile << "\n"
        << "End\n";
    // Escreve a definição da curva B-Spline para visualização de debug.
    file << "Type BSplineCurve Curve1\n";
    for (size_t i = 0; i < controlPoints.size(); ++i) {
        const auto& cp = controlPoints[i];
        // use the stored yellow‐level as height
        float yl = (i < editorPointYellowLevels.size())
            ? editorPointYellowLevels[i]
            : 0.0f;
        // Salva os pontos de controle com sua altura (yl) na coordenada Z para a curva.
        file << "ControlPoint "
            << cp.x << " "
            << cp.y << " "
            << yl << "\n";
    }
    file << "PointsPerSegment 100\n"
        << "Color 1.0 0.0 0.0 1.0\n"
        << "End\n";
    file.close();
}
// ============================================================================
// ============================================================================
// ============================================================================



/**
 * @brief Lê o arquivo de cena (.txt) e popula todas as estruturas de dados da aplicação
 * (configurações globais, objetos, curvas). Este é o parser que prepara o modo visualizador.
 */
// Requisito 3: Leitura do arquivo de cena e pontos de animação
void readSceneFile(const std::string& sceneFilePath,
    std::unordered_map<std::string, Object3D>* meshes,
    std::vector<std::string>* meshList,
    std::unordered_map<std::string, BSplineCurve>* bSplineCurves,
    GlobalConfig* globalConfig)
{
    std::ifstream file(sceneFilePath);
    std::string line;
    if (!file.is_open())
    {
        std::cerr << "Falha ao abrir o arquivo " << sceneFilePath << std::endl;
        return;
    }

    // Variáveis temporárias para armazenar os dados de um objeto enquanto ele está sendo lido.
    std::string objectType, name, objFilePath, mtlFilePath, animFile;
    glm::vec3 scale{ 1.0f }, position{ 0.0f }, rotation{ 0.0f }, angle{ 0.0f };
    GLuint incrementalAngle = false;
    std::vector<glm::vec3> tempControlPoints;
    GLuint pointsPerSegment = 0;
    glm::vec4 color{ 1.0f };

    // Lê o arquivo linha por linha.
    while (getline(file, line))
    {
        std::istringstream ss(line);
        std::string type;
        ss >> type; // O primeiro token da linha define o tipo de dado.

        // Bloco `if-else` gigante para parsear cada tipo de linha e preencher as variáveis temporárias.
        if (type == "Type")
            ss >> objectType >> name;
        else if (type == "LightPos")
            ss >> globalConfig->lightPos.x >> globalConfig->lightPos.y >> globalConfig->lightPos.z;
        else if (type == "LightColor")
            ss >> globalConfig->lightColor.r >> globalConfig->lightColor.g >> globalConfig->lightColor.b;
        else if (type == "CameraPos")
            ss >> globalConfig->cameraPos.x >> globalConfig->cameraPos.y >> globalConfig->cameraPos.z;
        else if (type == "CameraFront")
            ss >> globalConfig->cameraFront.x >> globalConfig->cameraFront.y >> globalConfig->cameraFront.z;
        else if (type == "Fov")
            ss >> globalConfig->fov;
        else if (type == "NearPlane")
            ss >> globalConfig->nearPlane;
        else if (type == "FarPlane")
            ss >> globalConfig->farPlane;
        else if (type == "Sensitivity")
            ss >> globalConfig->sensitivity;
        else if (type == "CameraSpeed")
            ss >> globalConfig->cameraSpeed;
        else if (type == "AttConstant")
            ss >> globalConfig->attConstant;
        else if (type == "AttLinear")
            ss >> globalConfig->attLinear;
        else if (type == "AttQuadratic")
            ss >> globalConfig->attQuadratic;
        else if (type == "FogColor")
            ss >> globalConfig->fogColor.r >> globalConfig->fogColor.g >> globalConfig->fogColor.b;
        else if (type == "FogStart")
            ss >> globalConfig->fogStart;
        else if (type == "FogEnd")
            ss >> globalConfig->fogEnd;
        else if (type == "Obj")
            ss >> objFilePath;
        else if (type == "Mtl")
            ss >> mtlFilePath;
        else if (type == "Scale")
            ss >> scale.x >> scale.y >> scale.z;
        else if (type == "Position")
            ss >> position.x >> position.y >> position.z;
        else if (type == "Rotation")
            ss >> rotation.x >> rotation.y >> rotation.z;
        else if (type == "Angle")
            ss >> angle.x >> angle.y >> angle.z;
        else if (type == "IncrementalAngle")
            ss >> incrementalAngle;
        else if (type == "AnimationFile")
            ss >> animFile;
        else if (type == "ControlPoint")
        {
            glm::vec3 cp;
            ss >> cp.x >> cp.y >> cp.z;
            tempControlPoints.push_back(cp);
        }
        else if (type == "PointsPerSegment")
            ss >> pointsPerSegment;
        else if (type == "Color")
            ss >> color.r >> color.g >> color.b >> color.a;
        else if (type == "End") // A diretiva "End" finaliza o objeto atual e o constrói.
        {
            if (objectType == "GlobalConfig")
            {
                // Os dados já foram preenchidos diretamente na struct globalConfig.
            }
            else if (objectType == "Mesh")
            {
                // Cria o Object3D. O construtor do Object3D irá, por sua vez,
                // parsear os arquivos .obj e .mtl especificados.
                Object3D obj(
                    /*_name=*/           name,
                    /*_objPath=*/        objFilePath,
                    /*_mtlPath=*/        mtlFilePath,
                    /*scale_=*/          scale,
                    /*pos_=*/            position,
                    /*rot_=*/            rotation,
                    /*ang_=*/            angle,
                    /*incAng=*/          incrementalAngle
                );

                // Se houver um arquivo de animação, lê os pontos e os armazena no objeto.
                if (!animFile.empty()) {
                    std::ifstream anim(animFile);
                    std::string animLine;
                    while (std::getline(anim, animLine)) {
                        std::istringstream ass(animLine);
                        glm::vec3 pos;
                        ass >> pos.x >> pos.y >> pos.z;
                        obj.animationPositions.push_back(pos);
                    }
                    anim.close();
                }

                // Insere o objeto totalmente carregado no mapa global.
                meshes->insert({ name, obj });
                meshList->push_back(name);
            }
            else if (objectType == "BSplineCurve")
            {
                // Cria a estrutura BSplineCurve com os dados lidos e a insere no mapa.
                BSplineCurve bc = createBSplineCurve(tempControlPoints, pointsPerSegment);
                bc.name = name;
                bc.controlPoints = tempControlPoints;
                bc.color = color;
                bc.pointsPerSegment = pointsPerSegment;
                bc.controlPointsVAO = generateControlPointsBuffer(tempControlPoints);
                bSplineCurves->insert(std::make_pair(name, bc));
                tempControlPoints.clear(); // Limpa para o próximo objeto do tipo curva.
            }
        }
    }
    file.close();
}



// ---------------------------------------------------------------------------
//  Cria (na primeira chamada) ou apenas atualiza o mesmo VAO/VBO
// ---------------------------------------------------------------------------
/**
 * @brief Gera (ou atualiza) o VBO/VAO para os pontos de controle do editor.
 * @details Esta função é otimizada: ela cria os buffers na primeira chamada e,
 * nas chamadas subsequentes, apenas atualiza os dados dentro do VBO existente,
 * o que é muito mais eficiente do que recriar tudo a cada frame ou clique.
 * @param controlPoints O vetor atualizado de pontos de controle.
 * @return O ID do VAO dos pontos de controle.
 */
GLuint generateControlPointsBuffer(const std::vector<glm::vec3> controlPoints)
{
    if (gCtrlPtsVAO == 0)             // primeira vez
    {
        // Cria VAO e VBO.
        glGenVertexArrays(1, &gCtrlPtsVAO);
        glGenBuffers(1, &gCtrlPtsVBO);

        glBindVertexArray(gCtrlPtsVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gCtrlPtsVBO);
        // Usa GL_DYNAMIC_DRAW como dica para o OpenGL de que estes dados serão modificados frequentemente.
        glBufferData(GL_ARRAY_BUFFER,
            controlPoints.size() * sizeof(glm::vec3),
            controlPoints.data(),
            GL_DYNAMIC_DRAW);

        // Configura o layout do atributo de vértice.
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    else                               // só reenviar dados
    {
        // Se os buffers já existem, apenas os vincula e atualiza os dados.
        glBindBuffer(GL_ARRAY_BUFFER, gCtrlPtsVBO);
        // glBufferSubData poderia ser mais eficiente se o tamanho do buffer não mudasse,
        // mas glBufferData com o mesmo buffer ID funciona e lida com o redimensionamento.
        glBufferData(GL_ARRAY_BUFFER,
            controlPoints.size() * sizeof(glm::vec3),
            controlPoints.data(),
            GL_DYNAMIC_DRAW);
    }

    return gCtrlPtsVAO;                // devolve sempre o mesmo VAO
}