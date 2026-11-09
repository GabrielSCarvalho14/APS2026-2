/*
 * APS - Interface grafica Windows, inteiramente em C11 + Win32.
 *
 * Preserva aps_ordenacao.c, incluindo algoritmos, seeds e cronometro.
 * Todas as tarefas sao executadas por aps_ordenacao.exe em um processo filho.
 * O GUI apenas aciona comandos, mostra logs e interpreta os arquivos CSV.
 */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#include <math.h>

#define APP_W 1280
#define APP_H 790
#define BUF_COMMAND 4096
#define WM_LOG_APPEND (WM_APP + 1)
#define WM_TASK_DONE  (WM_APP + 2)

enum {
    BTN_GENERATE = 101, BTN_OPEN_CSV, BTN_MANUAL, BTN_SELFTEST,
    BTN_BENCHMARK, BTN_OPEN_RESULTS, EDIT_QUANTITY = 201, EDIT_SEED,
    COMBO_SCENARIO, COMBO_CHART, EDIT_LOG, EDIT_MANUAL = 301,
    BTN_MANUAL_RUN
};
enum { TASK_GENERATE = 1, TASK_ANALYZE, TASK_SELFTEST, TASK_BENCHMARK };

static const wchar_t *SCENARIO_UI[] = {
    L"Aleatorio", L"Ordenado", L"Inverso", L"Quase ordenado"
};
static const wchar_t *SCENARIO_CLI[] = {
    L"aleatorio", L"ordenado", L"inverso", L"quase"
};
static const char *SCENARIO_CSV[] = {
    "aleatorio", "ordenado", "inverso", "quase"
};
static const char *ALGORITHM_CSV[] = {
    "Insertion", "Merge", "Quick", "Heap"
};
static const wchar_t *ALGORITHM_UI[] = {
    L"Insertion", L"Merge", L"Quick", L"Heap"
};
static const int SIZES[] = {1000, 5000, 20000, 100000};
static const COLORREF COLORS[] = {
    RGB(49, 81, 157), RGB(13, 139, 128), RGB(203, 100, 51), RGB(118, 76, 160)
};

typedef struct {
    int type;
    wchar_t argument[MAX_PATH * 2];
    wchar_t result_path[MAX_PATH * 2];
    unsigned quantity;
    unsigned seed;
    int scenario;
} Task;

typedef struct {
    int type;
    int success;
    wchar_t path[MAX_PATH * 2];
} TaskResult;

static HINSTANCE g_instance;
static HWND g_main;
static HWND g_log;
static HWND g_qty;
static HWND g_seed;
static HWND g_scenario;
static HWND g_chart_combo;
static HWND g_buttons[6];
static HWND g_manual_window;
static HWND g_manual_edit;
static HFONT g_font_small, g_font_body, g_font_bold, g_font_title, g_font_stat;
static HFONT g_font_log;
static HBRUSH g_bg_brush, g_white_brush;
static wchar_t g_base_dir[MAX_PATH * 2];
static wchar_t g_output_dir[MAX_PATH * 2];
static wchar_t g_core_exe[MAX_PATH * 2];
static wchar_t g_result_csv[MAX_PATH * 2];
static wchar_t g_status[256] = L"Pronto para demonstrar o sistema.";
static volatile LONG g_busy = 0;
static double g_times[4][4][4][3];
static int g_counts[4][4][4];
static int g_has_results = 0;
static int g_measurements = 0;

static COLORREF C_NAV = RGB(19, 34, 59);
static COLORREF C_BG = RGB(244, 247, 251);
static COLORREF C_TEXT = RGB(30, 47, 72);
static COLORREF C_MUTED = RGB(105, 120, 140);
static COLORREF C_BLUE = RGB(37, 99, 219);
static COLORREF C_LINE = RGB(222, 229, 237);

/* Funcao make_timestamp: Gera marca temporal para nomes de novos arquivos de saida. */
static void make_timestamp(wchar_t *out, size_t capacity) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(out, capacity, L"%04u%02u%02u_%02u%02u%02u_%03u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
             st.wSecond, st.wMilliseconds);
}

/* Funcao make_path: Combina diretorio base e nome para formar um caminho Windows. */
static void make_path(wchar_t *out, size_t capacity,
                      const wchar_t *directory, const wchar_t *filename) {
    swprintf(out, capacity, L"%ls\\%ls", directory, filename);
}

/* Funcao basename_of: Obtem o nome final do arquivo dentro de um caminho. */
static const wchar_t *basename_of(const wchar_t *path) {
    const wchar_t *p = wcsrchr(path, L'\\');
    const wchar_t *q = wcsrchr(path, L'/');
    if (!p || (q && q > p)) p = q;
    return p ? p + 1 : path;
}

/* Funcao exists_file: Confere se determinado arquivo existe no disco. */
static int exists_file(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/* Funcao set_status: Atualiza a mensagem de estado exibida na interface. */
static void set_status(const wchar_t *message) {
    wcsncpy(g_status, message, 255);
    g_status[255] = L'\0';
    if (g_main) InvalidateRect(g_main, NULL, FALSE);
}

/* Funcao style_window: Aplica ao controle a identidade visual configurada. */
static void style_window(HWND hwnd) {
    if (hwnd) SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font_body, TRUE);
}

/* Funcao create_control: Cria controles Win32 com fonte, identificador e dimensoes. */
static HWND create_control(DWORD ex_style, const wchar_t *cls,
                           const wchar_t *text, DWORD style,
                           int x, int y, int w, int h,
                           HWND parent, int id) {
    HWND ctl = CreateWindowExW(ex_style, cls, text, style,
                              x, y, w, h, parent,
                              (HMENU)(INT_PTR)id, g_instance, NULL);
    style_window(ctl);
    return ctl;
}

