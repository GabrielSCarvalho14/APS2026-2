/*
 * APS Estrutura de Dados 2026/2 - Analise de ordenacao de catalogos de imagens.
 * Linguagem C11. Os dados sao SINTETICOS, nao sao imagens do INPE.
 * Compilar: gcc -std=c11 -O2 -Wall -Wextra -pedantic aps_ordenacao.c -o aps_ordenacao
 */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_REGISTROS 150000u
#define MAX_LINHA 256
#define SETOR_TAM 16
#define REPETICOES 3

typedef struct {
    uint32_t id;
    uint64_t instante_min; /* Minutos transcorridos desde 1970-01-01 UTC */
    uint8_t nuvens_pct;
    char setor[SETOR_TAM];
} Registro;

typedef enum { ALEATORIO, ORDENADO, INVERSO, QUASE_ORDENADO } Cenario;
typedef enum { INSERCAO, INTERCALACAO, RAPIDO, HEAP, QT_ALGORITMOS } Algoritmo;

static uint64_t estado_rng = UINT64_C(1);
static uint64_t comparacoes = 0;

/* Funcao rng64: Atualiza o estado pseudoaleatorio e produz um numero de 64 bits. */
static uint64_t rng64(void) {
    uint64_t x = estado_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    estado_rng = x;
    return x * UINT64_C(2685821657736338717);
}

/* Funcao sorteio: Limita o numero pseudoaleatorio ao intervalo informado. */
static uint64_t sorteio(uint64_t teto) { return teto ? rng64() % teto : 0; }

/* Funcao comparar: Compara dois registros pelos campos de ordenacao e conta a comparacao. */
static int comparar(const Registro *a, const Registro *b) {
    ++comparacoes;
    if (a->instante_min < b->instante_min) return -1;
    if (a->instante_min > b->instante_min) return 1;
    if (a->id < b->id) return -1;
    if (a->id > b->id) return 1;
    if (a->nuvens_pct < b->nuvens_pct) return -1;
    if (a->nuvens_pct > b->nuvens_pct) return 1;
    return strcmp(a->setor, b->setor);
}

/* Funcao iguais: Confere se todos os campos dos dois registros coincidem. */
static int iguais(const Registro *a, const Registro *b) {
    return a->id == b->id && a->instante_min == b->instante_min &&
           a->nuvens_pct == b->nuvens_pct && strcmp(a->setor, b->setor) == 0;
}

/* Funcao trocar: Permuta dois registros completos do vetor. */
static void trocar(Registro *a, Registro *b) {
    Registro t = *a;
    *a = *b;
    *b = t;
}

/* Funcao insertion_intervalo: Ordena por insercao um trecho delimitado do vetor. */
static void insertion_intervalo(Registro *v, size_t ini, size_t fim) {
    for (size_t i = ini + 1; i < fim; ++i) {
        Registro chave = v[i];
        size_t j = i;
        while (j > ini && comparar(&chave, &v[j - 1]) < 0) {
            v[j] = v[j - 1];
            --j;
        }
        v[j] = chave;
    }
}

/* Funcao insertion_sort: Aplica Insertion Sort sobre todo o vetor. */
static void insertion_sort(Registro *v, size_t n) {
    insertion_intervalo(v, 0, n);
}

/* Funcao merge_intervalo: Divide os dados recursivamente e intercala as partes ordenadas. */
static void merge_intervalo(Registro *v, Registro *aux, size_t ini, size_t fim) {
    if (fim - ini <= 1) return;
    size_t meio = ini + (fim - ini) / 2;
    merge_intervalo(v, aux, ini, meio);
    merge_intervalo(v, aux, meio, fim);
    size_t i = ini, j = meio, k = ini;
    while (i < meio && j < fim)
        aux[k++] = comparar(&v[i], &v[j]) <= 0 ? v[i++] : v[j++];
    while (i < meio) aux[k++] = v[i++];
    while (j < fim) aux[k++] = v[j++];
    memcpy(v + ini, aux + ini, (fim - ini) * sizeof(*v));
}

