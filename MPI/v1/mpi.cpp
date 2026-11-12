
/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 1 (MPI + OpenMP): paraleliza só a recursão (msort). O merge é o do baseline, sem alteração.
 * O MPI divide o vetor entre os processos e, dentro de cada processo, a recursão
 * roda com as tasks do OpenMP (o msort_paralelo do openmp.cpp, com proteção para trechos pequenos).
 * A versão com medições está em mpiV1perfilamento.cpp.
 *
 * Compilar: mpicxx -O2 -fopenmp -o mpi mpi.cpp
 * Executar: OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpi 20000000
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"
using namespace std;

enum { TAG_CAB = 0, TAG_DADOS = 1, TAG_RESULTADO = 2 };

/* ---------- OpenMP: igual ao openmp.cpp ---------- */

/* "partes" é quantas threads estão disponíveis para este trecho. Ele é
 * dividido ao meio a cada nível; quando chega a 1, o resto da subárvore
 * roda sequencial, com o msort do baseline. */
static void msort_paralelo(vector<int>& a, vector<int>& tmp, int lo, int hi, int partes) {
    // Evita continuar a recursão em trechos vazios ou com apenas um elemento.
    if (hi - lo <= 1) {
        return;
    }

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

    // Espera os dois lados terminarem para iniciar o merge.
    #pragma omp taskwait
    baseline_merge(a, tmp, lo, mid, hi);
}

/* ---------- MPI: divide o trecho entre processos, mesma ideia do OpenMP ---------- */

/* "partes" é quantos processos estão disponíveis para este trecho. Ele é
 * dividido ao meio a cada nível; quando chega a 1, o processo ordena o trecho
 * com o msort_paralelo do OpenMP ("threads" threads). "meu" é o rank deste
 * processo (o primeiro dos "partes" processos deste trecho). Todo processo tem
 * os vetores a e tmp inteiros e trabalha nos mesmos índices lo..hi. */
static void msort_mpi(vector<int>& a, vector<int>& tmp, int lo, int hi, int partes, int meu, int threads) {
    if (partes <= 1) {
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo(a, tmp, lo, hi, threads);
            }
        }
        return;
    }

    int mid = (lo + hi) / 2;
    int outro = meu + (partes - partes / 2); // Primeiro processo do segundo grupo.

    /* A metade esquerda vira tarefa (outro processo executa) e a direita é feita
     * pelo processo atual, ao mesmo tempo. */
    int cab[3] = { lo, mid, partes / 2 };

    MPI_Send(cab, 3, MPI_INT, outro, TAG_CAB, MPI_COMM_WORLD);

    MPI_Request req;
    MPI_Isend(a.data() + lo, mid - lo, MPI_INT, outro, TAG_DADOS, MPI_COMM_WORLD, &req);

    msort_mpi(a, tmp, mid, hi, partes - partes / 2, meu, threads);

    // Espera o envio terminar e recebe a metade esquerda já ordenada.
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    MPI_Recv(a.data() + lo, mid - lo, MPI_INT, outro, TAG_RESULTADO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // O merge continua sequencial, utilizando a função original do baseline.
    baseline_merge(a, tmp, lo, mid, hi);
}

/* Processos diferentes do 0: esperam um trecho, ordenam e devolvem a quem mandou. */
static void ajudante(vector<int>& a, vector<int>& tmp, int meu, int threads) {
    int cab[3];
    MPI_Status st;

    MPI_Recv(cab, 3, MPI_INT, MPI_ANY_SOURCE, TAG_CAB, MPI_COMM_WORLD, &st);

    int pai = st.MPI_SOURCE;
    int lo = cab[0], hi = cab[1], partes = cab[2];

    MPI_Recv(a.data() + lo, hi - lo, MPI_INT, pai, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    msort_mpi(a, tmp, lo, hi, partes, meu, threads);

    MPI_Send(a.data() + lo, hi - lo, MPI_INT, pai, TAG_RESULTADO, MPI_COMM_WORLD);
}

int main(int argc, char** argv) {
    int provided;

    // Inicializa o MPI: somente a thread principal realiza chamadas MPI.
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);

    // Verifica se o MPI oferece o nível de suporte a threads solicitado.
    if (provided < MPI_THREAD_FUNNELED) {
        if (rank == 0) {
            fprintf(stderr, "Erro: suporte MPI_THREAD_FUNNELED indisponivel.\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int n = (argc > 1) ? atoi(argv[1]) : 20000000;

    // Evita criar vetores com tamanho inválido.
    if (n <= 0) {
        if (rank == 0) {
            fprintf(stderr, "Erro: o tamanho do vetor deve ser positivo.\n");
        }
        MPI_Finalize();
        return 1;
    }

    vector<int> a(n), tmp(n);

    if (rank == 0) {
        gerar_vetor(a);
    }

    int threads = omp_get_max_threads();

    // Aguarda todos os processos terminarem a alocação dos vetores.
    MPI_Barrier(MPI_COMM_WORLD);

    double t0 = agora();

    if (rank == 0) {
        msort_mpi(a, tmp, 0, n, procs, 0, threads);
    } else {
        ajudante(a, tmp, rank, threads);
    }

    double t1 = agora();

    if (rank == 0) {
        bool ok = true;

        for (int i = 1; i < n; i++) {
            if (a[i - 1] > a[i]) {
                ok = false;
                break;
            }
        }

        printf("n=%d  procs=%d  threads=%d  ordenado=%s  tempo=%.6f s\n",
               n, procs, threads, ok ? "SIM" : "NAO", t1 - t0);
    }

    MPI_Finalize();
    return 0;
}
