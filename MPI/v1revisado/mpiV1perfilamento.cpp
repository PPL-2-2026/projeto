/* Baseline 07 — Mergesort (divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * MPI V1 + OpenMP: recursao paralela; merge sequencial do baseline.
 * Fase 1: tempo de parede sem instrumentacao interna.
 * Fase 2: merges por nivel e diagnostico de comunicacao/espera MPI.
 *
 * Compilar: mpicxx -O2 -fopenmp -o mpiV1perfilamento mpiV1perfilamento.cpp
 * Executar: OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpiV1perfilamento 20000000 10 2 2160
 */
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"
#include "../../utils/perfilamento.h"

using namespace std;

enum { TAG_CAB = 0, TAG_DADOS = 1, TAG_RESULTADO = 2 };
static constexpr int LIMIAR_TAREFA_OPENMP = 2160;
static int limite_perfilamento = 2160;
static vector<Nivel> niveis;

struct CustosMPI {
    double envio_cabecalho = 0.0;
    double inicio_envio_dados = 0.0;
    double espera_envio = 0.0;
    double espera_resultado = 0.0;
    double recebimento_cabecalho = 0.0;
    double recebimento_dados = 0.0;
    double envio_resultado = 0.0;
    double merge_mpi = 0.0;
    double ordenacao_local = 0.0;
};
static CustosMPI custos;

static bool ler_inteiro(const char* texto, int& valor) {
    errno = 0;
    char* fim = nullptr;
    long numero = strtol(texto, &fim, 10);
    if (errno == ERANGE || fim == texto || *fim != '\0' || numero < INT_MIN || numero > INT_MAX)
        return false;
    valor = static_cast<int>(numero);
    return true;
}

static void ordenar_com_openmp(vector<int>& vetor, vector<int>& auxiliar,
                               int inicio, int fim, int orcamento_paralelismo) {
    const int tamanho = fim - inicio;
    if (tamanho <= 1) return;
    if (orcamento_paralelismo <= 1 || tamanho < LIMIAR_TAREFA_OPENMP) {
        baseline_msort(vetor, auxiliar, inicio, fim);
        return;
    }
    const int meio = inicio + tamanho / 2;
    const int orcamento_esquerda = orcamento_paralelismo / 2;
    #pragma omp task shared(vetor, auxiliar) firstprivate(inicio, meio, orcamento_esquerda)
    ordenar_com_openmp(vetor, auxiliar, inicio, meio, orcamento_esquerda);
    ordenar_com_openmp(vetor, auxiliar, meio, fim, orcamento_paralelismo - orcamento_esquerda);
    #pragma omp taskwait
    baseline_merge(vetor, auxiliar, inicio, meio, fim);
}