/* Funcao find_scenario: Encontra o indice de um cenario no CSV de benchmark. */
static int find_scenario(const char *value) {
    for (int i = 0; i < 4; ++i)
        if (strcmp(value, SCENARIO_CSV[i]) == 0) return i;
    return -1;
}

/* Funcao find_algorithm: Encontra o indice correspondente ao nome do algoritmo. */
static int find_algorithm(const char *value) {
    for (int i = 0; i < 4; ++i)
        if (strcmp(value, ALGORITHM_CSV[i]) == 0) return i;
    return -1;
}

/* Funcao find_size: Mapeia uma quantidade de registros para o grafico. */
static int find_size(int n) {
    for (int i = 0; i < 4; ++i)
        if (SIZES[i] == n) return i;
    return -1;
}

/* Funcao median_of: Calcula a mediana das repeticoes validas de uma serie. */
static double median_of(const double *values, int count) {
    double sorted[3] = {0, 0, 0};
    if (!count) return 0;
    if (count > 3) count = 3;
    for (int i = 0; i < count; ++i) sorted[i] = values[i];
    for (int i = 0; i < count; ++i)
        for (int j = i + 1; j < count; ++j)
            if (sorted[j] < sorted[i]) {
                double temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
    if (count % 2) return sorted[count / 2];
    return (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0;
}

/* Funcao load_benchmark: Le as medicoes de benchmark e alimenta os dados do grafico. */
static int load_benchmark(const wchar_t *path) {
    FILE *file = _wfopen(path, L"rb");
    if (!file) return 0;
    double times[4][4][4][3] = {0};
    int counts[4][4][4] = {0};
    char line[512];
    int valid = 0;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return 0;
    }
    if (strstr(line, "cenario,tamanho,algoritmo,repeticao,tempo_ms") == NULL) {
        fclose(file);
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        char scen[32], alg[32], correct[16];
        int size, repetition;
        double ms;
        unsigned long long comparisons;
        if (sscanf(line, "%31[^,],%d,%31[^,],%d,%lf,%llu,%15s",
                   scen, &size, alg, &repetition, &ms,
                   &comparisons, correct) != 7) continue;
        int s = find_scenario(scen);
        int n = find_size(size);
        int a = find_algorithm(alg);
        if (s < 0 || n < 0 || a < 0 || repetition < 1 ||
            repetition > 3 || ms < 0 || !isfinite(ms) ||
            strncmp(correct, "SIM", 3) != 0) continue;
        int r = counts[s][n][a];
        if (r < 3) {
            times[s][n][a][r] = ms;
            counts[s][n][a] = r + 1;
            ++valid;
        }
    }
    fclose(file);
    if (!valid) return 0;
    memcpy(g_times, times, sizeof(g_times));
    memcpy(g_counts, counts, sizeof(g_counts));
    wcsncpy(g_result_csv, path, (MAX_PATH * 2) - 1);
    g_result_csv[(MAX_PATH * 2) - 1] = L'\0';
    g_has_results = 1;
    g_measurements = valid;
    if (g_main) InvalidateRect(g_main, NULL, FALSE);
    return valid;
}

/* Funcao append_to_log: Acrescenta linhas legiveis ao painel de eventos da interface. */
static void append_to_log(const wchar_t *text) {
    if (!g_log) return;
    int length = GetWindowTextLengthW(g_log);
    if (length > 28000) SetWindowTextW(g_log, L"(Inicio do log omitido)\r\n");
    SendMessageW(g_log, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(g_log, EM_SCROLLCARET, 0, 0);
}

/* Funcao post_utf8_log: Converte texto UTF-8 do nucleo e envia ao controle de log. */
static void post_utf8_log(const char *text, DWORD length) {
    if (!length) return;
    int total = MultiByteToWideChar(CP_UTF8, 0, text, (int)length, NULL, 0);
    if (total <= 0) return;
    wchar_t *message = (wchar_t *)calloc((size_t)total * 2 + 2, sizeof(wchar_t));
    if (!message) return;
    wchar_t *wide = message + total + 1;
    if (!MultiByteToWideChar(CP_UTF8, 0, text, (int)length, wide, total)) {
        free(message);
        return;
    }
    int j = 0;
    for (int i = 0; i < total; ++i) {
        if (wide[i] == L'\n' && (i == 0 || wide[i - 1] != L'\r'))
            message[j++] = L'\r';
        message[j++] = wide[i];
    }
    message[j] = L'\0';
    if (!PostMessageW(g_main, WM_LOG_APPEND, 0, (LPARAM)message)) free(message);
}

/* Funcao run_core_command: Executa o nucleo C e captura sua saida para a interface. */
static int run_core_command(const wchar_t *parameters) {
    wchar_t command[BUF_COMMAND];
    swprintf(command, BUF_COMMAND, L"\"%ls\" %ls", g_core_exe, parameters);
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE read_pipe = NULL, write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0)) return 0;
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE null_input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    startup.hStdInput = null_input != INVALID_HANDLE_VALUE ? null_input : NULL;
    BOOL launched = CreateProcessW(
        g_core_exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
        NULL, g_output_dir, &startup, &process);
    CloseHandle(write_pipe);
    if (null_input != INVALID_HANDLE_VALUE) CloseHandle(null_input);
    if (!launched) {
        CloseHandle(read_pipe);
        const char *msg = "Nao foi possivel iniciar aps_ordenacao.exe.\n";
        post_utf8_log(msg, (DWORD)strlen(msg));
        return 0;
    }
    char buffer[2048];
    DWORD received;
    while (ReadFile(read_pipe, buffer, (DWORD)sizeof(buffer), &received, NULL) &&
           received > 0) {
        post_utf8_log(buffer, received);
    }
    CloseHandle(read_pipe);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code == 0;
}