/* Funcao merge_sort: Aloca memoria auxiliar, executa Merge Sort e libera a area usada. */
static int merge_sort(Registro *v, size_t n) {
    if (n <= 1) return 1;
    Registro *aux = malloc(n * sizeof(*aux));
    if (!aux) return 0;
    merge_intervalo(v, aux, 0, n);
    free(aux);
    return 1;
}

/* Funcao mediana_tres: Escolhe o elemento mediano entre tres candidatos de pivo. */
static Registro mediana_tres(Registro a, Registro b, Registro c) {
    if (comparar(&a, &b) > 0) trocar(&a, &b);
    if (comparar(&b, &c) > 0) trocar(&b, &c);
    if (comparar(&a, &b) > 0) trocar(&a, &b);
    return b;
}

/* Particao de tres vias: adequado tambem a chaves repetidas.
 * A recursao ocorre apenas na menor particao, limitando a pilha a O(log n).
 */
/* Funcao quick_intervalo: Particiona em tres vias e ordena recursivamente faixas do vetor. */
static void quick_intervalo(Registro *v, size_t ini, size_t fim) {
    while (fim - ini > 16) {
        Registro pivo = mediana_tres(v[ini], v[ini + (fim - ini) / 2], v[fim - 1]);
        size_t menor = ini, atual = ini, maior = fim;
        while (atual < maior) {
            int c = comparar(&v[atual], &pivo);
            if (c < 0) trocar(&v[menor++], &v[atual++]);
            else if (c > 0) trocar(&v[atual], &v[--maior]);
            else ++atual;
        }
        if (menor - ini < fim - maior) {
            quick_intervalo(v, ini, menor);
            ini = maior;
        } else {
            quick_intervalo(v, maior, fim);
            fim = menor;
        }
    }
    insertion_intervalo(v, ini, fim);
}

/* Funcao quick_sort: Inicia o Quick Sort para a colecao recebida. */
static void quick_sort(Registro *v, size_t n) {
    quick_intervalo(v, 0, n);
}

/* Funcao descer_heap: Restaura a propriedade de heap maximo a partir de uma raiz. */
static void descer_heap(Registro *v, size_t tamanho, size_t raiz) {
    for (;;) {
        if (raiz > (tamanho - 1) / 2) return;
        size_t filho = 2 * raiz + 1;
        if (filho >= tamanho) return;
        if (filho + 1 < tamanho && comparar(&v[filho], &v[filho + 1]) < 0)
            ++filho;
        if (comparar(&v[raiz], &v[filho]) >= 0) return;
        trocar(&v[raiz], &v[filho]);
        raiz = filho;
    }
}

/* Funcao heap_sort: Constroi o heap e fixa sucessivamente os maiores registros. */
static void heap_sort(Registro *v, size_t n) {
    if (n <= 1) return;
    for (size_t i = n / 2; i > 0; --i) descer_heap(v, n, i - 1);
    for (size_t fim = n; fim > 1; --fim) {
        trocar(&v[0], &v[fim - 1]);
        descer_heap(v, fim - 1, 0);
    }
}

/* Funcao nome_alg: Retorna o nome textual do algoritmo selecionado. */
static const char *nome_alg(Algoritmo a) {
    static const char *nomes[] = {"Insertion", "Merge", "Quick", "Heap"};
    return nomes[a];
}

/* Funcao nome_cenario: Retorna o nome textual do perfil de entrada selecionado. */
static const char *nome_cenario(Cenario c) {
    static const char *nomes[] = {"aleatorio", "ordenado", "inverso", "quase"};
    return nomes[c];
}

/* Funcao executar: Encaminha o vetor para o algoritmo de ordenacao escolhido. */
static int executar(Algoritmo a, Registro *v, size_t n) {
    switch (a) {
        case INSERCAO: insertion_sort(v, n); return 1;
        case INTERCALACAO: return merge_sort(v, n);
        case RAPIDO: quick_sort(v, n); return 1;
        case HEAP: heap_sort(v, n); return 1;
        default: return 0;
    }
}

/* Funcao validacao_ordenacao: Verifica se a saida esta em ordem nao decrescente. */
static int validacao_ordenacao(const Registro *v, size_t n) {
    uint64_t anterior = comparacoes;
    for (size_t i = 1; i < n; ++i) {
        if (comparar(&v[i - 1], &v[i]) > 0) {
            comparacoes = anterior;
            return 0;
        }
    }
    comparacoes = anterior;
    return 1;
}

