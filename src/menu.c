#include "menu.h"
#include "nucleo.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_MAX 512
#define LINHA_MAX 512
#define MAX_CATALOGO 150000U

/* Entrada por linha: aceita caminhos com espacos e rejeita linhas truncadas. */
/* Funcao ler_texto: Le uma linha de texto com limite e remove quebras de linha. */
static int ler_texto(const char *pergunta, char *destino, size_t capacidade) {
    if (pergunta != NULL) {
        fputs(pergunta, stdout);
        fflush(stdout);
    }

    if (destino == NULL || capacidade < 2 || capacidade > INT32_MAX ||
        fgets(destino, (int)capacidade, stdin) == NULL) {
        return 0;
    }

    size_t fim = strcspn(destino, "\r\n");
    if (destino[fim] == '\0' && fim == capacidade - 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
        }
        puts("Entrada muito longa. Tente novamente.");
        return 0;
    }
    destino[fim] = '\0';
    return 1;
}

/* Funcao ler_uint64: Solicita e valida um inteiro dentro dos limites informados. */
static int ler_uint64(const char *pergunta, uint64_t minimo,
                      uint64_t maximo, uint64_t *valor) {
    char linha[LINHA_MAX];
    if (valor == NULL || !ler_texto(pergunta, linha, sizeof(linha)) ||
        linha[0] == '\0' || linha[0] == '-') {
        return 0;
    }

    char *fim = NULL;
    errno = 0;
    unsigned long long convertido = strtoull(linha, &fim, 10);
    if (errno != 0 || fim == linha || *fim != '\0' ||
        convertido < minimo || convertido > maximo) {
        return 0;
    }

    *valor = (uint64_t)convertido;
    return 1;
}

/* Funcao ler_caminho: Recebe um caminho de arquivo e aplica a opcao padrao. */
static int ler_caminho(const char *pergunta, char *caminho,
                       size_t capacidade, const char *padrao) {
    if (!ler_texto(pergunta, caminho, capacidade)) {
        return 0;
    }
    if (caminho[0] == '\0' && padrao != NULL) {
        size_t tamanho = strlen(padrao);
        if (tamanho >= capacidade) {
            return 0;
        }
        memcpy(caminho, padrao, tamanho + 1);
    }
    return caminho[0] != '\0';
}

/* Impede sobrescrever pelo menu os arquivos que sustentam a monografia. */
/* Funcao caminho_protegido: Impede que o menu sobrescreva dados originais do relatorio. */
static int caminho_protegido(const char *caminho) {
    const char *nome = caminho;
    for (const char *p = caminho; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            nome = p + 1;
        }
    }
    char minusculas[CAMINHO_MAX];
    size_t i = 0;
    for (; nome[i] != '\0' && i + 1 < sizeof(minusculas); ++i) {
        minusculas[i] = (char)tolower((unsigned char)nome[i]);
    }
    minusculas[i] = '\0';
    return strcmp(minusculas, "resultados_brutos.csv") == 0 ||
           strcmp(minusculas, "catalogo_exemplo.csv") == 0;
}

/* Funcao analisar_arquivo: Chama o nucleo para importar, ordenar e validar um CSV. */
static int analisar_arquivo(const char *caminho) {
    char *argumentos[] = {"aps_ordenacao", "--analyze", (char *)caminho, NULL};
    return aps_core_main(3, argumentos);
}

/* Funcao opcao_arquivo: Implementa a opcao do menu de leitura de CSV externo. */
static void opcao_arquivo(void) {
    char caminho[CAMINHO_MAX];
    puts("\n1 - ANALISAR CATALOGO DE IMAGENS (CSV)");
    puts("Formato: id,instante_min,nuvens_pct,setor");
    if (!ler_caminho("Arquivo [catalogo_exemplo.csv]: ", caminho,
                     sizeof(caminho), "catalogo_exemplo.csv")) {
        puts("Caminho invalido.");
        return;
    }

    puts("Os arquivos antes_ordenacao.csv e depois_*.csv podem ser substituidos.");
    if (analisar_arquivo(caminho) != 0) {
        puts("A analise nao foi concluida. Verifique o CSV.");
    }
}

/* Funcao opcao_manual: Implementa a opcao de digitacao manual dos registros. */
static void opcao_manual(void) {
    char *argumentos[] = {"aps_ordenacao", "--manual", NULL};
    puts("\n2 - INFORMAR REGISTROS MANUALMENTE");
    puts("Digite a quantidade e cada registro no formato solicitado.");
    puts("Os arquivos antes_ordenacao.csv e depois_*.csv podem ser substituidos.");
    if (aps_core_main(2, argumentos) != 0) {
        puts("Nao foi possivel concluir a analise manual.");
    }
}