/* Funcao run_task: Executa uma tarefa em segundo plano e informa sua conclusao. */
static DWORD WINAPI run_task(LPVOID context) {
    Task *task = (Task *)context;
    TaskResult *out = (TaskResult *)calloc(1, sizeof(TaskResult));
    if (!out) {
        InterlockedExchange(&g_busy, 0);
        free(task);
        return 1;
    }
    out->type = task->type;
    wchar_t args[BUF_COMMAND];
    int success = 0;
    if (task->type == TASK_GENERATE) {
        swprintf(args, BUF_COMMAND, L"--generate %u %ls %u \"%ls\"",
                 task->quantity, SCENARIO_CLI[task->scenario],
                 task->seed, task->argument);
        success = run_core_command(args);
        if (success) {
            const char *note = "\nAnalisando o catalogo gerado...\n";
            post_utf8_log(note, (DWORD)strlen(note));
            swprintf(args, BUF_COMMAND, L"--analyze \"%ls\"", task->argument);
            success = run_core_command(args);
        }
    } else if (task->type == TASK_ANALYZE) {
        swprintf(args, BUF_COMMAND, L"--analyze \"%ls\"", task->argument);
        success = run_core_command(args);
    } else if (task->type == TASK_SELFTEST) {
        success = run_core_command(L"--selftest");
    } else if (task->type == TASK_BENCHMARK) {
        swprintf(args, BUF_COMMAND, L"--bench \"%ls\"", task->argument);
        success = run_core_command(args);
        if (success) {
            wcsncpy(out->path, task->argument, (MAX_PATH * 2) - 1);
        }
    }
    out->success = success;
    if (!PostMessageW(g_main, WM_TASK_DONE, 0, (LPARAM)out)) {
        InterlockedExchange(&g_busy, 0);
        free(out);
    }
    free(task);
    return 0;
}

/* Funcao enable_actions: Habilita ou bloqueia botoes conforme a tarefa em andamento. */
static void enable_actions(int enabled) {
    for (int i = 0; i < 6; ++i) EnableWindow(g_buttons[i], enabled);
    EnableWindow(g_qty, enabled);
    EnableWindow(g_seed, enabled);
    EnableWindow(g_scenario, enabled);
}

/* Funcao start_task: Prepara uma tarefa e inicia sua thread de processamento. */
static int start_task(Task *task) {
    if (!task) return 0;
    if (!exists_file(g_core_exe)) {
        MessageBoxW(g_main, L"Primeiro execute build_gui.bat para gerar aps_ordenacao.exe.",
                    L"Programa principal nao encontrado", MB_ICONERROR);
        free(task);
        return 0;
    }
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) {
        MessageBoxW(g_main, L"Aguarde a tarefa atual terminar.",
                    L"Processamento em andamento", MB_ICONINFORMATION);
        free(task);
        return 0;
    }
    SetWindowTextW(g_log, L"");
    append_to_log(L"Iniciando comando no programa original em C...\r\n\r\n");
    enable_actions(0);
    set_status(L"Executando a tarefa... Os resultados aparecerao no painel de logs.");
    HANDLE thread = CreateThread(NULL, 0, run_task, task, 0, NULL);
    if (!thread) {
        enable_actions(1);
        InterlockedExchange(&g_busy, 0);
        free(task);
        set_status(L"Falha ao iniciar a tarefa.");
        return 0;
    }
    CloseHandle(thread);
    return 1;
}

/* Funcao begin_generate: Le os parametros e inicia geracao e analise do catalogo. */
static void begin_generate(void) {
    wchar_t qty_text[32], seed_text[32], *end;
    GetWindowTextW(g_qty, qty_text, 32);
    GetWindowTextW(g_seed, seed_text, 32);
    unsigned long qty = wcstoul(qty_text, &end, 10);
    if (*end || qty < 1 || qty > 150000) {
        MessageBoxW(g_main, L"Informe de 1 a 150000 registros.", L"Quantidade invalida",
                    MB_ICONWARNING);
        return;
    }
    unsigned long seed = wcstoul(seed_text, &end, 10);
    if (*end || !seed_text[0]) {
        MessageBoxW(g_main, L"A semente deve ser um inteiro nao negativo.",
                    L"Semente invalida", MB_ICONWARNING);
        return;
    }
    int scenario = (int)SendMessageW(g_scenario, CB_GETCURSEL, 0, 0);
    if (scenario < 0 || scenario > 3) scenario = 0;
    Task *task = (Task *)calloc(1, sizeof(Task));
    if (!task) return;
    task->type = TASK_GENERATE;
    task->quantity = (unsigned)qty;
    task->seed = (unsigned)seed;
    task->scenario = scenario;
    wchar_t stamp[50], filename[110];
    make_timestamp(stamp, 50);
    swprintf(filename, 110, L"catalogo_%ls.csv", stamp);
    make_path(task->argument, MAX_PATH * 2, g_output_dir, filename);
    start_task(task);
}