/* Funcao validar_igualdade: Confere a igualdade integral com os registros de referencia. */
static int validar_igualdade(const Registro *a, const Registro *b, size_t n) {
    for (size_t i = 0; i < n; ++i) if (!iguais(&a[i], &b[i])) return 0;
    return 1;
}

/* Funcao alocar: Reserva com seguranca a memoria necessaria para o vetor. */
static Registro *alocar(size_t n) {
    if (n > MAX_REGISTROS || n > SIZE_MAX / sizeof(Registro)) return NULL;
    return malloc((n ? n : 1) * sizeof(Registro));
}

/* Funcao gerar: Cria registros sinteticos para o cenario e a semente informados. */
static void gerar(Registro *v, size_t n, Cenario c, uint64_t semente) {
    static const char *setores[] = {"AM", "PA", "RO", "RR", "AC", "AP", "TO", "MT", "MA"};
    estado_rng = semente ? semente : UINT64_C(1);
    for (size_t i = 0; i < n; ++i) {
        v[i].id = (uint32_t)(i + 1);
        v[i].instante_min = UINT64_C(29453760) + sorteio(260000);
        v[i].nuvens_pct = (uint8_t)sorteio(101);
        snprintf(v[i].setor, sizeof(v[i].setor), "%s", setores[sorteio(9)]);
    }
    if (c != ALEATORIO) {
        /* Define ordem total inicial usando Merge; esta etapa nao e cronometrada. */
        if (!merge_sort(v, n)) {
            fprintf(stderr, "Erro de memoria na geracao.\n");
            exit(EXIT_FAILURE);
        }
        if (c == INVERSO) {
            for (size_t i = 0; i < n / 2; ++i) trocar(&v[i], &v[n - 1 - i]);
        } else if (c == QUASE_ORDENADO && n > 1) {
            /* Perturba 5% do numero de registros por trocas aleatorias. */
            for (size_t i = 0; i < n / 20; ++i)
                trocar(&v[sorteio(n)], &v[sorteio(n)]);
        }
    }
}

/* Funcao ler_linha_csv: Interpreta e valida os campos de uma linha CSV de catalogo. */
static int ler_linha_csv(char *linha, Registro *r) {
    char resto = '\0', setor[SETOR_TAM];
    unsigned id, nuvens;
    uint64_t instante;
    int campos = sscanf(linha, " %u , %" SCNu64 " , %u , %15[A-Za-z0-9_] %c",
                        &id, &instante, &nuvens, setor, &resto);
    if (campos != 4 || id == 0 || nuvens > 100 || instante == 0) return 0;
    r->id = (uint32_t)id;
    r->instante_min = instante;
    r->nuvens_pct = (uint8_t)nuvens;
    strcpy(r->setor, setor);
    return 1;
}

/* Funcao ler_csv: Abre o CSV, le os registros e trata erros de arquivo ou formato. */
static int ler_csv(const char *caminho, Registro **saida, size_t *quantidade) {
    FILE *f = fopen(caminho, "r");
    if (!f) { perror("Nao foi possivel abrir CSV"); return 0; }
    Registro *v = alocar(MAX_REGISTROS);
    if (!v) { fclose(f); fprintf(stderr, "Memoria insuficiente.\n"); return 0; }
    char linha[MAX_LINHA];
    size_t n = 0, numero = 0;
    while (fgets(linha, sizeof(linha), f)) {
        ++numero;
        if (!strchr(linha, '\n') && !feof(f)) {
            fprintf(stderr, "Linha %zu longa demais.\n", numero);
            free(v); fclose(f); return 0;
        }
        if (numero == 1 && strncmp(linha, "id,", 3) == 0) continue;
        if (linha[0] == '\n' || linha[0] == '\r') continue;
        if (n == MAX_REGISTROS || !ler_linha_csv(linha, &v[n])) {
            fprintf(stderr, "Linha %zu invalida ou limite excedido.\n", numero);
            free(v); fclose(f); return 0;
        }
        ++n;
    }
    if (ferror(f)) { perror("Erro ao ler CSV"); free(v); fclose(f); return 0; }
    fclose(f);
    *saida = v;
    *quantidade = n;
    return 1;
}

