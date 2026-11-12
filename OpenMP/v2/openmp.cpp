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

/* Divide a intercalação em "partes" pedaços independentes.
 * O pivô (meio da metade maior) separa por valor: tudo que vai para a
 * primeira parte é <= tudo que vai para a segunda, e cada parte escreve
 * na sua própria faixa de tmp. Por isso as duas podem rodar ao mesmo tempo. */
static void intercala_dividindo(const vector<int>& a, vector<int>& tmp,
                                int l1, int r1, int l2, int r2, int k, int partes) {
    int n1 = r1 - l1, n2 = r2 - l2;
    if (partes <= 1 || n1 + n2 == 0) { // não divide mais, ou trecho vazio
        intercala(a, tmp, l1, r1, l2, r2, k);
        return;
    }
    int m1, m2;
    if (n1 >= n2) {   // divide a metade esquerda no meio
        m1 = (l1 + r1) / 2; // encontra o valor do meio do primeiro trecho
        m2 = lower_bound(a.begin() + l2, a.begin() + r2, a[m1]) - a.begin(); // busca no segundo trecho o primeiro valor maior ou igual ao valor que está no meio do primeiro trecho
    } else {          // divide a metade direita no meio
        m2 = (l2 + r2) / 2;
        m1 = upper_bound(a.begin() + l1, a.begin() + r1, a[m2]) - a.begin(); // Busca no primeiro trecho o primeiro valor estritamente maior que esse valor do meio
    }
    int km = k + (m1 - l1) + (m2 - l2); // onde a segunda parte começa em tmp

    #pragma omp task shared(a, tmp)
    {
        intercala_dividindo(a, tmp, l1, m1, l2, m2, k, partes / 2);
    }
    intercala_dividindo(a, tmp, m1, r1, m2, r2, km, partes - partes / 2); // na thread atual
    #pragma omp taskwait
}

// Merge dividido entre "partes" threads: intercalação e cópia de volta.
static void merge_paralelo(vector<int>& a, vector<int>& tmp, int lo, int mid, int hi, int partes) {
    if (partes <= 1) {
        baseline_merge(a, tmp, lo, mid, hi);
        return;
    }
    intercala_dividindo(a, tmp, lo, mid, mid, hi, lo, partes);
    #pragma omp taskloop num_tasks(partes) shared(a, tmp)
    for (int t = lo; t < hi; t++) {
        a[t] = tmp[t];
    }
}

/* "partes" é quantas threads estão disponíveis para este trecho. Ele é
 * dividido ao meio a cada nível; quando chega a 1, o resto da subárvore
 * roda sequencial. Assim só os níveis de cima, que têm menos merges que
 * threads, usam o merge paralelo. */
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
    merge_paralelo(a, tmp, lo, mid, hi, partes);
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
