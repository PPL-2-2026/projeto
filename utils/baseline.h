/* Funções do baseline.cpp (Baseline 07 — Prof. Rodrigo Goncalves Pinto) para serem
 * reaproveitadas pelas outras versões: cronômetro, geração do vetor de entrada,
 * merge e msort. O algoritmo é o mesmo; os laços while do merge ficaram na função
 * intercala, que a versão paralela também usa. */
#pragma once
#include <cstdint>
#include <ctime>
#include <vector>

// Tempo atual em segundos (relógio monotônico).
inline double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* Gera o vetor de entrada exatamente como o baseline.cpp (semente fixa) */
inline void gerar_vetor(std::vector<int>& a) {
    uint64_t s = 12345;
    for (size_t i = 0; i < a.size(); i++) { s = s*6364136223846793005ULL + 1; a[i] = (int)(s >> 33); }
}

// Intercala a[l1,r1) com a[l2,r2), escrevendo em tmp a partir de k.
// São os laços while do merge do baseline, para dois trechos quaisquer.
inline void intercala(const std::vector<int>& a, std::vector<int>& tmp,
                      int l1, int r1, int l2, int r2, int k) {
    while (l1 < r1 && l2 < r2) tmp[k++] = (a[l1] <= a[l2]) ? a[l1++] : a[l2++];
    while (l1 < r1) tmp[k++] = a[l1++];
    while (l2 < r2) tmp[k++] = a[l2++];
}

inline void baseline_merge(std::vector<int>& a, std::vector<int>& tmp, int lo, int mid, int hi) {
    intercala(a, tmp, lo, mid, mid, hi, lo);
    for (int t = lo; t < hi; t++) a[t] = tmp[t];
}

inline void baseline_msort(std::vector<int>& a, std::vector<int>& tmp, int lo, int hi) {
    if (hi - lo <= 1) return;
    int mid = (lo + hi) / 2;
    baseline_msort(a, tmp, lo, mid);
    baseline_msort(a, tmp, mid, hi);
    baseline_merge(a, tmp, lo, mid, hi);
}