/* Funcao choose_csv: Abre a caixa do Windows para selecionar um arquivo CSV. */
static int choose_csv(wchar_t *output, size_t capacity, int for_results) {
    wchar_t filename[MAX_PATH * 2] = L"";
    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"Arquivos CSV (*.csv)\0*.csv\0Todos os arquivos (*.*)\0*.*\0\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH * 2;
    dialog.lpstrInitialDir = g_base_dir;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    dialog.lpstrTitle = for_results ?
        L"Escolher um arquivo com medições de benchmark" :
        L"Escolher um catálogo CSV para ordenar";
    if (!GetOpenFileNameW(&dialog)) return 0;
    wcsncpy(output, filename, capacity - 1);
    output[capacity - 1] = L'\0';
    return 1;
}

/* Funcao begin_analyze: Inicia a analise de um catalogo CSV escolhido pelo usuario. */
static void begin_analyze(void) {
    wchar_t file[MAX_PATH * 2];
    if (!choose_csv(file, MAX_PATH * 2, 0)) return;
    Task *task = (Task *)calloc(1, sizeof(Task));
    if (!task) return;
    task->type = TASK_ANALYZE;
    wcsncpy(task->argument, file, (MAX_PATH * 2) - 1);
    start_task(task);
}

/* Funcao begin_selftest: Aciona os 96 autotestes do executavel original. */
static void begin_selftest(void) {
    Task *task = (Task *)calloc(1, sizeof(Task));
    if (!task) return;
    task->type = TASK_SELFTEST;
    start_task(task);
}

/* Funcao begin_benchmark: Solicita confirmacao e executa a bateria de 192 medicoes. */
static void begin_benchmark(void) {
    if (MessageBoxW(g_main,
        L"O benchmark inclui 100 mil registros e pode demorar, especialmente com Insertion Sort.\n\n"
        L"Os resultados serao gravados em um NOVO CSV, sem substituir o original. Continuar?",
        L"Confirmar benchmark", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    Task *task = (Task *)calloc(1, sizeof(Task));
    if (!task) return;
    task->type = TASK_BENCHMARK;
    wchar_t stamp[50], filename[110];
    make_timestamp(stamp, 50);
    swprintf(filename, 110, L"benchmark_%ls.csv", stamp);
    make_path(task->argument, MAX_PATH * 2, g_output_dir, filename);
    start_task(task);
}

/* Funcao open_results: Carrega um CSV de benchmark e atualiza curvas do grafico. */
static void open_results(void) {
    wchar_t file[MAX_PATH * 2];
    if (!choose_csv(file, MAX_PATH * 2, 1)) return;
    int same_file = _wcsicmp(file, g_result_csv) == 0;
    int rows = load_benchmark(file);
    if (!rows) {
        MessageBoxW(g_main,
            L"Esse arquivo nao contem medicoes validas de benchmark.\n\n"
            L"Para atualizar o grafico, abra resultados_brutos.csv ou um "
            L"benchmark_*.csv gerado pelo botao Benchmark.\n\n"
            L"Arquivos de catalogo e depois_*.csv mostram registros, "
            L"mas nao contem as series de desempenho deste grafico.",
            L"CSV de benchmark necessario", MB_ICONWARNING);
        return;
    }
    wchar_t notice[640];
    if (same_file) {
        swprintf(notice, 640,
                 L"O arquivo %ls ja estava sendo exibido. "
                 L"As curvas permanecem iguais.", basename_of(file));
    } else {
        swprintf(notice, 640,
                 L"Grafico atualizado: %d medicoes carregadas de %ls.",
                 rows, basename_of(file));
    }
    set_status(notice);
    append_to_log(L"\r\n[RESULTADOS] ");
    append_to_log(notice);
    append_to_log(L"\r\n");
    RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    MessageBoxW(g_main, notice, L"Resultados do benchmark", MB_OK | MB_ICONINFORMATION);
}

/* Funcao write_manual_csv: Valida linhas digitadas e salva o catalogo manual em CSV. */
static int write_manual_csv(HWND parent, const wchar_t *body, wchar_t *file,
                            size_t capacity) {
    if (!body || !*body) return 0;
    wchar_t stamp[50], filename[110];
    make_timestamp(stamp, 50);
    swprintf(filename, 110, L"manual_%ls.csv", stamp);
    make_path(file, capacity, g_output_dir, filename);
    FILE *output = _wfopen(file, L"wb");
    if (!output) {
        MessageBoxW(parent, L"Nao foi possivel criar o arquivo CSV.", L"Erro",
                    MB_ICONERROR);
        return 0;
    }
    const char *header = "id,instante_min,nuvens_pct,setor\n";
    fputs(header, output);
    int length = WideCharToMultiByte(CP_UTF8, 0, body, -1, NULL, 0, NULL, NULL);
    char *utf8 = length > 0 ? (char *)malloc((size_t)length) : NULL;
    if (!utf8) {
        fclose(output);
        return 0;
    }
    WideCharToMultiByte(CP_UTF8, 0, body, -1, utf8, length, NULL, NULL);
    const char *p = utf8;
    int records = 0;
    while (*p) {
        while (*p == '\r' || *p == '\n') ++p;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '\r' && *p != '\n') ++p;
        int commas = 0;
        for (const char *ch = start; ch < p; ++ch) if (*ch == ',') ++commas;
        if (commas != 3 || records >= 100) {
            fclose(output);
            free(utf8);
            DeleteFileW(file);
            MessageBoxW(parent, L"Digite de 1 a 100 registros, um por linha, "
                        L"com quatro campos separados por virgulas.",
                        L"Formato invalido", MB_ICONWARNING);
            return 0;
        }
        fwrite(start, 1, (size_t)(p - start), output);
        fputc('\n', output);
        ++records;
    }
    fclose(output);
    free(utf8);
    if (!records) {
        DeleteFileW(file);
        return 0;
    }
    return 1;
}

/* Funcao manual_proc: Processa os eventos da janela de entrada manual. */
static LRESULT CALLBACK manual_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    if (message == WM_CREATE) {
        create_control(0, L"STATIC",
            L"Digite um registro por linha: id,instante_min,nuvens_pct,setor",
            WS_CHILD | WS_VISIBLE, 20, 20, 510, 26, window, 0);
        g_manual_edit = create_control(WS_EX_CLIENTEDGE, L"EDIT",
            L"1,29667360,79,MA\r\n2,29642495,25,PA\r\n3,29710322,3,AC",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT |
            ES_MULTILINE | WS_VSCROLL | ES_AUTOVSCROLL,
            20, 56, 530, 190, window, EDIT_MANUAL);
        create_control(0, L"BUTTON", L"Executar ordenacao",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            350, 258, 200, 36, window, BTN_MANUAL_RUN);
        return 0;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == BTN_MANUAL_RUN) {
        int len = GetWindowTextLengthW(g_manual_edit);
        if (len <= 0 || len > 20000) return 0;
        wchar_t *body = (wchar_t *)calloc((size_t)len + 1, sizeof(wchar_t));
        if (!body) return 0;
        GetWindowTextW(g_manual_edit, body, len + 1);
        wchar_t file[MAX_PATH * 2];
        int okay = write_manual_csv(window, body, file, MAX_PATH * 2);
        free(body);
        if (okay) {
            DestroyWindow(window);
            Task *task = (Task *)calloc(1, sizeof(Task));
            if (task) {
                task->type = TASK_ANALYZE;
                wcsncpy(task->argument, file, (MAX_PATH * 2) - 1);
                start_task(task);
            }
        }
        return 0;
    }
    if (message == WM_DESTROY) {
        g_manual_window = NULL;
        g_manual_edit = NULL;
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/* Funcao begin_manual: Abre o formulario de insercao manual de registros. */
static void begin_manual(void) {
    if (g_manual_window) {
        SetForegroundWindow(g_manual_window);
        return;
    }
    g_manual_window = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"APSManualWindow", L"Entrada manual - APS",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 585, 350,
        g_main, NULL, g_instance, NULL);
}

/* Funcao draw_text: Desenha texto no painel grafico do Windows. */
static void draw_text(HDC dc, const wchar_t *text, int x, int y,
                      HFONT font, COLORREF color) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text, (int)wcslen(text));
    SelectObject(dc, old);
}