/* Funcao gravar_csv: Grava em CSV os registros completos do vetor. */
static int gravar_csv(const char *caminho, const Registro *v, size_t n) {
    FILE *f = fopen(caminho, "w");
    if (!f) { perror("Nao foi possivel gravar CSV"); return 0; }
    fprintf(f, "id,instante_min,nuvens_pct,setor\n");
    for (size_t i = 0; i < n; ++i) {
        if (fprintf(f, "%" PRIu32 ",%" PRIu64 ",%u,%s\n", v[i].id,
                    v[i].instante_min, (unsigned)v[i].nuvens_pct, v[i].setor) < 0) {
            perror("Erro ao gravar dados"); fclose(f); return 0;
        }
    }
    if (fclose(f) != 0) { perror("Erro ao fechar CSV"); return 0; }
    return 1;
}

/* Funcao visualizar: Apresenta no terminal uma amostra de registros. */
static void visualizar(const Registro *v, size_t n, const char *titulo) {
    printf("\n%s (%zu registros; amostra ate 8):\n", titulo, n);
    puts("ID        INSTANTE_MIN       NUVENS    SETOR");
    for (size_t i = 0; i < n && i < 8; ++i)
        printf("%-9" PRIu32 " %-19" PRIu64 " %-9u %s\n", v[i].id,
               v[i].instante_min, (unsigned)v[i].nuvens_pct, v[i].setor);
    if (n > 8) puts("... Lista completa disponivel nos arquivos CSV.");
}

/* CPU time: cronometro inicia imediatamente antes de executar algoritmo.
 * Arquivos, geracao, copia, listagem e verificacao ocorrem fora da janela.
 */
/* Funcao medir: Cronometra somente a ordenacao e recolhe numero de comparacoes. */
static int medir(Algoritmo a, Registro *v, size_t n, double *ms, uint64_t *ops) {
    comparacoes = 0;
    clock_t inicio = clock();
    if (inicio == (clock_t)-1) return 0;
    int ok = executar(a, v, n);
    clock_t fim = clock();
    if (!ok || fim == (clock_t)-1) return 0;
    *ms = 1000.0 * (double)(fim - inicio) / CLOCKS_PER_SEC;
    *ops = comparacoes;
    return 1;
}

/* Funcao comparar_todos: Executa as quatro tecnicas sobre copias e verifica os resultados. */
static int comparar_todos(const Registro *dados, size_t n, int exportar) {
    Registro *base = alocar(n), *ref = alocar(n);
    if (!base || !ref) { free(base); free(ref); return 0; }
    if (n) memcpy(ref, dados, n * sizeof(*ref));
    if (!merge_sort(ref, n)) { free(base); free(ref); return 0; }
    visualizar(dados, n, "Antes da ordenacao");
    if (exportar && !gravar_csv("antes_ordenacao.csv", dados, n)) {
        free(base); free(ref); return 0;
    }
    puts("\nAlgoritmo          Tempo (ms)       Comparacoes       Correto");
    for (int i = 0; i < QT_ALGORITMOS; ++i) {
        if (n) memcpy(base, dados, n * sizeof(*base));
        double ms;
        uint64_t ops;
        if (!medir((Algoritmo)i, base, n, &ms, &ops)) {
            fprintf(stderr, "Falha ao executar %s.\n", nome_alg((Algoritmo)i));
            free(base); free(ref); return 0;
        }
        int certo = validacao_ordenacao(base, n) && validar_igualdade(base, ref, n);
        printf("%-18s %11.3f %18" PRIu64 "       %s\n", nome_alg((Algoritmo)i),
               ms, ops, certo ? "SIM" : "NAO");
        if (!certo) { free(base); free(ref); return 0; }
        if (exportar) {
            char nome[64];
            snprintf(nome, sizeof(nome), "depois_%s.csv", nome_alg((Algoritmo)i));
            if (!gravar_csv(nome, base, n)) { free(base); free(ref); return 0; }
        }
        if (i == 0) visualizar(base, n, "Depois da ordenacao");
    }
    free(base); free(ref);
    return 1;
}

