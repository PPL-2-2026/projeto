/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 2: paraleliza a recursão (msort) E o merge.
 * A versão com medições está em openmpV2Perfilamento.cpp.
 *
 * Compilar: g++ -O2 -fopenmp -o openmp openmp.cpp
 * Executar: OMP_NUM_THREADS=8 ./openmp 20000000   (tamanho do vetor)
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <ctime>
#include <algorithm>
#include <omp.h>
#include "../../utils/baseline.h"
using namespace std;

/* ---------- versão paralela ---------- */

/* Divide a intercalação em trechos independentes.
 * O pivô (meio da metade maior) separa por valor: tudo que vai para a
 * primeira parte é <= tudo que vai para a segunda, e cada parte escreve
 * na sua própria faixa do auxiliar. Por isso as duas podem rodar ao mesmo tempo. */
static void intercala_dividindo(const vector<int>& dados, vector<int>& auxiliar,
                                int inicio_esq, int fim_esq, int inicio_dir, int fim_dir, int inicio_saida, int particoes_do_trecho) {
    int tamanho_esq = fim_esq - inicio_esq, tamanho_dir = fim_dir - inicio_dir;
    if (particoes_do_trecho <= 1 || tamanho_esq + tamanho_dir == 0) { // não divide mais, ou trecho vazio
        intercala(dados, auxiliar, inicio_esq, fim_esq, inicio_dir, fim_dir, inicio_saida);
        return;
    }
    int corte_esq, corte_dir;
    if (tamanho_esq >= tamanho_dir) {   // divide a metade esquerda no meio
        corte_esq = (inicio_esq + fim_esq) / 2; // encontra o valor do meio do primeiro trecho
        corte_dir = lower_bound(dados.begin() + inicio_dir, dados.begin() + fim_dir, dados[corte_esq]) - dados.begin(); // busca no segundo trecho o primeiro valor maior ou igual ao valor que está no meio do primeiro trecho
    } else {          // divide a metade direita no meio
        corte_dir = (inicio_dir + fim_dir) / 2;
        corte_esq = upper_bound(dados.begin() + inicio_esq, dados.begin() + fim_esq, dados[corte_dir]) - dados.begin(); // Busca no primeiro trecho o primeiro valor estritamente maior que esse valor do meio
    }
    int inicio_segunda_saida = inicio_saida + (corte_esq - inicio_esq) + (corte_dir - inicio_dir); // onde a segunda parte começa no auxiliar

    #pragma omp task shared(dados, auxiliar)
    {
        intercala_dividindo(dados, auxiliar, inicio_esq, corte_esq, inicio_dir, corte_dir, inicio_saida, particoes_do_trecho / 2);
    }
    intercala_dividindo(dados, auxiliar, corte_esq, fim_esq, corte_dir, fim_dir, inicio_segunda_saida, particoes_do_trecho - particoes_do_trecho / 2); // na thread atual
    #pragma omp taskwait
}

// Merge dividido em partições: intercalação e cópia de volta.
static void merge_paralelo(vector<int>& dados, vector<int>& auxiliar, int inicio, int meio, int fim, int particoes_do_trecho) {
    if (particoes_do_trecho <= 1) {
        baseline_merge(dados, auxiliar, inicio, meio, fim);
        return;
    }
    intercala_dividindo(dados, auxiliar, inicio, meio, meio, fim, inicio, particoes_do_trecho);
    #pragma omp taskloop num_tasks(particoes_do_trecho) shared(dados, auxiliar)
    for (int indice = inicio; indice < fim; indice++) {
        dados[indice] = auxiliar[indice];
    }
}

/* Cada chamada divide suas partições entre as duas metades. Com uma única
 * partição, a subárvore é ordenada sequencialmente. */
static void msort_paralelo(vector<int>& dados, vector<int>& auxiliar, int inicio, int fim, int particoes_do_trecho) {
    if (particoes_do_trecho <= 1) {
        baseline_msort(dados, auxiliar, inicio, fim);
        return;
    }
    int meio = (inicio + fim) / 2;
    /* A metade esquerda vira task (outra thread executa) e a direita é feita
     * pela thread atual, ao mesmo tempo. */
    #pragma omp task shared(dados, auxiliar)
    {
        msort_paralelo(dados, auxiliar, inicio, meio, particoes_do_trecho / 2);
    }
    msort_paralelo(dados, auxiliar, meio, fim, particoes_do_trecho - particoes_do_trecho / 2);
    
    // espera os dois lados terminarem pra iniciar o merge
    #pragma omp taskwait
    merge_paralelo(dados, auxiliar, inicio, meio, fim, particoes_do_trecho);
}

int main(int argc, char** argv) {
    int tamanho_vetor = (argc > 1) ? atoi(argv[1]) : 20000000;
    vector<int> dados(tamanho_vetor), auxiliar(tamanho_vetor);
    gerar_vetor(dados);

    int max_threads = omp_get_max_threads();
    double inicio_ordenacao = agora();
    #pragma omp parallel
    {
        #pragma omp single
        {
            msort_paralelo(dados, auxiliar, 0, tamanho_vetor, max_threads);
        }
    }
    double fim_ordenacao = agora();

    bool ordenado = true;
    for (int indice = 1; indice < tamanho_vetor; indice++) if (dados[indice-1] > dados[indice]) { ordenado = false; break; }
    printf("n=%d  threads=%d  ordenado=%s  tempo=%.6f s\n", tamanho_vetor, max_threads, ordenado ? "SIM" : "NAO", fim_ordenacao - inicio_ordenacao);
    return 0;
}