/* Funcao draw_panel: Desenha um painel visual retangular na interface. */
static void draw_panel(HDC dc, int x, int y, int w, int h,
                       COLORREF fill, COLORREF border, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ prev_brush = SelectObject(dc, brush);
    HGDIOBJ prev_pen = SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, radius, radius);
    SelectObject(dc, prev_brush);
    SelectObject(dc, prev_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

/* Funcao draw_chart: Desenha as medianas de desempenho por tamanho e algoritmo. */
static void draw_chart(HDC dc) {
    int scenario = (int)SendMessageW(g_chart_combo, CB_GETCURSEL, 0, 0);
    if (scenario < 0 || scenario > 3) scenario = 0;
    draw_text(dc, L"Comparacao de desempenho", 266, 393, g_font_bold, C_TEXT);
    draw_text(dc, L"Mediana de tempo de CPU por tamanho | log (zero no piso visual)",
              266, 421, g_font_small, C_MUTED);
    if (!g_has_results) {
        draw_text(dc, L"Sem resultados. Abra um CSV de benchmark.",
                  285, 520, g_font_body, C_MUTED);
        return;
    }
    int x0 = 315, y0 = 472, width = 550, height = 165;
    double lo = 1e30, hi = 0;
    for (int a = 0; a < 4; ++a)
        for (int n = 0; n < 4; ++n)
            if (g_counts[scenario][n][a] > 0) {
                double v = median_of(g_times[scenario][n][a], g_counts[scenario][n][a]);
                if (v <= 0.00001) v = 0.001; /* zero: abaixo da resolucao */
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
    if (hi <= 0 || lo == 1e30) {
        draw_text(dc, L"Este cenario nao esta presente no CSV.", 285, 520, g_font_body, C_MUTED);
        return;
    }
    double low_exp = floor(log10(lo)) - 0.25;
    double high_exp = ceil(log10(hi)) + 0.25;
    if (high_exp <= low_exp) high_exp = low_exp + 1;
    for (int i = 0; i <= 4; ++i) {
        int y = y0 + i * height / 4;
        HPEN pen = CreatePen(PS_SOLID, 1, C_LINE);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, x0, y, NULL);
        LineTo(dc, x0 + width, y);
        SelectObject(dc, old);
        DeleteObject(pen);
        wchar_t label[40];
        double exponent = high_exp - i * (high_exp - low_exp) / 4.0;
        swprintf(label, 40, L"%.3g", pow(10.0, exponent));
        draw_text(dc, label, 264, y - 8, g_font_small, C_MUTED);
    }
    for (int n = 0; n < 4; ++n) {
        int x = x0 + n * width / 3;
        wchar_t label[35];
        if (SIZES[n] == 1000) swprintf(label, 35, L"1 mil");
        else if (SIZES[n] == 5000) swprintf(label, 35, L"5 mil");
        else if (SIZES[n] == 20000) swprintf(label, 35, L"20 mil");
        else swprintf(label, 35, L"100 mil");
        draw_text(dc, label, x - 18, 647, g_font_small, C_MUTED);
    }
    for (int a = 0; a < 4; ++a) {
        HPEN pen = CreatePen(PS_SOLID, 3, COLORS[a]);
        HGDIOBJ old = SelectObject(dc, pen);
        int previous_x = -1, previous_y = -1;
        for (int n = 0; n < 4; ++n) {
            if (!g_counts[scenario][n][a]) {
                previous_x = -1;
                continue;
            }
            double v = median_of(g_times[scenario][n][a], g_counts[scenario][n][a]);
            if (v <= 0.00001) v = 0.001;
            int x = x0 + n * width / 3;
            int y = y0 + (int)((high_exp - log10(v)) /
                         (high_exp - low_exp) * (double)height);
            if (previous_x >= 0) {
                MoveToEx(dc, previous_x, previous_y, NULL);
                LineTo(dc, x, y);
            }
            HBRUSH dot = CreateSolidBrush(COLORS[a]);
            HGDIOBJ old_brush = SelectObject(dc, dot);
            Ellipse(dc, x - 4, y - 4, x + 4, y + 4);
            SelectObject(dc, old_brush);
            DeleteObject(dot);
            previous_x = x;
            previous_y = y;
        }
        SelectObject(dc, old);
        DeleteObject(pen);
        HBRUSH swatch = CreateSolidBrush(COLORS[a]);
        RECT patch = {270 + a * 145, 676, 286 + a * 145, 687};
        FillRect(dc, &patch, swatch);
        DeleteObject(swatch);
        draw_text(dc, ALGORITHM_UI[a], 293 + a * 145, 671, g_font_small, C_TEXT);
    }
}

/* Funcao paint_dashboard: Compoe o cabecalho, cartoes, botoes e area de resultados. */
static void paint_dashboard(HDC dc) {
    RECT all = {0, 0, APP_W, APP_H};
    FillRect(dc, &all, g_bg_brush);
    RECT nav = {0, 0, 218, APP_H};
    HBRUSH nav_brush = CreateSolidBrush(C_NAV);
    FillRect(dc, &nav, nav_brush);
    DeleteObject(nav_brush);
    draw_text(dc, L"APS 2026/2", 28, 34, g_font_title, RGB(255, 255, 255));
    draw_text(dc, L"CIENCIA DA COMPUTACAO", 28, 71, g_font_small, RGB(169, 191, 220));
    draw_text(dc, L"ESTRUTURA DE DADOS", 28, 93, g_font_small, RGB(169, 191, 220));
    draw_text(dc, L"01   Gerar catalogo", 30, 171, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"02   Carregar CSV", 30, 219, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"03   Entrada manual", 30, 267, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"04   Autotestes", 30, 315, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"05   Benchmark", 30, 363, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"06   Benchmark CSV", 30, 411, g_font_body, RGB(231, 239, 249));
    draw_text(dc, L"Algoritmos originais", 28, 680, g_font_small, RGB(169, 191, 220));
    draw_text(dc, L"preservados em C11", 28, 701, g_font_small, RGB(169, 191, 220));

    draw_text(dc, L"Analise de algoritmos de ordenacao", 245, 34, g_font_title, C_TEXT);
    draw_text(dc, L"Catalogo sintetico de registros | Interface demonstrativa",
              246, 76, g_font_body, C_MUTED);
    wchar_t measurement_count[24];
    swprintf(measurement_count, 24, L"%d", g_measurements);
    const wchar_t *stats[3] = {L"4", L"96", measurement_count};
    const wchar_t *labels[3] = {
        L"Algoritmos de ordenacao", L"Autotestes do projeto", L"Medicoes no grafico"
    };
    for (int i = 0; i < 3; ++i) {
        int x = 244 + i * 338;
        draw_panel(dc, x, 116, 318, 91, RGB(255, 255, 255), C_LINE, 14);
        draw_text(dc, stats[i], x + 19, 132, g_font_stat, C_BLUE);
        draw_text(dc, labels[i], x + 87, 153, g_font_body, C_MUTED);
    }
    draw_panel(dc, 244, 220, 994, 145, RGB(255, 255, 255), C_LINE, 14);
    draw_text(dc, L"Executar funcionalidades", 263, 233, g_font_bold, C_TEXT);
    draw_text(dc, L"Quantidade", 263, 261, g_font_small, C_MUTED);
    draw_text(dc, L"Cenario de entrada", 386, 261, g_font_small, C_MUTED);
    draw_text(dc, L"Semente", 629, 261, g_font_small, C_MUTED);
    draw_panel(dc, 244, 378, 683, 338, RGB(255, 255, 255), C_LINE, 14);
    draw_panel(dc, 943, 378, 295, 338, RGB(255, 255, 255), C_LINE, 14);
    draw_text(dc, L"Log da execucao", 961, 394, g_font_bold, C_TEXT);
    draw_chart(dc);
    draw_text(dc, L"Fonte do grafico:", 264, 700, g_font_small, C_MUTED);
    if (g_has_results)
        draw_text(dc, basename_of(g_result_csv), 376, 700, g_font_small, C_MUTED);
    else draw_text(dc, L"nenhuma", 376, 700, g_font_small, C_MUTED);
    draw_text(dc, g_status, 248, 747, g_font_body, g_busy ? C_BLUE : C_MUTED);
    draw_text(dc, L"Arquivos gerados na pasta gui_saida", 982, 747, g_font_small, C_MUTED);
}

/*
 * O painel foi desenhado em coordenadas logicas APP_W x APP_H.
 * Com a janela maximizada, controles nativos precisam acompanhar a mesma
 * transformacao usada pelo GDI durante WM_PAINT.
 */
/* Funcao move_scaled: Redimensiona controles segundo o tamanho atual da janela. */
static void move_scaled(HWND child, int x, int y, int width, int height,
                        int client_width, int client_height) {
    if (!child) return;
    MoveWindow(child,
               MulDiv(x, client_width, APP_W),
               MulDiv(y, client_height, APP_H),
               MulDiv(width, client_width, APP_W),
               MulDiv(height, client_height, APP_H), TRUE);
}

/* Funcao layout_dashboard: Reorganiza os controles para a area util da janela. */
static void layout_dashboard(HWND window) {
    RECT client;
    GetClientRect(window, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    move_scaled(g_qty, 263, 280, 104, 27, width, height);
    move_scaled(g_scenario, 386, 280, 220, 180, width, height);
    move_scaled(g_seed, 629, 280, 105, 27, width, height);
    for (int i = 0; i < 6; ++i)
        move_scaled(g_buttons[i], 258 + i * 162, 323, 149, 31,
                    width, height);
    move_scaled(g_chart_combo, 742, 398, 166, 180, width, height);
    move_scaled(g_log, 960, 427, 261, 273, width, height);
}

/* Funcao main_proc: Atende a eventos do Windows e aciona os comandos da GUI. */
static LRESULT CALLBACK main_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CREATE) {
        g_main = window;
        g_qty = create_control(WS_EX_CLIENTEDGE, L"EDIT", L"100",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
            263, 280, 104, 27, window, EDIT_QUANTITY);
        g_scenario = create_control(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            386, 280, 220, 180, window, COMBO_SCENARIO);
        for (int i = 0; i < 4; ++i)
            SendMessageW(g_scenario, CB_ADDSTRING, 0, (LPARAM)SCENARIO_UI[i]);
        SendMessageW(g_scenario, CB_SETCURSEL, 0, 0);
        g_seed = create_control(WS_EX_CLIENTEDGE, L"EDIT", L"42",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
            629, 280, 105, 27, window, EDIT_SEED);
        static const wchar_t *button_labels[6] = {
            L"Gerar e analisar", L"Carregar CSV", L"Entrada manual",
            L"96 autotestes", L"Benchmark", L"Abrir benchmark"
        };
        for (int i = 0; i < 6; ++i) {
            g_buttons[i] = create_control(0, L"BUTTON", button_labels[i],
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                258 + i * 162, 323, 149, 31, window, BTN_GENERATE + i);
        }
        g_chart_combo = create_control(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            742, 398, 166, 180, window, COMBO_CHART);
        for (int i = 0; i < 4; ++i)
            SendMessageW(g_chart_combo, CB_ADDSTRING, 0, (LPARAM)SCENARIO_UI[i]);
        SendMessageW(g_chart_combo, CB_SETCURSEL, 0, 0);
        g_log = create_control(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY | WS_VSCROLL,
            960, 427, 261, 273, window, EDIT_LOG);
        SendMessageW(g_log, WM_SETFONT, (WPARAM)g_font_log, TRUE);
        SendMessageW(g_log, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(6, 6));
        append_to_log(L"Bem-vindo!\r\n\r\nEscolha uma operacao acima.\r\n\r\n");
        append_to_log(L"Os arquivos originais nao serao alterados.\r\n");
        wchar_t old_result[MAX_PATH * 2];
        make_path(old_result, MAX_PATH * 2, g_base_dir, L"resultados_brutos.csv");
        if (exists_file(old_result)) {
            int rows = load_benchmark(old_result);
            if (rows > 0) set_status(L"Grafico inicial: benchmark original documentado.");
        }
        layout_dashboard(window);
        return 0;
    }
    if (message == WM_SIZE) {
        if (wparam != SIZE_MINIMIZED) {
            layout_dashboard(window);
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    }
    if (message == WM_COMMAND) {
        int id = LOWORD(wparam);
        if (id == COMBO_CHART && HIWORD(wparam) == CBN_SELCHANGE) {
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (HIWORD(wparam) == BN_CLICKED) {
            switch (id) {
                case BTN_GENERATE: begin_generate(); break;
                case BTN_OPEN_CSV: begin_analyze(); break;
                case BTN_MANUAL: begin_manual(); break;
                case BTN_SELFTEST: begin_selftest(); break;
                case BTN_BENCHMARK: begin_benchmark(); break;
                case BTN_OPEN_RESULTS: open_results(); break;
                default: break;
            }
        }
        return 0;
    }
    if (message == WM_DRAWITEM) {
        DRAWITEMSTRUCT *draw = (DRAWITEMSTRUCT *)lparam;
        int id = (int)draw->CtlID;
        if (id >= BTN_GENERATE && id <= BTN_OPEN_RESULTS) {
            int disabled = (draw->itemState & ODS_DISABLED) != 0;
            int pressed = (draw->itemState & ODS_SELECTED) != 0;
            COLORREF bg = disabled ? RGB(178, 189, 203) :
                          pressed ? RGB(26, 73, 166) :
                          id == BTN_GENERATE ? C_BLUE : RGB(55, 80, 114);
            RECT r = draw->rcItem;
            HBRUSH brush = CreateSolidBrush(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, bg);
            HGDIOBJ old_brush = SelectObject(draw->hDC, brush);
            HGDIOBJ old_pen = SelectObject(draw->hDC, pen);
            RoundRect(draw->hDC, r.left, r.top, r.right, r.bottom, 10, 10);
            SelectObject(draw->hDC, old_brush);
            SelectObject(draw->hDC, old_pen);
            DeleteObject(brush);
            DeleteObject(pen);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, RGB(255, 255, 255));
            SelectObject(draw->hDC, g_font_small);
            wchar_t label[100];
            GetWindowTextW(draw->hwndItem, label, 100);
            DrawTextW(draw->hDC, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
    }
    if (message == WM_CTLCOLOREDIT) {
        HDC dc = (HDC)wparam;
        SetBkColor(dc, RGB(255, 255, 255));
        SetTextColor(dc, C_TEXT);
        return (LRESULT)g_white_brush;
    }
    if (message == WM_CTLCOLORSTATIC) {
        HDC dc = (HDC)wparam;
        if ((HWND)lparam == g_log) {
            /* EDIT somente leitura envia WM_CTLCOLORSTATIC, nao WM_CTLCOLOREDIT.
             * Fundo transparente deixa caracteres antigos sobrepostos ao rolar. */
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, RGB(255, 255, 255));
            SetTextColor(dc, C_TEXT);
            return (LRESULT)g_white_brush;
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT);
        return (LRESULT)g_bg_brush;
    }
    if (message == WM_LOG_APPEND) {
        wchar_t *text = (wchar_t *)lparam;
        if (text) {
            append_to_log(text);
            free(text);
        }
        return 0;
    }
    if (message == WM_TASK_DONE) {
        TaskResult *result = (TaskResult *)lparam;
        InterlockedExchange(&g_busy, 0);
        enable_actions(1);
        if (result && result->success) {
            set_status(L"Operacao concluida. Confira os logs e os arquivos gerados.");
            append_to_log(L"\r\n[CONCLUIDO] Comando executado com sucesso.\r\n");
            if (result->type == TASK_BENCHMARK && result->path[0]) {
                int rows = load_benchmark(result->path);
                if (rows > 0) {
                    set_status(L"Benchmark novo carregado no grafico (arquivo separado).");
                    append_to_log(L"[GRAFICO] Medicoes novas carregadas. "
                                  L"Altere o cenario para comparar as curvas.\r\n");
                } else {
                    set_status(L"Benchmark finalizado, mas o grafico nao conseguiu ler o CSV.");
                }
            } else if (result->type == TASK_GENERATE ||
                       result->type == TASK_ANALYZE) {
                set_status(L"Catalogo ordenado. O grafico mostra apenas CSVs de benchmark.");
                append_to_log(L"[GRAFICO] Esta analise nao atualiza o grafico de "
                              L"benchmark. Para gerar novas curvas, use Benchmark.\r\n");
            } else if (result->type == TASK_SELFTEST) {
                set_status(L"Autotestes concluidos. Confira a mensagem de aprovacao no log.");
            }
        } else {
            set_status(L"A operacao encontrou erro. Veja o log.");
            append_to_log(L"\r\n[ERRO] Verifique a entrada e o log.\r\n");
        }
        free(result);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        int width = client.right - client.left;
        int height = client.bottom - client.top;
        if (width > 0 && height > 0) {
            int state = SaveDC(dc);
            SetMapMode(dc, MM_ANISOTROPIC);
            SetWindowExtEx(dc, APP_W, APP_H, NULL);
            SetViewportExtEx(dc, width, height, NULL);
            paint_dashboard(dc);
            RestoreDC(dc, state);
        }
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_CLOSE) {
        if (InterlockedCompareExchange(&g_busy, 0, 0)) {
            MessageBoxW(window, L"Aguarde a tarefa em andamento terminar.",
                        L"Operacao em curso", MB_ICONINFORMATION);
            return 0;
        }
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

/* Funcao WinMain: Inicializa a aplicacao grafica e inicia o loop de eventos Win32. */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show) {
    (void)previous;
    (void)command_line;
    g_instance = instance;
    DWORD len = GetModuleFileNameW(NULL, g_base_dir, MAX_PATH * 2);
    if (!len || len >= MAX_PATH * 2) return 1;
    wchar_t *slash = wcsrchr(g_base_dir, L'\\');
    if (!slash) return 1;
    *slash = L'\0';
    make_path(g_output_dir, MAX_PATH * 2, g_base_dir, L"gui_saida");
    if (!CreateDirectoryW(g_output_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 1;
    make_path(g_core_exe, MAX_PATH * 2, g_base_dir, L"aps_ordenacao.exe");
    g_bg_brush = CreateSolidBrush(C_BG);
    g_white_brush = CreateSolidBrush(RGB(255, 255, 255));
    g_font_small = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");
    g_font_body = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    g_font_bold = CreateFontW(-19, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    g_font_title = CreateFontW(-29, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");
    g_font_stat = CreateFontW(-38, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    g_font_log = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH, L"Consolas");
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = main_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"APSOrdenacaoMain";
    wc.hbrBackground = g_bg_brush;
    if (!RegisterClassW(&wc)) return 1;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = manual_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"APSManualWindow";
    wc.hbrBackground = g_bg_brush;
    if (!RegisterClassW(&wc)) return 1;
    HWND window = CreateWindowExW(0, L"APSOrdenacaoMain",
        L"APS - Analise de Algoritmos de Ordenacao em C",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, APP_W + 16, APP_H + 39,
        NULL, NULL, instance, NULL);
    if (!window) return 1;
    (void)show;
    ShowWindow(window, SW_SHOWMAXIMIZED);
    UpdateWindow(window);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(g_font_small);
    DeleteObject(g_font_body);
    DeleteObject(g_font_bold);
    DeleteObject(g_font_title);
    DeleteObject(g_font_stat);
    DeleteObject(g_font_log);
    DeleteObject(g_bg_brush);
    DeleteObject(g_white_brush);
    return (int)msg.wParam;
}