/* Funcao opcao_gerar: Configura quantidade, cenario e semente para gerar catalogo. */
static void opcao_gerar(void) {
    static const char *cenarios[] = {
        "aleatorio", "ordenado", "inverso", "quase"
    };
    uint64_t quantidade, cenario, semente;
    char quantidade_txt[32], semente_txt[32], caminho[CAMINHO_MAX], resposta[LINHA_MAX];

    puts("\n3 - GERAR CATALOGO SINTETICO DE IMAGENS");
    if (!ler_uint64("Numero de registros (1-150000): ",
                    1, MAX_CATALOGO, &quantidade)) {
        puts("Quantidade invalida.");
        return;
    }
    puts("1. Aleatorio   2. Ordenado   3. Invertido   4. Quase ordenado");
    if (!ler_uint64("Cenario (1-4): ", 1, 4, &cenario)) {
        puts("Cenario invalido.");
        return;
    }
    if (!ler_uint64("Semente numerica (ex.: 42): ", 0, UINT64_MAX, &semente)) {
        puts("Semente invalida.");
        return;
    }
    if (!ler_caminho("Nome do novo CSV [novo_catalogo.csv]: ",
                     caminho, sizeof(caminho), "novo_catalogo.csv")) {
        puts("Nome do arquivo invalido.");
        return;
    }

    if (caminho_protegido(caminho)) {
        puts("Esse nome pertence aos dados originais da monografia. Use outro.");
        return;
    }

    snprintf(quantidade_txt, sizeof(quantidade_txt), "%" PRIu64, quantidade);
    snprintf(semente_txt, sizeof(semente_txt), "%" PRIu64, semente);
    char *argumentos[] = {"aps_ordenacao", "--generate", quantidade_txt,
                         (char *)cenarios[cenario - 1], semente_txt, caminho, NULL};
    if (aps_core_main(6, argumentos) != 0) {
        puts("Falha na geracao do CSV.");
        return;
    }

    if (!ler_texto("Ordenar e comparar agora? [S/n]: ",
                   resposta, sizeof(resposta))) {
        return;
    }
    if (resposta[0] != 'n' && resposta[0] != 'N') {
        puts("Os arquivos antes_ordenacao.csv e depois_*.csv podem ser substituidos.");
        if (analisar_arquivo(caminho) != 0) {
            puts("O arquivo foi gerado, mas nao pode ser analisado.");
        }
    }
}

/* Funcao opcao_benchmark: Solicita destino e inicia as medicoes comparativas completas. */
static void opcao_benchmark(void) {
    char caminho[CAMINHO_MAX];
    puts("\n4 - EXECUTAR COMPARACAO COMPLETA DE DESEMPENHO");
    puts("Quatro algoritmos, quatro cenarios, quatro tamanhos e tres repeticoes.");
    puts("AVISO: em 100 mil registros, Insertion Sort pode demorar.");
    puts("Use outro nome de arquivo para preservar resultados_brutos.csv.");
    if (!ler_caminho("Salvar novos testes em [benchmark_novo.csv]: ",
                     caminho, sizeof(caminho), "benchmark_novo.csv")) {
        puts("Caminho invalido.");
        return;
    }
    if (caminho_protegido(caminho)) {
        puts("Para proteger os arquivos usados na monografia, escolha outro nome.");
        return;
    }
    char *argumentos[] = {"aps_ordenacao", "--bench", caminho, NULL};
    if (aps_core_main(3, argumentos) != 0) {
        puts("Falha durante o benchmark.");
    }
}

/* Funcao opcao_testes: Aciona o conjunto de verificacoes automaticas do nucleo. */
static void opcao_testes(void) {
    char *argumentos[] = {"aps_ordenacao", "--selftest", NULL};
    puts("\n5 - TESTES AUTOMATIZADOS");
    if (aps_core_main(2, argumentos) != 0) {
        puts("ATENCAO: pelo menos um teste falhou.");
    }
}

/* Funcao opcao_resultados: Le e apresenta um resumo de um CSV de resultados existente. */
static void opcao_resultados(void) {
    char caminho[CAMINHO_MAX], linha[LINHA_MAX];
    puts("\n6 - VISUALIZAR RESULTADOS DE BENCHMARK ANTERIOR");
    if (!ler_caminho("Arquivo CSV [resultados_brutos.csv]: ",
                     caminho, sizeof(caminho), "resultados_brutos.csv")) {
        puts("Caminho invalido.");
        return;
    }
    FILE *arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        perror("Nao foi possivel abrir os resultados");
        return;
    }
    puts("Primeiras 10 linhas de dados (a tabela completa esta no CSV):");
    int linhas = 0;
    while (linhas <= 10 && fgets(linha, sizeof(linha), arquivo) != NULL) {
        fputs(linha, stdout);
        ++linhas;
    }
    if (ferror(arquivo)) {
        perror("Erro de leitura do relatorio");
    }
    fclose(arquivo);
}

/* Funcao executar_menu_principal: Exibe o menu e despacha as opcoes escolhidas pelo usuario. */
int executar_menu_principal(void) {
    char escolha[LINHA_MAX];
    uint64_t opcao;

    for (;;) {
        puts("\n===================================================");
        puts(" APS - ANALISE DE ALGORITMOS DE ORDENACAO EM C");
        puts(" Catalogo simulado de imagens da Amazonia");
        puts("===================================================");
        puts("1. Carregar CSV, ordenar e comparar os 4 algoritmos");
        puts("2. Digitar registros, ordenar e comparar");
        puts("3. Gerar registros aleatorios e opcionalmente analisar");
        puts("4. Executar benchmark (4 cenarios, 4 tamanhos)");
        puts("5. Rodar testes automaticos (96 combinacoes)");
        puts("6. Visualizar resultados anteriores");
        puts("7. Sair");
        if (!ler_texto("Escolha uma opcao (1-7): ", escolha, sizeof(escolha))) {
            return 0;
        }
        char *fim = NULL;
        errno = 0;
        unsigned long numero = strtoul(escolha, &fim, 10);
        opcao = (errno == 0 && fim != escolha && *fim == '\0') ? numero : 0;

        switch (opcao) {
            case 1: opcao_arquivo(); break;
            case 2: opcao_manual(); break;
            case 3: opcao_gerar(); break;
            case 4: opcao_benchmark(); break;
            case 5: opcao_testes(); break;
            case 6: opcao_resultados(); break;
            case 7: puts("Programa encerrado."); return 0;
            default: puts("Opcao invalida. Escolha um numero entre 1 e 7."); break;
        }
    }
}