/* Funcao benchmark: Executa os quatro cenarios e tamanhos, exportando 192 medidas. */
static int benchmark(const char *arquivo) {
    const size_t tamanhos[] = {1000, 5000, 20000, 100000};
    FILE *f = fopen(arquivo, "w");
    if (!f) { perror("Falha ao criar arquivo de resultados"); return 0; }
    fprintf(f, "cenario,tamanho,algoritmo,repeticao,tempo_ms,comparacoes,correto\n");
    for (size_t ti = 0; ti < sizeof(tamanhos) / sizeof(tamanhos[0]); ++ti) {
        size_t n = tamanhos[ti];
        Registro *original = alocar(n), *trabalho = alocar(n), *referencia = alocar(n);
        if (!original || !trabalho || !referencia) {
            fprintf(stderr, "Memoria insuficiente para %zu registros.\n", n);
            free(original); free(trabalho); free(referencia); fclose(f); return 0;
        }
        for (int ci = ALEATORIO; ci <= QUASE_ORDENADO; ++ci) {
            Cenario c = (Cenario)ci;
            gerar(original, n, c, UINT64_C(20261026) + (uint64_t)n * 13 + (uint64_t)ci);
            memcpy(referencia, original, n * sizeof(*original));
            if (!merge_sort(referencia, n)) {
                free(original); free(trabalho); free(referencia); fclose(f); return 0;
            }
            for (int ai = INSERCAO; ai < QT_ALGORITMOS; ++ai) {
                for (int repeticao = 1; repeticao <= REPETICOES; ++repeticao) {
                    memcpy(trabalho, original, n * sizeof(*original));
                    double ms;
                    uint64_t ops;
                    int ok = medir((Algoritmo)ai, trabalho, n, &ms, &ops);
                    ok = ok && validacao_ordenacao(trabalho, n) &&
                         validar_igualdade(trabalho, referencia, n);
                    if (!ok) {
                        fprintf(stderr, "Teste falhou: %s, %s, %zu, repeticao %d\n",
                                nome_cenario(c), nome_alg((Algoritmo)ai), n, repeticao);
                        free(original); free(trabalho); free(referencia); fclose(f); return 0;
                    }
                    fprintf(f, "%s,%zu,%s,%d,%.6f,%" PRIu64 ",SIM\n",
                            nome_cenario(c), n, nome_alg((Algoritmo)ai), repeticao, ms, ops);
                }
                printf("OK: %-9s n=%-6zu %-10s (%d repeticoes)\n", nome_cenario(c), n,
                       nome_alg((Algoritmo)ai), REPETICOES);
                fflush(stdout);
            }
        }
        free(original); free(trabalho); free(referencia);
    }
    if (fclose(f) != 0) { perror("Erro ao fechar benchmark"); return 0; }
    printf("Resultados salvos em %s\n", arquivo);
    return 1;
}

/* Funcao teste_automatico: Executa os 96 casos de autoteste de ordenacao. */
static int teste_automatico(void) {
    const size_t tamanhos[] = {0, 1, 2, 7, 33, 1024};
    for (size_t ti = 0; ti < sizeof(tamanhos) / sizeof(tamanhos[0]); ++ti) {
        size_t n = tamanhos[ti];
        Registro *v = alocar(n), *t = alocar(n), *r = alocar(n);
        if (!v || !t || !r) { free(v); free(t); free(r); return 0; }
        for (int ci = ALEATORIO; ci <= QUASE_ORDENADO; ++ci) {
            gerar(v, n, (Cenario)ci, 12345);
            if (n) memcpy(r, v, n * sizeof(*v));
            if (!merge_sort(r, n)) { free(v); free(t); free(r); return 0; }
            for (int ai = INSERCAO; ai < QT_ALGORITMOS; ++ai) {
                if (n) memcpy(t, v, n * sizeof(*v));
                comparacoes = 0;
                if (!executar((Algoritmo)ai, t, n) ||
                    !validacao_ordenacao(t, n) || !validar_igualdade(t, r, n)) {
                    fprintf(stderr, "Autoteste falhou em n=%zu, cenario=%d, algoritmo=%d.\n",
                            n, ci, ai);
                    free(v); free(t); free(r); return 0;
                }
            }
        }
        free(v); free(t); free(r);
    }
    puts("Autoteste aprovado: 96 combinacoes (incluindo vetor vazio e unitario).");
    return 1;
}

