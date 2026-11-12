/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 1 (MPI + OpenMP): paraleliza só a recursão (msort). O merge é o do baseline, sem alteração.
 * O MPI divide o vetor entre os processos e, dentro de cada processo, a recursão
 * roda com as tasks do OpenMP (igual ao openmpV1Perfilamento.cpp).
 * Protecoes para trechos pequenos e validacao dos parametros de execucao.
 * Mede com o mesmo protocolo do perfilamento do baseline:
 *   Fase 1: tempo de parede da versão paralela, validado contra o baseline
 *   Fase 2: onde o tempo é gasto, por nível (sort e cópia de cada merge)
 *
 * Compilar: mpicxx -O2 -fopenmp -o mpiV1perfilamento mpiV1perfilamento.cpp
 * Executar: OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpiV1perfilamento [n=20000000] [reps=10] [aquec=2] [corte=2160]
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"
#include "../../utils/perfilamento.h"
using namespace std;

enum { TAG_CAB = 0, TAG_DADOS = 1, TAG_RESULTADO = 2 };

/* ---------- OpenMP: versão paralela (igual ao openmp.cpp) ---------- */

/* "partes" é quantas threads estão disponíveis para este trecho. Ele é
 * dividido ao meio a cada nível; quando chega a 1, o resto da subárvore
 * roda sequencial, com o msort do baseline. */
