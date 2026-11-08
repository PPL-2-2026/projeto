/* Perfilamento do Baseline 07 — Mergesort
 * IESB 2026/2 — CCO085
 * Compilar: g++ -O2 -o perfilamento perfilamento.cpp
 * Executar: ./perfilamento [n=20000000] [reps=10] [aquec=2] [corte=2160]
 */
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <vector>
#include <algorithm>
using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* ---------- algoritmo original (igual ao baseline.cpp) ---------- */
static void merge(vector<int>& a, vector<int>& tmp, int lo, int mid, int hi) {
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi)  tmp[k++] = a[j++];
    for (int t = lo; t < hi; t++) a[t] = tmp[t];
}
static void msort(vector<int>& a, vector<int>& tmp, int lo, int hi) {
    if (hi - lo <= 1) return;
    int mid = (lo + hi) / 2;
    msort(a, tmp, lo, mid);
    msort(a, tmp, mid, hi);
    merge(a, tmp, lo, mid, hi);
}

/* Gera o vetor de entrada exatamente como o baseline.cpp (semente fixa) */
static void gerar_vetor(vector<int>& a) {
    uint64_t s = 12345;
    for (size_t i = 0; i < a.size(); i++) { s = s*6364136223846793005ULL + 1; a[i] = (int)(s >> 33); }
}

static bool ordenado(const vector<int>& a) {
    for (size_t i = 1; i < a.size(); i++) if (a[i - 1] > a[i]) return false;
    return true;
}

/* ---------- versão instrumentada ---------- */
struct Nivel {
    long long merges = 0;
    int tamanho = 0; // tamanho do trecho neste nível
    double sort = 0.0; // laços while (monta o resultado do sort em tmp)
    double copia = 0.0; // laço for (copia tmp de volta para a)
    double maior = 0.0; // maior merge individual do nível
};

/* As chamadas do clock também têm um custo em tempo (~36 ns cada) e nos níveis
 * inferiores do merge esse custo acaba ficando maior que o custo do próprio
 * merge. Então nós implementamos um numero de corte e qualquer subárvore com
 * menos de {CORTE} elementos é ordenada pelo msort original, sem instrumentação. */
static int CORTE = 2160;
static vector<Nivel> niveis;

static void merge_perfilamento(vector<int>& a, vector<int>& tmp,
                               int lo, int mid, int hi, int nivel) {
    double t0 = agora();
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi) tmp[k++] = a[j++];
    double t1 = agora();
    for (int t = lo; t < hi; t++) a[t] = tmp[t];
    double t2 = agora();

    if ((int)niveis.size() <= nivel) niveis.resize(nivel + 1);
    niveis[nivel].merges++;
    niveis[nivel].tamanho = max(niveis[nivel].tamanho, hi - lo);
    niveis[nivel].sort += t1 - t0;
    niveis[nivel].copia += t2 - t1;
    niveis[nivel].maior = max(niveis[nivel].maior, t2 - t0);
}

static void msort_perfilamento(vector<int>& a, vector<int>& tmp, int lo, int hi, int nivel) {
    if (hi - lo < CORTE || hi - lo <= 1) {
        msort(a, tmp, lo, hi);
        return;
    }
    int mid = (lo + hi) / 2;
    msort_perfilamento(a, tmp, lo, mid, nivel + 1);
    msort_perfilamento(a, tmp, mid, hi, nivel + 1);
    merge_perfilamento(a, tmp, lo, mid, hi, nivel);
}

/* ---- Fase 1: wall time, sem instrumentação (média das repetições)---- */

static double fase1(vector<int>& a, vector<int>& tmp,
                    int reps, int aquec) {
    int n = (int)a.size();
    double tempo_medio = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        // Toda repetição usa o mesmo vetor inicial
        gerar_vetor(a);
        double t0 = agora();
        msort(a, tmp, 0, n);
        double dt = agora() - t0;
        // Aquecimento é descartado
        if (r >= aquec) tempo_medio += dt;
    }
    bool ok = ordenado(a);
    tempo_medio /= reps; // média das repetições
    printf("\n[Fase 1] tempo por ordenacao=%.6f s  ordenado=%s\n", tempo_medio, ok ? "SIM" : "NAO");
    return tempo_medio;
}

