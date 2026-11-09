/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 1: paraleliza só a recursão (msort). O merge é o do baseline, sem alteração.
 * Mede com o mesmo protocolo do perfilamento do baseline:
 *   Fase 1: tempo de parede da versão paralela, validado contra o baseline
 *   Fase 2: onde o tempo é gasto, por nível (sort e cópia de cada merge)
 *
 * Compilar: g++ -O2 -fopenmp -o openmpV1Perfilamento openmpV1Perfilamento.cpp
 * Executar: OMP_NUM_THREADS=8 ./openmpV1Perfilamento [n=20000000] [reps=10] [aquec=2] [corte=2160]
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <omp.h>
#include "../../utils/baseline.h"
#include "../../utils/perfilamento.h"
using namespace std;

/* ---------- versão paralela (igual ao openmp.cpp) ---------- */

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

/* ---------- versão instrumentada (mesmo protocolo do perfilamento.cpp) ---------- */
// Mesmo corte do perfilamento: merges menores que isso não são cronometrados.
static int CORTE = 2160;
static vector<Nivel> niveis;

// Merge do baseline cronometrado: igual ao perfilamento do baseline.
static void merge_perfilamento(vector<int>& a, vector<int>& tmp, int lo, int mid, int hi, int nivel) {
    double t0 = agora();
    intercala(a, tmp, lo, mid, mid, hi, lo);
    double t1 = agora();
    for (int t = lo; t < hi; t++) a[t] = tmp[t];
    double t2 = agora();
    // várias threads atualizam a mesma tabela: cinco campos que mudam juntos (v1.md)
    #pragma omp critical
    {
        registrar(niveis, nivel, hi - lo, t1 - t0, t2 - t1);
    }
}

static void msort_perfilamento(vector<int>& a, vector<int>& tmp, int lo, int hi, int nivel) {
    if (hi - lo < CORTE || hi - lo <= 1) {
        baseline_msort(a, tmp, lo, hi);
        return;
    }
    int mid = (lo + hi) / 2;
    msort_perfilamento(a, tmp, lo, mid, nivel + 1);
    msort_perfilamento(a, tmp, mid, hi, nivel + 1);
    merge_perfilamento(a, tmp, lo, mid, hi, nivel);
}

static void msort_paralelo_perfilamento(vector<int>& a, vector<int>& tmp,
                                        int lo, int hi, int partes, int nivel) {
    if (partes <= 1) {
        msort_perfilamento(a, tmp, lo, hi, nivel);
        return;
    }
    int mid = (lo + hi) / 2;
    #pragma omp task shared(a, tmp)
    {
        msort_paralelo_perfilamento(a, tmp, lo, mid, partes / 2, nivel + 1);
    }
    msort_paralelo_perfilamento(a, tmp, mid, hi, partes - partes / 2, nivel + 1);
    #pragma omp taskwait
    merge_perfilamento(a, tmp, lo, mid, hi, nivel); // merge do baseline, numa thread só
}

/* ---------- protocolo de medição ---------- */

/* ---- Fase 1: wall time da versão paralela, validado contra o baseline ---- */
static double fase1(vector<int>& a, vector<int>& tmp, const vector<int>& referencia,
                    int threads, int reps, int aquec) {
    int n = (int)a.size();
    double tempo_medio = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        // Toda repetição usa o mesmo vetor inicial
        gerar_vetor(a);
        double t0 = agora();
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo(a, tmp, 0, n, threads);
            }
        }
        double dt = agora() - t0;
        // Aquecimento é descartado
        if (r >= aquec) tempo_medio += dt;
    }
    tempo_medio /= reps; // média das repetições
    // validação: o resultado tem que ser idêntico, elemento a elemento, ao do baseline
    bool igual = (a == referencia);
    printf("\n[Fase 1] tempo por ordenacao=%.6f s  igual ao baseline=%s\n", tempo_medio, igual ? "SIM" : "NAO");
    return tempo_medio;
}

/* ---- Fase 2: onde o tempo é gasto (média das repetições) ---- */
static void fase2(vector<int>& a, vector<int>& tmp, const vector<int>& referencia,
                  int threads, int reps, int aquec, double tempo_fase1) {
    int n = (int)a.size();
    double total_perfilamento = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        if (r == aquec) { // descarta o aquecimento
            niveis.clear(); total_perfilamento = 0.0;
        }
        gerar_vetor(a); // mesma entrada a cada repetição
        double t0 = agora();
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo_perfilamento(a, tmp, 0, n, threads, 0);
            }
        }
        total_perfilamento += agora() - t0;
    }
    bool igual = (a == referencia);
    double media = total_perfilamento / reps;

    printf("\n[Fase 2] onde o tempo e gasto (media de %d repeticoes)  igual ao baseline=%s\n",
           reps, igual ? "SIM" : "NAO");
    // Cabeçalho da tabela
    printf("  %5s %10s %7s %11s %11s %11s %11s\n",
           "nivel", "tamanho", "merges", "sort", "copia", "total", "maior");
    double soma_sort = 0.0, soma_copia = 0.0;
    // Linhas da tabela
    for (size_t d = 0; d < niveis.size(); d++) {
        double tempo_sort = niveis[d].sort / reps;
        double copia = niveis[d].copia / reps;
        double total = tempo_sort + copia;
        long long merges = niveis[d].merges / reps;
        soma_sort += tempo_sort;
        soma_copia += copia;
        printf("  %5zu %10d %7lld %10.6fs %10.6fs %10.6fs %10.6fs\n",
               d, niveis[d].tamanho, merges, tempo_sort, copia, total, niveis[d].maior);
    }
    double soma_merges = soma_sort + soma_copia;
    double percentual_sort = 100.0 * soma_sort / soma_merges;
    double percentual_copia = 100.0 * soma_copia / soma_merges;
    double custo_instrumentacao = 100.0 * (media - tempo_fase1) / tempo_fase1;

    // os merges de um mesmo nível rodam ao mesmo tempo, então as somas são tempo de trabalho das threads
    printf("\n  merges grandes, somando as threads: %.6f s\n", soma_merges);
    printf("    sort:                        %.6f s (%5.1f%% dos merges grandes)\n", soma_sort, percentual_sort);
    printf("    copia de volta:              %.6f s (%5.1f%% dos merges grandes)\n", soma_copia, percentual_copia);
    printf("  total instrumentado: %.6f s  (custo da instrumentacao: %+.1f%% vs Fase 1)\n", media, custo_instrumentacao);
}

int main(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 20000000;
    int reps = (argc > 2) ? atoi(argv[2]) : 10;
    int aquec = (argc > 3) ? atoi(argv[3]) : 2;
    CORTE = (argc > 4) ? atoi(argv[4]) : 2160;
    int threads = omp_get_max_threads();

    vector<int> a(n), tmp(n);

    // resultado de referência: o mesmo vetor ordenado pelo msort do baseline (fora do cronômetro)
    vector<int> referencia(n);
    gerar_vetor(referencia);
    baseline_msort(referencia, tmp, 0, n);

    printf("n=%d  (a + tmp = %.1f MiB)  threads=%d  reps=%d  aquecimento=%d  corte=%d\n",
           n, 2.0 * n * sizeof(int) / (1024.0 * 1024.0), threads, reps, aquec, CORTE);

    double tempo_fase1 = fase1(a, tmp, referencia, threads, reps, aquec);
    if (n < CORTE) {
        printf("\n[Fase 2] ignorada: n menor que o corte (%d)\n", CORTE);
        return 0;
    }
    fase2(a, tmp, referencia, threads, reps, aquec, tempo_fase1);
    return 0;
}
