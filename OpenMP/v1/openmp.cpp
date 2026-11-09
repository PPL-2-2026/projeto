/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 1: paraleliza só a recursão (msort). O merge é o do baseline, sem alteração.
 *
 * Compilar: g++ -O2 -fopenmp -o openmp openmp.cpp
 * Executar: OMP_NUM_THREADS=8 ./openmp 20000000   (tamanho do vetor)
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <omp.h>
#include "../../utils/baseline.h"
using namespace std;

/* "partes" é quantas threads estão disponíveis para este trecho. Ele é
 * dividido ao meio a cada nível; quando chega a 1, o resto da subárvore
 * roda sequencial, com o msort do baseline. */
static void msort_paralelo(vector<int>& a, vector<int>& tmp, int lo, int hi, int partes) {
    if (partes <= 1) {
        baseline_msort(a, tmp, lo, hi);
        return;
    }
    int mid = (lo + hi) / 2;
    /* A metade esquerda vira task (outra thread executa) e a direita é feita
     * pela thread atual, ao mesmo tempo. */
    #pragma omp task shared(a, tmp)
    {
        msort_paralelo(a, tmp, lo, mid, partes / 2);
    }
    msort_paralelo(a, tmp, mid, hi, partes - partes / 2);

    // espera os dois lados terminarem pra iniciar o merge
    #pragma omp taskwait
    baseline_merge(a, tmp, lo, mid, hi);
}

int main(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 20000000;
    vector<int> a(n), tmp(n);
    gerar_vetor(a);

    int threads = omp_get_max_threads();
    double t0 = agora();
    #pragma omp parallel
    {
        #pragma omp single
        {
            msort_paralelo(a, tmp, 0, n, threads);
        }
    }
    double t1 = agora();

    bool ok = true;
    for (int i = 1; i < n; i++) if (a[i-1] > a[i]) { ok = false; break; }
    printf("n=%d  threads=%d  ordenado=%s  tempo=%.6f s\n", n, threads, ok ? "SIM" : "NAO", t1 - t0);
    return 0;
}