/* Funcao obter_inteiro: Converte argumento textual em numero inteiro sem sinal valido. */
static int obter_inteiro(const char *s, uint64_t *valor) {
    if (!s || !*s || *s == '-') return 0;
    errno = 0;
    char *fim;
    unsigned long long v = strtoull(s, &fim, 10);
    if (errno || fim == s || *fim != '\0') return 0;
    *valor = (uint64_t)v;
    return 1;
}

/* Funcao obter_cenario: Converte o argumento textual para um cenario conhecido. */
static int obter_cenario(const char *s, Cenario *c) {
    for (int i = ALEATORIO; i <= QUASE_ORDENADO; ++i) {
        if (strcmp(s, nome_cenario((Cenario)i)) == 0) {
            *c = (Cenario)i; return 1;
        }
    }
    return 0;
}

/* Funcao ajuda: Exibe os comandos aceitos pelo executavel de linha de comando. */
static void ajuda(const char *exe) {
    printf("Uso:\n  %s --selftest\n  %s --bench resultados.csv\n", exe, exe);
    printf("  %s --generate N CENARIO SEMENTE arquivo.csv\n", exe);
    printf("  %s --analyze arquivo.csv\n  %s --manual\n", exe, exe);
    puts("Cenarios: aleatorio, ordenado, inverso, quase. Limite: 150000 registros.");
    puts("CSV: id,instante_min,nuvens_pct,setor (sem aspas e sem separador interno).");
    puts("Arquivos --analyze: antes_ordenacao.csv e depois_*.csv no diretorio atual.");
}

/* Funcao entrada_manual: Coleta registros pelo terminal e os submete aos algoritmos. */
static int entrada_manual(void) {
    char linha[256];
    uint64_t quantidade;
    printf("Quantidade de registros (1-100): ");
    if (!fgets(linha, sizeof(linha), stdin)) return 0;
    linha[strcspn(linha, "\r\n")] = '\0';
    if (!obter_inteiro(linha, &quantidade) || quantidade < 1 || quantidade > 100) {
        fprintf(stderr, "Quantidade invalida.\n"); return 0;
    }
    Registro *v = alocar((size_t)quantidade);
    if (!v) return 0;
    for (size_t i = 0; i < (size_t)quantidade; ++i) {
        printf("Registro %zu no formato id,instante_min,nuvens_pct,setor: ", i + 1);
        if (!fgets(linha, sizeof(linha), stdin) || !ler_linha_csv(linha, &v[i])) {
            fprintf(stderr, "Registro invalido.\n"); free(v); return 0;
        }
    }
    int ok = comparar_todos(v, (size_t)quantidade, 1);
    free(v);
    return ok;
}

/* Funcao main: Seleciona a operacao de linha de comando e retorna seu resultado. */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
        return teste_automatico() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 3 && strcmp(argv[1], "--bench") == 0)
        return benchmark(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 3 && strcmp(argv[1], "--analyze") == 0) {
        Registro *v = NULL;
        size_t n = 0;
        if (!ler_csv(argv[2], &v, &n)) return EXIT_FAILURE;
        int ok = comparar_todos(v, n, 1);
        free(v);
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (argc == 2 && strcmp(argv[1], "--manual") == 0)
        return entrada_manual() ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc == 6 && strcmp(argv[1], "--generate") == 0) {
        uint64_t n, semente;
        Cenario c;
        if (!obter_inteiro(argv[2], &n) || n < 1 || n > MAX_REGISTROS ||
            !obter_cenario(argv[3], &c) || !obter_inteiro(argv[4], &semente)) {
            ajuda(argv[0]); return EXIT_FAILURE;
        }
        Registro *v = alocar((size_t)n);
        if (!v) return EXIT_FAILURE;
        gerar(v, (size_t)n, c, semente);
        int ok = gravar_csv(argv[5], v, (size_t)n);
        free(v);
        if (ok) printf("Gerados %" PRIu64 " registros em %s.\n", n, argv[5]);
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    ajuda(argv[0]);
    return argc == 1 || (argc == 2 && strcmp(argv[1], "--help") == 0)
               ? EXIT_SUCCESS : EXIT_FAILURE;
}