/* ---- Fase 2: onde o tempo é gasto (média das repetições)---- */
static void fase2(vector<int>& a, vector<int>& tmp,
                  int reps, int aquec, double tempo_fase1) {
    int n = (int)a.size();
    double total_perfilamento = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        if (r == aquec) { // descarta o aquecimento
            niveis.clear(); total_perfilamento = 0.0;
        }
        gerar_vetor(a); // mesma entrada a cada repetição
        double t0 = agora();
        msort_perfilamento(a, tmp, 0, n, 0);
        total_perfilamento += agora() - t0;
    }
    bool ok = ordenado(a);
    double media = total_perfilamento / reps;

    printf("\n[Fase 2] onde o tempo e gasto (media de %d repeticoes)  ordenado=%s\n",
           reps, ok ? "SIM" : "NAO");
    // Cabeçalho da tabela
    printf("  %5s %10s %7s %11s %11s %11s %7s %11s\n",
           "nivel", "tamanho", "merges", "sort", "copia", "total", "%", "maior");
    double soma_sort = 0.0, soma_copia = 0.0;
    // Linhas da tabela
    for (size_t d = 0; d < niveis.size(); d++) {
        double tempo_sort = niveis[d].sort / reps;
        double copia = niveis[d].copia / reps;
        double total = tempo_sort + copia;
        double percentual = 100.0 * total / media;
        long long merges = niveis[d].merges / reps;
        soma_sort += tempo_sort;
        soma_copia += copia;
        printf("  %5zu %10d %7lld %10.6fs %10.6fs %10.6fs %6.1f%% %10.6fs\n",
               d, niveis[d].tamanho, merges, tempo_sort, copia, total, percentual, niveis[d].maior);
    }
    double soma_merges = soma_sort + soma_copia;
    double abaixo_corte = media - soma_merges; // calculado por diferença
    int ultimo_nivel = (int)niveis.size() - 1;

    double percentual_merges = 100.0 * soma_merges / media;
    double percentual_sort = 100.0 * soma_sort / soma_merges;
    double percentual_copia = 100.0 * soma_copia / soma_merges;
    double percentual_abaixo_corte = 100.0 * abaixo_corte / media;
    double custo_instrumentacao = 100.0 * (media - tempo_fase1) / tempo_fase1;

    printf("\n  merges grandes (niveis 0-%d):   %.6f s (%5.1f%%)\n", ultimo_nivel, soma_merges, percentual_merges);
    printf("    sort:                        %.6f s (%5.1f%% dos merges grandes)\n", soma_sort, percentual_sort);
    printf("    copia de volta:              %.6f s (%5.1f%% dos merges grandes)\n", soma_copia, percentual_copia);
    printf("  abaixo do corte (< %d):     %.6f s (%5.1f%%)\n", CORTE, abaixo_corte, percentual_abaixo_corte);
    printf("  total instrumentado: %.6f s  (custo da instrumentacao: %+.1f%% vs Fase 1)\n", media, custo_instrumentacao);
}

int main(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 20000000;
    int reps = (argc > 2) ? atoi(argv[2]) : 10;
    int aquec = (argc > 3) ? atoi(argv[3]) : 2;
    CORTE = (argc > 4) ? atoi(argv[4]) : 2160;

    vector<int> a(n), tmp(n);

    printf("n=%d  (a + tmp = %.1f MiB)  reps=%d  aquecimento=%d  corte=%d\n",
           n, 2.0 * n * sizeof(int) / (1024.0 * 1024.0), reps, aquec, CORTE);

    double tempo_fase1 = fase1(a, tmp, reps, aquec);
    if (n < CORTE) {
        printf("\n[Fase 2] ignorada: n menor que o corte (%d)\n", CORTE);
        return 0;
    }
    fase2(a, tmp, reps, aquec, tempo_fase1);
    return 0;
}
