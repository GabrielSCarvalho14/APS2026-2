#include "menu.h"
#include "nucleo.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Funcao main: Abre o menu sem argumentos e repassa comandos ao nucleo original. */
int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "--menu") == 0)) {
        return executar_menu_principal();
    }

    /* Todos os comandos originais continuam funcionando sem modificacoes. */
    return aps_core_main(argc, argv);
}