static void msort_paralelo(vector<int>& a, vector<int>& tmp, int lo, int hi, int partes) {
    // Trechos vazios ou unitarios ja estao ordenados.
    if (hi - lo <= 1) return;

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
    int outro = meu + (partes - partes / 2); // primeiro processo do segundo grupo
    /* A metade esquerda vira tarefa (outro processo executa) e a direita é feita
     * pelo processo atual, ao mesmo tempo. */
    int cab[3] = { lo, mid, partes / 2 };
    MPI_Send(cab, 3, MPI_INT, outro, TAG_CAB, MPI_COMM_WORLD);
    MPI_Request req;
    MPI_Isend(a.data() + lo, mid - lo, MPI_INT, outro, TAG_DADOS, MPI_COMM_WORLD, &req);
    msort_mpi(a, tmp, mid, hi, partes - partes / 2, meu, threads);

    // espera os dois lados terminarem pra iniciar o merge
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    MPI_Recv(a.data() + lo, mid - lo, MPI_INT, outro, TAG_RESULTADO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    baseline_merge(a, tmp, lo, mid, hi);
}

/* Processos diferentes do 0: esperam um trecho, ordenam e devolvem a quem mandou. */
static void ajudante(vector<int>& a, vector<int>& tmp, int meu, int threads) {
    int cab[3];
    MPI_Status st;
    MPI_Recv(cab, 3, MPI_INT, MPI_ANY_SOURCE, TAG_CAB, MPI_COMM_WORLD, &st);
    int pai = st.MPI_SOURCE, lo = cab[0], hi = cab[1], partes = cab[2];
    MPI_Recv(a.data() + lo, hi - lo, MPI_INT, pai, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    msort_mpi(a, tmp, lo, hi, partes, meu, threads);
    MPI_Send(a.data() + lo, hi - lo, MPI_INT, pai, TAG_RESULTADO, MPI_COMM_WORLD);
}

/* ---------- versão instrumentada (mesmo protocolo do perfilamento.cpp) ---------- */
// Mesmo corte do perfilamento: merges menores que isso não são cronometrados.
static int CORTE = 2160;
static vector<Nivel> niveis; // cada processo registra os merges que ele mesmo executou

// Merge do baseline cronometrado: igual ao perfilamento do baseline.
static void merge_perfilamento(vector<int>& a, vector<int>& tmp, int lo, int mid, int hi, int nivel) {
    double t0 = agora();
    intercala(a, tmp, lo, mid, mid, hi, lo);
    double t1 = agora();
    for (int t = lo; t < hi; t++) a[t] = tmp[t];
    double t2 = agora();
    // várias threads atualizam a mesma tabela: cinco campos que mudam juntos
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

// OpenMP instrumentado: igual ao msort_paralelo_perfilamento do openmpV1Perfilamento.cpp
static void msort_paralelo_perfilamento(vector<int>& a, vector<int>& tmp,
                                        int lo, int hi, int partes, int nivel) {
    // Evita criar tasks para trechos vazios ou unitarios.
    if (hi - lo <= 1) return;

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

// MPI instrumentado: o nível continua contando a partir dos níveis do MPI
static void msort_mpi_perfilamento(vector<int>& a, vector<int>& tmp, int lo, int hi,
                                   int partes, int nivel, int meu, int threads) {
    if (partes <= 1) {
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo_perfilamento(a, tmp, lo, hi, threads, nivel);
            }
        }
        return;
    }
    int mid = (lo + hi) / 2;
    int outro = meu + (partes - partes / 2);
    int cab[4] = { lo, mid, partes / 2, nivel + 1 };
    MPI_Send(cab, 4, MPI_INT, outro, TAG_CAB, MPI_COMM_WORLD);
    MPI_Request req;
    MPI_Isend(a.data() + lo, mid - lo, MPI_INT, outro, TAG_DADOS, MPI_COMM_WORLD, &req);
    msort_mpi_perfilamento(a, tmp, mid, hi, partes - partes / 2, nivel + 1, meu, threads);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    MPI_Recv(a.data() + lo, mid - lo, MPI_INT, outro, TAG_RESULTADO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    merge_perfilamento(a, tmp, lo, mid, hi, nivel); // merge do baseline, num processo só
}

static void ajudante_perfilamento(vector<int>& a, vector<int>& tmp, int meu, int threads) {
    int cab[4];
    MPI_Status st;
    MPI_Recv(cab, 4, MPI_INT, MPI_ANY_SOURCE, TAG_CAB, MPI_COMM_WORLD, &st);
    int pai = st.MPI_SOURCE, lo = cab[0], hi = cab[1], partes = cab[2], nivel = cab[3];
    MPI_Recv(a.data() + lo, hi - lo, MPI_INT, pai, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    msort_mpi_perfilamento(a, tmp, lo, hi, partes, nivel, meu, threads);
    MPI_Send(a.data() + lo, hi - lo, MPI_INT, pai, TAG_RESULTADO, MPI_COMM_WORLD);
}

/* Junta no rank 0 as tabelas de todos os processos: soma merges, sort e cópia; maior e tamanho = máximo. */
static void juntar_niveis(int rank) {
    int local = (int)niveis.size(), total;
    MPI_Allreduce(&local, &total, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    vector<double> s(total, 0.0), c(total, 0.0), m(total, 0.0), t(total, 0.0), mx(total, 0.0);
    for (int d = 0; d < local; d++) {
        s[d] = niveis[d].sort; c[d] = niveis[d].copia; m[d] = (double)niveis[d].merges;
        t[d] = niveis[d].tamanho; mx[d] = niveis[d].maior;
    }
    vector<double> rs(total), rc(total), rm(total), rt(total), rmx(total);
    if (total > 0) {
        MPI_Reduce(s.data(),  rs.data(),  total, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(c.data(),  rc.data(),  total, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(m.data(),  rm.data(),  total, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(t.data(),  rt.data(),  total, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(mx.data(), rmx.data(), total, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }
    if (rank == 0) {
        niveis.resize(total);
        for (int d = 0; d < total; d++) {
            niveis[d].sort = rs[d]; niveis[d].copia = rc[d]; niveis[d].merges = (long long)rm[d];
            niveis[d].tamanho = (int)rt[d]; niveis[d].maior = rmx[d];
        }
    }
}

/* ---------- protocolo de medição ---------- */

/* ---- Fase 1: wall time da versão paralela, validado contra o baseline ---- */
static double fase1(vector<int>& a, vector<int>& tmp, const vector<int>& referencia,
                    int procs, int threads, int rank, int reps, int aquec) {
    int n = (int)a.size();
    double tempo_medio = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        if (rank != 0) { ajudante(a, tmp, rank, threads); continue; }
        // Toda repetição usa o mesmo vetor inicial
        gerar_vetor(a);
        double t0 = agora();
        msort_mpi(a, tmp, 0, n, procs, 0, threads);
        double dt = agora() - t0;
        // Aquecimento é descartado
        if (r >= aquec) tempo_medio += dt;
    }
    if (rank != 0) return 0.0;
    tempo_medio /= reps; // média das repetições
    // validação: o resultado tem que ser idêntico, elemento a elemento, ao do baseline
    bool igual = (a == referencia);
    printf("\n[Fase 1] tempo por ordenacao=%.6f s  igual ao baseline=%s\n", tempo_medio, igual ? "SIM" : "NAO");
    return tempo_medio;
}

/* ---- Fase 2: onde o tempo é gasto (média das repetições) ---- */
static void fase2(vector<int>& a, vector<int>& tmp, const vector<int>& referencia,
                  int procs, int threads, int rank, int reps, int aquec, double tempo_fase1) {
    int n = (int)a.size();
    double total_perfilamento = 0.0;
    for (int r = 0; r < aquec + reps; r++) {
        if (r == aquec) { // descarta o aquecimento
            niveis.clear(); total_perfilamento = 0.0;
        }
        if (rank != 0) { ajudante_perfilamento(a, tmp, rank, threads); continue; }
        gerar_vetor(a); // mesma entrada a cada repetição
        double t0 = agora();
        msort_mpi_perfilamento(a, tmp, 0, n, procs, 0, 0, threads);
        total_perfilamento += agora() - t0;
    }
    juntar_niveis(rank);
    if (rank != 0) return;
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
    double percentual_sort = (soma_merges > 0.0) ? 100.0 * soma_sort / soma_merges : 0.0;
    double percentual_copia = (soma_merges > 0.0) ? 100.0 * soma_copia / soma_merges : 0.0;
    double custo_instrumentacao = (tempo_fase1 > 0.0) ? 100.0 * (media - tempo_fase1) / tempo_fase1 : 0.0;

    // os merges de um mesmo nível rodam ao mesmo tempo, então as somas são tempo de trabalho das threads e processos
    printf("\n  merges grandes, somando as threads e processos: %.6f s\n", soma_merges);
    printf("    sort:                        %.6f s (%5.1f%% dos merges grandes)\n", soma_sort, percentual_sort);
    printf("    copia de volta:              %.6f s (%5.1f%% dos merges grandes)\n", soma_copia, percentual_copia);
    printf("  total instrumentado: %.6f s  (custo da instrumentacao: %+.1f%% vs Fase 1)\n", media, custo_instrumentacao);
}

int main(int argc, char** argv) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided); // MPI so e chamado fora das regioes paralelas
    int rank, procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);

    // Confirma que a implementacao MPI oferece o suporte a threads solicitado.
    if (provided < MPI_THREAD_FUNNELED) {
        if (rank == 0) fprintf(stderr, "Erro: suporte MPI_THREAD_FUNNELED indisponivel.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int n = (argc > 1) ? atoi(argv[1]) : 20000000;
    int reps = (argc > 2) ? atoi(argv[2]) : 10;
    int aquec = (argc > 3) ? atoi(argv[3]) : 2;
    CORTE = (argc > 4) ? atoi(argv[4]) : 2160;
    int threads = omp_get_max_threads();

    // Evita alocacoes invalidas e divisoes por zero nas medias do perfilamento.
    if (n <= 0 || reps <= 0 || aquec < 0 || CORTE <= 0) {
        if (rank == 0) {
            fprintf(stderr, "Erro: use n > 0, reps > 0, aquec >= 0 e corte > 0.\n");
        }
        MPI_Finalize();
        return 1;
    }

    vector<int> a(n), tmp(n);

    // resultado de referência: o mesmo vetor ordenado pelo msort do baseline (fora do cronômetro)
    vector<int> referencia;
    if (rank == 0) {
        referencia.resize(n);
        gerar_vetor(referencia);
        baseline_msort(referencia, tmp, 0, n);

        printf("n=%d  (a + tmp = %.1f MiB por processo)  procs=%d  threads=%d  reps=%d  aquecimento=%d  corte=%d\n",
               n, 2.0 * n * sizeof(int) / (1024.0 * 1024.0), procs, threads, reps, aquec, CORTE);
    }

    double tempo_fase1 = fase1(a, tmp, referencia, procs, threads, rank, reps, aquec);
    if (n < CORTE) {
        if (rank == 0) printf("\n[Fase 2] ignorada: n menor que o corte (%d)\n", CORTE);
        MPI_Finalize();
        return 0;
    }
    fase2(a, tmp, referencia, procs, threads, rank, reps, aquec, tempo_fase1);
    MPI_Finalize();
    return 0;
}