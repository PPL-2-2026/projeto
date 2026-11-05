#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <ctime>
#include <omp.h>
using namespace std;

static const int LIMITE_TAREFA = 100000;

static double agora() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

void merge(vector<int>& arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    vector<int> L(n1), R(n2);

    if (n1 + n2 >= LIMITE_TAREFA) {
        // Copia a metade esquerda.
        #pragma omp task shared(arr, L) firstprivate(left, n1)
        {
            for (int i = 0; i < n1; i++)
                L[i] = arr[left + i];
        }

        // Copia a metade direita.
        #pragma omp task shared(arr, R) firstprivate(mid, n2)
        {
            for (int j = 0; j < n2; j++)
                R[j] = arr[mid + 1 + j];
        }

        // Aguarda as duas cópias antes de intercalar.
        #pragma omp taskwait
    } else {
        for (int i = 0; i < n1; i++)
            L[i] = arr[left + i];

        for (int j = 0; j < n2; j++)
            R[j] = arr[mid + 1 + j];
    }

    int i = 0, j = 0;
    int k = left;

    // Intercalação sequencial.
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }

    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }
}

void mergeSort(vector<int>& arr, int left, int right) {
    if (left >= right)
        return;

    int mid = left + (right - left) / 2;

    if (right - left + 1 >= LIMITE_TAREFA) {
        // Ordena as duas metades em tarefas independentes.
        #pragma omp task shared(arr) firstprivate(left, mid)
        {
            mergeSort(arr, left, mid);
        }

        #pragma omp task shared(arr) firstprivate(mid, right)
        {
            mergeSort(arr, mid + 1, right);
        }

        // Aguarda a ordenação das duas metades.
        #pragma omp taskwait
    } else {
        mergeSort(arr, left, mid);
        mergeSort(arr, mid + 1, right);
    }

    merge(arr, left, mid, right);
}

int main(int argc, char** argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 20000000;

    if (n < 0) {
        printf("O tamanho do vetor deve ser nao negativo.\n");
        return 1;
    }

    vector<int> arr(n);

    uint64_t s = 12345;
    for (int i = 0; i < n; i++) {
        s = s * 6364136223846793005ULL + 1;
        arr[i] = (int)(s >> 33);
    }

    double t0 = agora();

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            mergeSort(arr, 0, n - 1);
        }
    }

    double t1 = agora();

    bool ok = true;
    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) {
            ok = false;
            break;
        }
    }

    printf("n=%d  ordenado=%s  tempo=%.6f s\n",
           n, ok ? "SIM" : "NAO", t1 - t0);

    return 0;
}