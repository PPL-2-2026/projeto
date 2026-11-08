#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <ctime>
#include <omp.h>
using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void merge(vector<int>& a, vector<int>& tmp, int lo, int mid, int hi) {
    int i = lo, j = mid, k = lo;
     
        while (i < mid && j < hi) tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
        while (i < mid) tmp[k++] = a[i++];
        while (j < hi)  tmp[k++] = a[j++];
    
        #pragma omp parallel for num_threads(6)
            for (int t = lo; t < hi; t++) a[t] = tmp[t];        
}

static void msort(vector<int>& a, vector<int>& tmp, int lo, int hi) {
    if (hi - lo <= 1) return;
    int mid = (lo + hi) / 2;

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            msort(a, tmp, lo, mid);
        }
        #pragma omp section
        {
            msort(a, tmp, mid, hi);
        }
    }
    merge(a, tmp, lo, mid, hi);
}

int main(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 20000000;
    vector<int> a(n), tmp(n);
    uint64_t s = 12345;
    for (int i = 0; i < n; i++) { s = s*6364136223846793005ULL + 1; a[i] = (int)(s >> 33); }

    double t0 = agora();
    msort(a, tmp, 0, n);
    double t1 = agora();

    bool ok = true;
    for (int i = 1; i < n; i++) if (a[i-1] > a[i]) { ok = false; break; }
    printf("n=%d  ordenado=%s  tempo=%.6f s\n", n, ok ? "SIM" : "NAO", t1 - t0);
    return 0;
}