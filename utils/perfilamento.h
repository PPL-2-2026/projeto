/* Estruturas compartilhadas pelo perfilamento do baseline e da versão paralela. */
#pragma once
#include <algorithm>
#include <vector>

// Tempos acumulados dos merges de um nível da recursão.
struct Nivel {
    long long merges = 0;
    int tamanho = 0; // tamanho do trecho neste nível
    double sort = 0.0; // laços while (intercalação em tmp)
    double copia = 0.0; // laço for (copia tmp de volta para a)
    double maior = 0.0; // maior merge individual do nível
};

/* Soma o tempo de um merge na linha do seu nível.
 * Não é protegida contra várias threads: quem chama em paralelo deve
 * envolver a chamada com #pragma omp critical (ver OpenMP/v2/v2.md). */
inline void registrar(std::vector<Nivel>& niveis, int nivel, int tamanho, double t_sort, double t_copia) {
    if ((int)niveis.size() <= nivel) niveis.resize(nivel + 1);
    niveis[nivel].merges++;
    niveis[nivel].tamanho = std::max(niveis[nivel].tamanho, tamanho);
    niveis[nivel].sort += t_sort;
    niveis[nivel].copia += t_copia;
    niveis[nivel].maior = std::max(niveis[nivel].maior, t_sort + t_copia);
}