static void ordenar_com_mpi(vector<int>& vetor, vector<int>& auxiliar,
                            int inicio, int fim, int processos_disponiveis,
                            int rank_responsavel, int quantidade_threads) {
    if (processos_disponiveis <= 1) {
        #pragma omp parallel
        {
            #pragma omp single
            ordenar_com_openmp(vetor, auxiliar, inicio, fim, quantidade_threads);
        }
        return;
    }
    const int meio = inicio + (fim - inicio) / 2;
    const int processos_esquerda = processos_disponiveis / 2;
    const int rank_auxiliar = rank_responsavel + (processos_disponiveis - processos_esquerda);
    int cabecalho[3] = { inicio, meio, processos_esquerda };
    MPI_Send(cabecalho, 3, MPI_INT, rank_auxiliar, TAG_CAB, MPI_COMM_WORLD);
    MPI_Request requisicao;
    MPI_Isend(vetor.data() + inicio, meio - inicio, MPI_INT,
              rank_auxiliar, TAG_DADOS, MPI_COMM_WORLD, &requisicao);
    ordenar_com_mpi(vetor, auxiliar, meio, fim,
                    processos_disponiveis - processos_esquerda, rank_responsavel, quantidade_threads);
    MPI_Wait(&requisicao, MPI_STATUS_IGNORE);
    MPI_Recv(vetor.data() + inicio, meio - inicio, MPI_INT,
             rank_auxiliar, TAG_RESULTADO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    baseline_merge(vetor, auxiliar, inicio, meio, fim);
}

static void executar_processo_auxiliar(vector<int>& vetor, vector<int>& auxiliar,
                                      int rank_atual, int quantidade_threads) {
    int cabecalho[3];
    MPI_Status estado;
    MPI_Recv(cabecalho, 3, MPI_INT, MPI_ANY_SOURCE, TAG_CAB, MPI_COMM_WORLD, &estado);
    const int rank_pai = estado.MPI_SOURCE;
    const int inicio = cabecalho[0], fim = cabecalho[1], processos_disponiveis = cabecalho[2];
    MPI_Recv(vetor.data() + inicio, fim - inicio, MPI_INT,
             rank_pai, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    ordenar_com_mpi(vetor, auxiliar, inicio, fim, processos_disponiveis, rank_atual, quantidade_threads);
    MPI_Send(vetor.data() + inicio, fim - inicio, MPI_INT,
             rank_pai, TAG_RESULTADO, MPI_COMM_WORLD);
}

// Medicoes dos merges grandes: o tempo de cada merge e registrado por thread.
static void merge_instrumentado(vector<int>& vetor, vector<int>& auxiliar,
                               int inicio, int meio, int fim, int nivel) {
    const double inicio_intercalacao = agora();
    intercala(vetor, auxiliar, inicio, meio, meio, fim, inicio);
    const double fim_intercalacao = agora();
    for (int indice = inicio; indice < fim; ++indice) vetor[indice] = auxiliar[indice];
    const double fim_copia = agora();
    #pragma omp critical(registro_merges)
    registrar(niveis, nivel, fim - inicio,
              fim_intercalacao - inicio_intercalacao, fim_copia - fim_intercalacao);
}

static void ordenar_sequencial_instrumentado(vector<int>& vetor, vector<int>& auxiliar,
                                            int inicio, int fim, int nivel) {
    if (fim - inicio <= 1) return;
    if (fim - inicio < limite_perfilamento) {
        baseline_msort(vetor, auxiliar, inicio, fim);
        return;
    }
    const int meio = inicio + (fim - inicio) / 2;
    ordenar_sequencial_instrumentado(vetor, auxiliar, inicio, meio, nivel + 1);
    ordenar_sequencial_instrumentado(vetor, auxiliar, meio, fim, nivel + 1);
    merge_instrumentado(vetor, auxiliar, inicio, meio, fim, nivel);
}

static void ordenar_openmp_instrumentado(vector<int>& vetor, vector<int>& auxiliar,
                                        int inicio, int fim, int orcamento_paralelismo, int nivel) {
    const int tamanho = fim - inicio;
    if (tamanho <= 1) return;
    if (orcamento_paralelismo <= 1 || tamanho < LIMIAR_TAREFA_OPENMP) {
        ordenar_sequencial_instrumentado(vetor, auxiliar, inicio, fim, nivel);
        return;
    }
    const int meio = inicio + tamanho / 2;
    const int orcamento_esquerda = orcamento_paralelismo / 2;
    #pragma omp task shared(vetor, auxiliar) firstprivate(inicio, meio, orcamento_esquerda, nivel)
    ordenar_openmp_instrumentado(vetor, auxiliar, inicio, meio, orcamento_esquerda, nivel + 1);
    ordenar_openmp_instrumentado(vetor, auxiliar, meio, fim,
                                orcamento_paralelismo - orcamento_esquerda, nivel + 1);
    #pragma omp taskwait
    if (tamanho >= limite_perfilamento)
        merge_instrumentado(vetor, auxiliar, inicio, meio, fim, nivel);
    else
        baseline_merge(vetor, auxiliar, inicio, meio, fim);
}

static void ordenar_mpi_instrumentado(vector<int>& vetor, vector<int>& auxiliar,
                                     int inicio, int fim, int processos_disponiveis,
                                     int nivel, int rank_responsavel, int quantidade_threads) {
    if (processos_disponiveis <= 1) {
        const double inicio_ordenacao = MPI_Wtime();
        #pragma omp parallel
        {
            #pragma omp single
            ordenar_openmp_instrumentado(vetor, auxiliar, inicio, fim, quantidade_threads, nivel);
        }
        custos.ordenacao_local += MPI_Wtime() - inicio_ordenacao;
        return;
    }
    const int meio = inicio + (fim - inicio) / 2;
    const int processos_esquerda = processos_disponiveis / 2;
    const int rank_auxiliar = rank_responsavel + (processos_disponiveis - processos_esquerda);
    int cabecalho[4] = { inicio, meio, processos_esquerda, nivel + 1 };

    double inicio_medicao = MPI_Wtime();
    MPI_Send(cabecalho, 4, MPI_INT, rank_auxiliar, TAG_CAB, MPI_COMM_WORLD);
    custos.envio_cabecalho += MPI_Wtime() - inicio_medicao;

    MPI_Request requisicao;
    inicio_medicao = MPI_Wtime();
    MPI_Isend(vetor.data() + inicio, meio - inicio, MPI_INT,
              rank_auxiliar, TAG_DADOS, MPI_COMM_WORLD, &requisicao);
    custos.inicio_envio_dados += MPI_Wtime() - inicio_medicao;

    ordenar_mpi_instrumentado(vetor, auxiliar, meio, fim,
                             processos_disponiveis - processos_esquerda,
                             nivel + 1, rank_responsavel, quantidade_threads);

    inicio_medicao = MPI_Wtime();
    MPI_Wait(&requisicao, MPI_STATUS_IGNORE);
    custos.espera_envio += MPI_Wtime() - inicio_medicao;

    inicio_medicao = MPI_Wtime();
    MPI_Recv(vetor.data() + inicio, meio - inicio, MPI_INT,
             rank_auxiliar, TAG_RESULTADO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    custos.espera_resultado += MPI_Wtime() - inicio_medicao;

    inicio_medicao = MPI_Wtime();
    if (fim - inicio >= limite_perfilamento)
        merge_instrumentado(vetor, auxiliar, inicio, meio, fim, nivel);
    else
        baseline_merge(vetor, auxiliar, inicio, meio, fim);
    custos.merge_mpi += MPI_Wtime() - inicio_medicao;
}

static void executar_auxiliar_instrumentado(vector<int>& vetor, vector<int>& auxiliar,
                                           int rank_atual, int quantidade_threads) {
    int cabecalho[4];
    MPI_Status estado;
    double inicio_medicao = MPI_Wtime();
    MPI_Recv(cabecalho, 4, MPI_INT, MPI_ANY_SOURCE, TAG_CAB, MPI_COMM_WORLD, &estado);
    custos.recebimento_cabecalho += MPI_Wtime() - inicio_medicao;
    const int rank_pai = estado.MPI_SOURCE;
    const int inicio = cabecalho[0], fim = cabecalho[1];
    const int processos_disponiveis = cabecalho[2], nivel = cabecalho[3];

    inicio_medicao = MPI_Wtime();
    MPI_Recv(vetor.data() + inicio, fim - inicio, MPI_INT,
             rank_pai, TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    custos.recebimento_dados += MPI_Wtime() - inicio_medicao;

    ordenar_mpi_instrumentado(vetor, auxiliar, inicio, fim, processos_disponiveis,
                             nivel, rank_atual, quantidade_threads);

    inicio_medicao = MPI_Wtime();
    MPI_Send(vetor.data() + inicio, fim - inicio, MPI_INT,
             rank_pai, TAG_RESULTADO, MPI_COMM_WORLD);
    custos.envio_resultado += MPI_Wtime() - inicio_medicao;
}

static void reunir_niveis(int rank) {
    const int niveis_locais = static_cast<int>(niveis.size());
    int quantidade_niveis = 0;
    MPI_Allreduce(&niveis_locais, &quantidade_niveis, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    vector<double> sort_local(quantidade_niveis, 0.0), copia_local(quantidade_niveis, 0.0);
    vector<double> merges_local(quantidade_niveis, 0.0), tamanho_local(quantidade_niveis, 0.0);
    vector<double> maior_local(quantidade_niveis, 0.0);
    for (int nivel = 0; nivel < niveis_locais; ++nivel) {
        sort_local[nivel] = niveis[nivel].sort;
        copia_local[nivel] = niveis[nivel].copia;
        merges_local[nivel] = static_cast<double>(niveis[nivel].merges);
        tamanho_local[nivel] = niveis[nivel].tamanho;
        maior_local[nivel] = niveis[nivel].maior;
    }
    vector<double> sort_global(quantidade_niveis), copia_global(quantidade_niveis);
    vector<double> merges_global(quantidade_niveis), tamanho_global(quantidade_niveis);
    vector<double> maior_global(quantidade_niveis);
    if (quantidade_niveis > 0) {
        MPI_Reduce(sort_local.data(), sort_global.data(), quantidade_niveis, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(copia_local.data(), copia_global.data(), quantidade_niveis, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(merges_local.data(), merges_global.data(), quantidade_niveis, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(tamanho_local.data(), tamanho_global.data(), quantidade_niveis, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(maior_local.data(), maior_global.data(), quantidade_niveis, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }
    if (rank == 0) {
        niveis.resize(quantidade_niveis);
        for (int nivel = 0; nivel < quantidade_niveis; ++nivel) {
            niveis[nivel].sort = sort_global[nivel];
            niveis[nivel].copia = copia_global[nivel];
            niveis[nivel].merges = static_cast<long long>(merges_global[nivel]);
            niveis[nivel].tamanho = static_cast<int>(tamanho_global[nivel]);
            niveis[nivel].maior = maior_global[nivel];
        }
    }
}

static void exibir_custos_mpi(int rank, int repeticoes) {
    const double locais[] = {
        custos.envio_cabecalho, custos.inicio_envio_dados, custos.espera_envio,
        custos.espera_resultado, custos.recebimento_cabecalho, custos.recebimento_dados,
        custos.envio_resultado, custos.merge_mpi, custos.ordenacao_local
    };
    constexpr int QUANTIDADE_MEDIDAS = sizeof(locais) / sizeof(locais[0]);
    double somas[QUANTIDADE_MEDIDAS] = {};
    double maximos[QUANTIDADE_MEDIDAS] = {};
    MPI_Reduce(locais, somas, QUANTIDADE_MEDIDAS, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(locais, maximos, QUANTIDADE_MEDIDAS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (rank != 0) return;
    const char* nomes[QUANTIDADE_MEDIDAS] = {
        "Envio de cabecalho", "Inicio de MPI_Isend", "Espera MPI_Wait",
        "Espera MPI_Recv (resultado)", "Recebimento de cabecalho",
        "Recebimento de dados", "Envio de resultado", "Merge nos nos MPI", "Ordenacao local OpenMP"
    };
    printf("\n[Diagnostico MPI] tempos por repeticao (segundos)\n");
    printf("  %-31s %13s %13s\n", "Operacao", "Soma ranks", "Maior rank");
    for (int indice = 0; indice < QUANTIDADE_MEDIDAS; ++indice)
        printf("  %-31s %13.6f %13.6f\n", nomes[indice],
               somas[indice] / repeticoes, maximos[indice] / repeticoes);
    printf("  OBS: tempos podem se sobrepor; nao somar para obter wall time.\n");
    printf("  OBS: MPI_Recv mede tempo bloqueado, incluindo espera por trabalho remoto.\n");
    printf("  OBS: merge MPI tambem aparece na tabela de merges por nivel.\n");
}

static double executar_fase1(vector<int>& vetor, vector<int>& auxiliar,
                            const vector<int>& referencia, int quantidade_processos,
                            int quantidade_threads, int rank, int repeticoes, int aquecimentos) {
    const int tamanho_vetor = static_cast<int>(vetor.size());
    double soma_tempos = 0.0;
    bool todas_corretas = true;
    for (int repeticao = 0; repeticao < aquecimentos + repeticoes; ++repeticao) {
        if (rank != 0) {
            executar_processo_auxiliar(vetor, auxiliar, rank, quantidade_threads);
            continue;
        }
        gerar_vetor(vetor);
        const double inicio = agora();
        ordenar_com_mpi(vetor, auxiliar, 0, tamanho_vetor,
                        quantidade_processos, 0, quantidade_threads);
        const double duracao = agora() - inicio;
        if (repeticao >= aquecimentos) {
            soma_tempos += duracao;
            todas_corretas = todas_corretas && (vetor == referencia);
        }
    }
    if (rank != 0) return 0.0;
    const double media = soma_tempos / repeticoes;
    printf("\n[Fase 1] tempo por ordenacao=%.6f s  todas iguais ao baseline=%s\n",
           media, todas_corretas ? "SIM" : "NAO");
    return media;
}

static void executar_fase2(vector<int>& vetor, vector<int>& auxiliar,
                           const vector<int>& referencia, int quantidade_processos,
                           int quantidade_threads, int rank, int repeticoes,
                           int aquecimentos, double tempo_fase1) {
    const int tamanho_vetor = static_cast<int>(vetor.size());
    double tempo_total = 0.0;
    bool todas_corretas = true;
    for (int repeticao = 0; repeticao < aquecimentos + repeticoes; ++repeticao) {
        if (repeticao == aquecimentos) {
            niveis.clear();
            custos = CustosMPI{};
            tempo_total = 0.0;
        }
        if (rank != 0) {
            executar_auxiliar_instrumentado(vetor, auxiliar, rank, quantidade_threads);
            continue;
        }
        gerar_vetor(vetor);
        const double inicio = agora();
        ordenar_mpi_instrumentado(vetor, auxiliar, 0, tamanho_vetor,
                                 quantidade_processos, 0, 0, quantidade_threads);
        const double duracao = agora() - inicio;
        if (repeticao >= aquecimentos) {
            tempo_total += duracao;
            todas_corretas = todas_corretas && (vetor == referencia);
        }
    }
    reunir_niveis(rank);
    exibir_custos_mpi(rank, repeticoes);
    if (rank != 0) return;
    const double media = tempo_total / repeticoes;
    printf("\n[Fase 2] merges instrumentados (media de %d repeticoes)  todas iguais ao baseline=%s\n",
           repeticoes, todas_corretas ? "SIM" : "NAO");
    printf("  %5s %10s %7s %11s %11s %11s %11s\n",
           "nivel", "tamanho", "merges", "sort", "copia", "total", "maior");
    double soma_sort = 0.0, soma_copia = 0.0;
    for (size_t nivel = 0; nivel < niveis.size(); ++nivel) {
        const double tempo_sort = niveis[nivel].sort / repeticoes;
        const double tempo_copia = niveis[nivel].copia / repeticoes;
        const double tempo_merge = tempo_sort + tempo_copia;
        soma_sort += tempo_sort;
        soma_copia += tempo_copia;
        printf("  %5zu %10d %7lld %10.6fs %10.6fs %10.6fs %10.6fs\n",
               nivel, niveis[nivel].tamanho, niveis[nivel].merges / repeticoes,
               tempo_sort, tempo_copia, tempo_merge, niveis[nivel].maior);
    }
    const double soma_merges = soma_sort + soma_copia;
    const double percentual_sort = soma_merges > 0.0 ? 100.0 * soma_sort / soma_merges : 0.0;
    const double percentual_copia = soma_merges > 0.0 ? 100.0 * soma_copia / soma_merges : 0.0;
    const double sobrecarga = tempo_fase1 > 0.0 ? 100.0 * (media - tempo_fase1) / tempo_fase1 : 0.0;
    printf("\n  merges grandes, somando threads e processos: %.6f s\n", soma_merges);
    printf("    intercalacao: %.6f s (%.1f%% dos merges medidos)\n", soma_sort, percentual_sort);
    printf("    copia de volta: %.6f s (%.1f%% dos merges medidos)\n", soma_copia, percentual_copia);
    printf("  total instrumentado: %.6f s (diferenca vs Fase 1: %+.1f%%)\n", media, sobrecarga);
    printf("  OBS: soma de trabalho paralelo nao equivale ao tempo de parede.\n");
}

int main(int argc, char** argv) {
    int suporte_threads = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &suporte_threads);
    int rank = 0, quantidade_processos = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &quantidade_processos);
    if (suporte_threads < MPI_THREAD_FUNNELED) {
        if (rank == 0) fprintf(stderr, "Erro: MPI_THREAD_FUNNELED indisponivel.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int tamanho_vetor = 20000000, repeticoes = 10, aquecimentos = 2;
    bool parametros_validos = argc <= 5;
    if (argc > 1) parametros_validos = ler_inteiro(argv[1], tamanho_vetor) && parametros_validos;
    if (argc > 2) parametros_validos = ler_inteiro(argv[2], repeticoes) && parametros_validos;
    if (argc > 3) parametros_validos = ler_inteiro(argv[3], aquecimentos) && parametros_validos;
    if (argc > 4) parametros_validos = ler_inteiro(argv[4], limite_perfilamento) && parametros_validos;
    if (!parametros_validos || tamanho_vetor <= 0 || repeticoes <= 0 || aquecimentos < 0 ||
        limite_perfilamento <= 0 || quantidade_processos > tamanho_vetor ||
        repeticoes > INT_MAX - aquecimentos) {
        if (rank == 0)
            fprintf(stderr, "Erro: use n>0, reps>0, aquec>=0, corte>0 e processos<=n.\n");
        MPI_Finalize();
        return 1;
    }
    const int quantidade_threads = omp_get_max_threads();
    vector<int> vetor(tamanho_vetor), auxiliar(tamanho_vetor);
    vector<int> referencia;
    if (rank == 0) {
        referencia.resize(tamanho_vetor);
        gerar_vetor(referencia);
        baseline_msort(referencia, auxiliar, 0, tamanho_vetor);
        printf("n=%d (vetor + auxiliar = %.1f MiB por processo) procs=%d threads=%d reps=%d aquecimento=%d corte=%d limiar_tasks=%d\n",
               tamanho_vetor, 2.0 * tamanho_vetor * sizeof(int) / (1024.0 * 1024.0),
               quantidade_processos, quantidade_threads, repeticoes, aquecimentos,
               limite_perfilamento, LIMIAR_TAREFA_OPENMP);
    }
    const double tempo_fase1 = executar_fase1(vetor, auxiliar, referencia,
                                             quantidade_processos, quantidade_threads,
                                             rank, repeticoes, aquecimentos);
    if (tamanho_vetor < limite_perfilamento) {
        if (rank == 0) printf("\n[Fase 2] ignorada: n menor que o corte (%d)\n", limite_perfilamento);
        MPI_Finalize();
        return 0;
    }
    executar_fase2(vetor, auxiliar, referencia, quantidade_processos,
                   quantidade_threads, rank, repeticoes, aquecimentos, tempo_fase1);
    MPI_Finalize();
    return 0;
}
