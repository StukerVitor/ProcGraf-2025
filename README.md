# Documentação Completa do Projeto: Modelador de Pistas e Visualizador 3D

Este projeto é uma aplicação gráfica completa escrita em C++ com OpenGL, que integra duas funcionalidades principais em um único executável:
1.  **Um Editor 2D Interativo:** Permite ao usuário criar a trajetória de uma pista de corrida clicando na tela para adicionar pontos de controle. O usuário também pode definir a altura em cada ponto, gerando uma pista 3D.
2.  **Um Visualizador 3D:** Carrega a pista gerada, um modelo de carro e renderiza uma cena completa com iluminação, texturas e uma animação do carro percorrendo a pista.

A arquitetura do projeto é dividida em quatro arquivos principais, cada um com uma responsabilidade bem definida.

### Índice
1.  [**`Shader.h` e `Shader.cpp`**](#1-shaderh-e-shadercpp-abstração-do-shader-program)
2.  [**`GeometryObjects.hpp`**](#2-geometryobjectshpp-a-bíblia-dos-objetos-3d)
3.  [**`Origem.cpp`**](#3-origemcpp-o-coração-da-aplicação)

---

### 1. `Shader.h` e `Shader.cpp`: Abstração do Shader Program

Esses dois arquivos criam uma classe `Shader` muito útil. Em OpenGL, compilar e linkar shaders envolve um monte de código repetitivo (criar shader, carregar fonte, compilar, checar erros, criar programa, anexar, linkar, checar erros...). A classe `Shader` encapsula toda essa lógica, tornando o código principal muito mais limpo.

#### `Shader.h`
Este é o arquivo de cabeçalho. Ele define a interface da classe `Shader`.
* **`#pragma once`**: Garante que o conteúdo deste arquivo seja incluído apenas uma vez na compilação, evitando erros de redefinição.
* **`class Shader`**:
    * **`private: GLuint id;`**: O único membro privado é um `GLuint`, que armazena o ID (identificador) do programa shader final que é gerado pelo OpenGL.
    * **`public:`**:
        * **`Shader(const std::string&, const std::string&)`**: Construtor que aceita o caminho para o arquivo de Vertex Shader e para o arquivo de Fragment Shader.
        * **`Shader(const char*, const char*, bool)`**: Um construtor sobrecarregado que aceita o código dos shaders diretamente como strings de C. Isso é útil para embutir shaders simples diretamente no código C++, como foi feito em `Origem.cpp`.
        * **`void setTextureUniform()`**: Um método auxiliar para definir a uniforme da textura.
        * **`GLuint getId()`**: Um getter simples que retorna o ID do programa shader.

#### `Shader.cpp`
Aqui está a implementação da classe.
* **Construtor (por arquivo)**:
    1.  Abre os arquivos de shader usando `std::ifstream`.
    2.  Lê todo o conteúdo dos arquivos para dentro de `std::stringstream` e, em seguida, para `std::string`.
    3.  Converte as `std::string` para `const GLchar*`.
    4.  **Compilação**: Chama `glCreateShader`, `glShaderSource` e `glCompileShader` tanto para o vertex quanto para o fragment shader. Após cada compilação, ele verifica se houve sucesso com `glGetShaderiv` e, em caso de falha, imprime o log de erro com `glGetShaderInfoLog`.
    5.  **Linkagem**: Cria um programa com `glCreateProgram`, anexa os shaders compilados com `glAttachShader` e os linka com `glLinkProgram`. Da mesma forma, verifica se a linkagem teve sucesso e reporta erros.
    6.  **Limpeza**: Após a linkagem, os objetos de shader individuais não são mais necessários, então eles são deletados com `glDeleteShader`.

* **Construtor (por string)**: A lógica é idêntica à anterior, mas pula a parte de leitura de arquivos, usando diretamente as strings de código fonte fornecidas.

Esta classe é um excelente exemplo de **RAII (Resource Acquisition Is Initialization)**. O recurso (o programa shader do OpenGL) é adquirido no construtor e, embora não haja um destrutor explícito para liberar o `glDeleteProgram`, a vida útil do objeto `Shader` está atrelada à da aplicação, sendo uma abstração eficaz.

---

### 2. `GeometryObjects.hpp`: A Bíblia dos Objetos 3D

Este arquivo é o mais denso e talvez o mais importante para a organização dos dados. Ele define todas as estruturas de dados necessárias para representar um objeto 3D, desde um simples vértice até um objeto completo com material, textura e transformações. Ele também contém as funções para carregar, configurar e salvar esses dados.

#### Estruturas de Dados Auxiliares
* **`Vec2`, `Vec3`**: Structs simples para vetores de 2 e 3 floats. Representam coordenadas de textura (UV), posições de vértice (XYZ) e vetores normais. Possuem o operador `==` sobrecarregado, que é crucial para a função de escrita de `.obj`.
* **`Face`**: Representa uma única face poligonal (geralmente um triângulo). Armazena vetores de vértices, normais e coordenadas de textura que compõem aquela face.
* **`Group`**: Em arquivos `.obj`, faces podem ser agrupadas e associadas a um material. Esta struct armazena o nome do grupo, o nome do material (`mtlName`) e uma lista de `Face`s.
* **`Vertex`**: Esta é uma estrutura **CRUCIAL**. Ela representa o formato de dados *intercalado* (interleaved) que será enviado para a GPU. Em vez de ter três VBOs (um para posições, um para UVs, um para normais), temos um único VBO onde para cada vértice, guardamos sua posição, depois sua UV, depois sua normal. Isso geralmente melhora a performance de cache da GPU.
* **`Material`**: Armazena as propriedades de um material, lidas de um arquivo `.mtl`:
    * `Ka`, `Kd`, `Ks`: Coeficientes de reflexão ambiente, difusa e especular (cores).
    * `Ns`: O expoente especular (shininess).
    * `textureName`: O nome do arquivo da textura difusa (`map_Kd`).

#### Funções Auxiliares de Configuração (OpenGL)
* **`setupMtl(const std::string& path)`**: Abre e lê um arquivo `.mtl`. Ele procura por palavras-chave ("Ka", "Kd", etc.) e preenche uma struct `Material` com os valores encontrados.
* **`setupTexture(const std::string& filename)`**: Uma função completa para carregar uma imagem (usando a popular biblioteca `stb_image.h`) e criar uma textura OpenGL. Ela:
    1.  Gera um ID de textura (`glGenTextures`).
    2.  Configura parâmetros de wrapping (`GL_REPEAT`) e filtering (`GL_LINEAR_MIPMAP_LINEAR`).
    3.  Carrega os dados da imagem com `stbi_load`.
    4.  Envia os dados para a GPU com `glTexImage2D`.
    5.  Gera mipmaps com `glGenerateMipmap` para melhor qualidade de imagem em diferentes distâncias.
* **`setupGeometry(const std::vector<Vertex>& vertices)`**: O coração da preparação de geometria para a GPU.
    1.  Cria um VBO (`glGenBuffers`) e um VAO (`glGenVertexArrays`).
    2.  Envia os dados do vetor de `Vertex` para o VBO com `glBufferData`.
    3.  Configura os **Vertex Attribute Pointers** (`glVertexAttribPointer`) dentro do VAO. Isso diz à GPU como interpretar os dados brutos no VBO:
        * **`location = 0`**: Posição (3 floats).
        * **`location = 1`**: Coordenadas de textura (2 floats).
        * **`location = 3`**: Normal (3 floats).
        O *stride* (passo) é `sizeof(Vertex)`, e o *offset* (deslocamento) é calculado para apontar para o início de cada atributo dentro da struct.

#### Estruturas Principais
* **`Mesh`**: Representa a geometria "pura" de um objeto.
    * Contém os dados brutos em vetores paralelos (`vertices`, `mappings`, `normals`) e também os `groups`.
    * Possui um `GLuint VAO` que armazena o ID do VAO configurado por `setupGeometry`.
    * **Construtores**: Oferece dois construtores flexíveis:
        1.  Um que recebe os dados já separados (posições, uvs, normais) e os intercala para criar o VAO. É usado pelo parser de `.obj`.
        2.  Um que recebe dados já intercalados (`std::vector<Vertex>`) e índices. Este é usado para a pista gerada proceduralmente. Ele reconstrói os vetores separados e as faces a partir dos dados intercalados, o que é um pouco redundante mas funciona.

* **`Object3D`**: A classe de mais alto nível. Representa um objeto completo na cena.
    * Contém uma `Mesh`, propriedades de transformação (posição, escala, rotação com `glm::vec3`), referências aos arquivos `.obj` e `.mtl`, um objeto `Material` e o `textureID`. Crucialmente, também possui um `std::vector<glm::vec3> animationPositions` para armazenar a trajetória do carro.
    * **Construtor Principal**: Este é um parser de arquivos `.obj` completo.
        1.  Abre o arquivo `.obj` e lê linha por linha.
        2.  **`v`, `vt`, `vn`**: Armazena todas as posições, UVs e normais do arquivo em vetores "brutos" (`raw_positions`, etc.).
        3.  **`usemtl`**: Quando encontra esta diretiva, cria um novo `Group`.
        4.  **`f`**: Processa as faces. A lógica aqui é robusta:
            * Suporta polígonos com mais de 3 vértices, triangulando-os com uma abordagem de "triangle-fan".
            * Analisa os índices de cada vértice da face. O formato `v/t/n` pode variar (e.g., `v//n` se não houver textura). O código lida com todos esses casos, extraindo os índices de posição (v), textura (t) e normal (n).
            * Usa os índices para buscar os dados nos vetores "brutos" e os adiciona aos vetores finais (`positions`, `texcoords`, `normals`). Essa duplicação de dados (de "raw" para final) é necessária porque o `.obj` pode reutilizar um mesmo vértice de posição com diferentes normais ou UVs, enquanto o formato de VBO que usamos exige que cada vértice final tenha um único conjunto de atributos.
        5.  Após o parsing, ele chama o construtor do `Mesh`, e em seguida `setupMtl` e `setupTexture` para carregar os assets associados.

* **`OBJWriter` e `Object3DWriter`**:
    * Implementam a funcionalidade inversa: salvar uma `Mesh` em um arquivo `.obj`.
    * A lógica de `write` na `OBJWriter` é interessante. Para cada vértice de uma face, ela precisa descobrir qual é seu índice no vetor global de vértices da `Mesh`. Ela faz isso usando `std::find`, o que pode ser lento para malhas muito grandes, mas é uma solução correta e simples de implementar.

---

### 3. `Origem.cpp`: O Coração da Aplicação

Este arquivo contém a função `main` e toda a lógica da aplicação, incluindo o loop de renderização, o tratamento de inputs e a geração procedural da pista.

#### Estrutura Geral e Globais
* O código utiliza várias variáveis globais (`meshes`, `bSplineCurves`, `globalConfig`, `editorMode`, etc.) para gerenciar o estado da aplicação. Embora não seja a prática ideal em engenharia de software de grande escala, para um projeto deste escopo, simplifica a comunicação entre as funções de callback e o loop principal.
* **`GlobalConfig`**: Uma struct que centraliza todas as configurações globais da cena (câmera, luz, fog, atenuação), o que organiza bem os parâmetros.
* **Shaders Embutidos**: Os códigos do Vertex e Fragment Shader principais são embutidos como strings `R"glsl(...)"`. Isso simplifica a distribuição do programa, que não precisa carregar arquivos de shader externos.

#### A Função `main()`
1.  **Inicialização**: Configura GLFW, cria uma janela, inicializa GLAD e define os callbacks de teclado e mouse. Ativa o teste de profundidade (`glEnable(GL_DEPTH_TEST)`).
2.  **Criação dos Shaders**: Instancia os objetos `Shader`, um para os objetos 3D e outro mais simples para desenhar linhas e pontos (`lineShader`).
3.  **Loop de Renderização (`while`)**: Este é o ciclo de vida da aplicação.
    * **Eventos e Limpeza**: `glfwPollEvents()` processa inputs; `glClear()` limpa os buffers de cor e profundidade.
    * **Matrizes de View e Projection**: As matrizes de câmera e projeção são calculadas uma vez por frame e são usadas para todos os objetos renderizados.
    * **Modo Editor (`if (editorMode)`)**:
        * Usa o `lineShader`.
        * Chama `generateControlPointsBuffer` para criar/atualizar um VBO com os pontos de controle clicados pelo usuário.
        * Desenha os pontos na tela (`glDrawArrays(GL_POINTS, ...)`). A cor de cada ponto é calculada com base em seu nível de "amarelo", que representa a altura (`yl`).
    * **Modo Visualizador (`else`)**:
        * **Movimento da Câmera**: Atualiza a posição da câmera com base nas flags `moveW`, `moveA`, etc., que são alteradas no `key_callback`.
        * **Configuração do Shader Principal**: Ativa o `objectShader` e envia todas as uniformes globais (propriedades da luz, da câmera, do fog, da atenuação).
        * **Renderização dos Objetos**: Itera sobre o mapa `meshes`. Para cada `Object3D`:
            * **Animação do Carro**: Se o objeto é o "Carro", uma lógica especial é acionada. Ela usa o ponto de animação atual, o anterior e o próximo para calcular a direção do movimento. Com base nessa direção, ela constrói uma **base ortonormal** (vetores `forward`, `right`, `upAxis`) que define a orientação completa do carro. Isso permite que o carro não apenas siga a curva, mas também se incline corretamente nas subidas e descidas. A matriz de modelo é montada com `translate * rot * scale`.
            * **Outros Objetos**: Para os demais objetos (a pista), a matriz de modelo é montada de forma mais simples.
            * **Uniforms de Material**: As propriedades do material do objeto (`Ka`, `Kd`, `Ks`, `Ns`) são enviadas para o shader.
            * **Desenho**: O VAO do objeto é ativado (`glBindVertexArray`), a textura é vinculada (`glBindTexture`), e a chamada de desenho (`glDrawArrays`) é feita.
        * **Desenho das Curvas B-Spline**: Se a opção estiver ativa, as curvas B-Spline (que definem a pista) são desenhadas usando o `lineShader`.
        * **Atualização da Animação**: Um acumulador de tempo (`animAccumulator`) garante que o carro avance na animação a uma taxa fixa (30 passos por segundo), independentemente do framerate da aplicação.

#### Funções de Callback e Lógica
* **`mouse_button_callback`**:
    * Captura cliques do mouse no modo editor.
    * A parte mais complexa é a **conversão de coordenadas de tela para coordenadas de mundo**. O processo é o inverso do pipeline de renderização:
        1.  Coordenadas de Tela (pixels) → Coordenadas Normalizadas de Dispositivo (NDC, de -1 a 1).
        2.  NDC → Coordenadas de Olho (Eye/View Space), aplicando a inversa da matriz de projeção.
        3.  Coordenadas de Olho → Coordenadas de Mundo (World Space), aplicando a inversa da matriz de visão.
        Isso define um raio que parte da câmera e passa pelo ponto clicado. A função então calcula onde esse raio intercepta o plano XY (`z=0`), e esse ponto de interseção é adicionado como um novo ponto de controle.

* **`key_callback`**:
    * **`+` e `-`**: No modo editor, ajustam a altura (`currentYellowLevel`) do último ponto adicionado.
    * **`W, A, S, D`**: Controlam a câmera no modo visualizador.
    * **`ESPAÇO`**: A tecla mágica que transita do modo editor para o visualizador. Ela executa uma sequência de passos fundamentais:
        1.  Combina os pontos 2D do editor (`editorControlPoints`) com as alturas (`editorPointYellowLevels`) para criar pontos de controle 3D.
        2.  Gera os pontos da curva B-Spline central da pista (`generateBSplinePoints`).
        3.  Gera a malha da pista (vértices e índices) com base na curva central (`generateTrackMesh`).
        4.  **Troca as coordenadas Y e Z** dos vértices e normais da malha da pista. Isso é feito porque a pista é criada no plano XY no editor, mas a cena 3D considera o chão como o plano XZ.
        5.  Cria um objeto `Mesh` para a pista e o salva em `track.obj` usando `OBJWriter`.
        6.  Exporta os pontos da curva (com Y e Z trocados) para `animation.txt`.
        7.  Gera um arquivo de cena (`Scene.txt`) que descreve toda a cena 3D.
        8.  Finalmente, **chama `readSceneFile` para carregar a cena que acabou de criar**, populando as estruturas de dados para o modo visualizador.
        9.  Muda o modo e o comportamento do cursor.

* **Geração de Geometria**:
    * **`generateBSplinePoints`**: Uma implementação matemática padrão de B-splines cúbicas uniformes, usando a formulação matricial para calcular os pontos da curva a partir de grupos de 4 pontos de controle.
    * **`generateTrackMesh`**: Algoritmo inteligente que "extrude" a curva central para os lados. Para cada segmento da curva, ele calcula um vetor perpendicular para encontrar os pontos das bordas interna e externa da pista. Em seguida, conecta esses pontos para formar "quads" (retângulos), que são divididos em dois triângulos cada, formando a malha da pista. As coordenadas de textura são atribuídas de forma a mapear uma textura repetidamente ao longo da pista.
    * **`generateSceneFile` e `exportAnimationPoints`**: Funções de escrita de arquivo que serializam o trabalho feito no editor em um formato persistente que pode ser lido pelo visualizador.

* **`readSceneFile`**: Um parser de texto customizado para o formato de arquivo de cena `.txt`. Ele lê a cena, objeto por objeto, configurando as `GlobalConfig`, criando os `Object3D` (o que, por sua vez, dispara a leitura dos `.obj` e `.mtl`), carregando os pontos de animação e definindo as curvas a serem exibidas.