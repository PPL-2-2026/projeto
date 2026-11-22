
/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versao 1 (MPI + OpenMP):
 * - MPI distribui os trechos entre processos.
 * - OpenMP paraleliza a recursao com tasks.
 * - O merge permanece sequencial, como no baseline.
 *
 * Compilar:
 * mpicxx -O2 -fopenmp -o mpi mpi.cpp
 *
 * Executar:
 * OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpi 20000000
 */

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"

using namespace std;

enum {
    TAG_CABECALHO = 0,
    TAG_DADOS = 1,
    TAG_RESULTADO = 2
};

// Limite minimo para criar novas tarefas OpenMP.
// Valor inicial para experimentacao; deve ser avaliado com benchmarks.
static constexpr int LIMIAR_TAREFA_OPENMP = 2160;

// Converte um argumento textual em inteiro positivo.
static bool ler_tamanho_vetor(const char* argumento, int& tamanho_vetor) {
    if (argumento == nullptr || argumento[0] == '\0') {
        return false;
    }

    errno = 0;
    char* fim_conversao = nullptr;
    long valor = strtol(argumento, &fim_conversao, 10);

    if (errno == ERANGE ||
        fim_conversao == argumento ||
        *fim_conversao != '\0' ||
        valor <= 0 ||
        valor > INT_MAX) {
        return false;
    }

    tamanho_vetor = static_cast<int>(valor);
    return true;
}

/* ---------- OpenMP: ordenacao recursiva ---------- */

// O orcamento de tarefas controla a subdivisao logica.
// Ele nao representa threads reservadas exclusivamente.
static void ordenar_com_openmp(
    vector<int>& vetor,
    vector<int>& vetor_auxiliar,
    int inicio,
    int fim,
    int orcamento_tarefas
) {
    int tamanho_trecho = fim - inicio;

    if (tamanho_trecho <= 1) {
        return;
    }

    // Evita criar tarefas quando a subdivisao nao compensa.
    if (orcamento_tarefas <= 1 ||
        tamanho_trecho < LIMIAR_TAREFA_OPENMP) {
        baseline_msort(vetor, vetor_auxiliar, inicio, fim);
        return;
    }

    int meio = inicio + tamanho_trecho / 2;
    int orcamento_esquerda = orcamento_tarefas / 2;
    int orcamento_direita = orcamento_tarefas - orcamento_esquerda;

    #pragma omp task shared(vetor, vetor_auxiliar)
    {
        ordenar_com_openmp(
            vetor,
            vetor_auxiliar,
            inicio,
            meio,
            orcamento_esquerda
        );
    }

    ordenar_com_openmp(
        vetor,
        vetor_auxiliar,
        meio,
        fim,
        orcamento_direita
    );

    // As duas metades precisam estar ordenadas antes do merge.
    #pragma omp taskwait

    baseline_merge(vetor, vetor_auxiliar, inicio, meio, fim);
}

/* ---------- MPI: distribuicao recursiva ---------- */

// O rank responsavel distribui a metade esquerda e
// processa a metade direita.
//
// Todos os processos utilizam os mesmos indices globais,
// embora cada um trabalhe apenas no trecho recebido.
static void ordenar_com_mpi(
    vector<int>& vetor,
    vector<int>& vetor_auxiliar,
    int inicio,
    int fim,
    int processos_disponiveis,
    int rank_responsavel,
    int quantidade_threads
) {
    int tamanho_trecho = fim - inicio;

    // Quando nao ha processos adicionais, utiliza OpenMP.
    if (processos_disponiveis <= 1) {
        #pragma omp parallel
        {
            #pragma omp single
            {
                ordenar_com_openmp(
                    vetor,
                    vetor_auxiliar,
                    inicio,
                    fim,
                    quantidade_threads
                );
            }
        }
        return;
    }

    int meio = inicio + tamanho_trecho / 2;

    int processos_esquerda = processos_disponiveis / 2;
    int processos_direita =
        processos_disponiveis - processos_esquerda;

    int rank_auxiliar = rank_responsavel + processos_direita;

    // Cabecalho:
    // [inicio, fim, quantidade de processos do trecho]
    int cabecalho_tarefa[3] = {
        inicio,
        meio,
        processos_esquerda
    };

    MPI_Send(
        cabecalho_tarefa,
        3,
        MPI_INT,
        rank_auxiliar,
        TAG_CABECALHO,
        MPI_COMM_WORLD
    );

    MPI_Request requisicao_envio;

    // Envia a metade esquerda sem bloquear ate a conclusao.
    MPI_Isend(
        vetor.data() + inicio,
        meio - inicio,
        MPI_INT,
        rank_auxiliar,
        TAG_DADOS,
        MPI_COMM_WORLD,
        &requisicao_envio
    );

    // Enquanto o envio acontece, ordena a metade direita.
    ordenar_com_mpi(
        vetor,
        vetor_auxiliar,
        meio,
        fim,
        processos_direita,
        rank_responsavel,
        quantidade_threads
    );

    MPI_Wait(
        &requisicao_envio,
        MPI_STATUS_IGNORE
    );

    // Recebe a metade esquerda ja ordenada.
    MPI_Recv(
        vetor.data() + inicio,
        meio - inicio,
        MPI_INT,
        rank_auxiliar,
        TAG_RESULTADO,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    // Mantem o merge sequencial da versao original.
    baseline_merge(
        vetor,
        vetor_auxiliar,
        inicio,
        meio,
        fim
    );
}

/* ---------- Processos auxiliares ---------- */

static void executar_processo_auxiliar(
    vector<int>& vetor,
    vector<int>& vetor_auxiliar,
    int rank_atual,
    int quantidade_threads
) {
    int cabecalho_tarefa[3];
    MPI_Status status_recebimento;

    MPI_Recv(
        cabecalho_tarefa,
        3,
        MPI_INT,
        MPI_ANY_SOURCE,
        TAG_CABECALHO,
        MPI_COMM_WORLD,
        &status_recebimento
    );

    int rank_origem = status_recebimento.MPI_SOURCE;
    int inicio = cabecalho_tarefa[0];
    int fim = cabecalho_tarefa[1];
    int processos_disponiveis = cabecalho_tarefa[2];

    MPI_Recv(
        vetor.data() + inicio,
        fim - inicio,
        MPI_INT,
        rank_origem,
        TAG_DADOS,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    ordenar_com_mpi(
        vetor,
        vetor_auxiliar,
        inicio,
        fim,
        processos_disponiveis,
        rank_atual,
        quantidade_threads
    );

    MPI_Send(
        vetor.data() + inicio,
        fim - inicio,
        MPI_INT,
        rank_origem,
        TAG_RESULTADO,
        MPI_COMM_WORLD
    );
}

/* ---------- Programa principal ---------- */

int main(int argc, char** argv) {
    int suporte_threads_fornecido;

    MPI_Init_thread(
        &argc,
        &argv,
        MPI_THREAD_FUNNELED,
        &suporte_threads_fornecido
    );

    int rank_atual;
    int quantidade_processos;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank_atual);
    MPI_Comm_size(MPI_COMM_WORLD, &quantidade_processos);

    if (suporte_threads_fornecido < MPI_THREAD_FUNNELED) {
        if (rank_atual == 0) {
            fprintf(
                stderr,
                "Erro: suporte MPI_THREAD_FUNNELED indisponivel.\n"
            );
        }

        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int tamanho_vetor = 20000000;

    if (argc > 1 &&
        !ler_tamanho_vetor(argv[1], tamanho_vetor)) {
        if (rank_atual == 0) {
            fprintf(
                stderr,
                "Erro: informe um tamanho de vetor inteiro e positivo.\n"
            );
        }

        MPI_Finalize();
        return 1;
    }

    // Evita distribuir menos elementos que processos.
    if (quantidade_processos > tamanho_vetor) {
        if (rank_atual == 0) {
            fprintf(
                stderr,
                "Erro: a quantidade de processos nao pode "
                "superar o tamanho do vetor.\n"
            );
        }

        MPI_Finalize();
        return 1;
    }

    vector<int> vetor(tamanho_vetor);
    vector<int> vetor_auxiliar(tamanho_vetor);

    if (rank_atual == 0) {
        gerar_vetor(vetor);
    }

    int quantidade_threads = omp_get_max_threads();

    // Todos os processos concluem a preparacao antes da medicao.
    MPI_Barrier(MPI_COMM_WORLD);

    double inicio_medicao = agora();

    if (rank_atual == 0) {
        ordenar_com_mpi(
            vetor,
            vetor_auxiliar,
            0,
            tamanho_vetor,
            quantidade_processos,
            0,
            quantidade_threads
        );
    } else {
        executar_processo_auxiliar(
            vetor,
            vetor_auxiliar,
            rank_atual,
            quantidade_threads
        );
    }

    double fim_medicao = agora();

    if (rank_atual == 0) {
        bool vetor_ordenado = true;

        for (int indice = 1; indice < tamanho_vetor; indice++) {
            if (vetor[indice - 1] > vetor[indice]) {
                vetor_ordenado = false;
                break;
            }
        }

        printf(
            "n=%d  procs=%d  threads=%d  ordenado=%s  tempo=%.6f s\n",
            tamanho_vetor,
            quantidade_processos,
            quantidade_threads,
            vetor_ordenado ? "SIM" : "NAO",
            fim_medicao - inicio_medicao
        );
    }

    MPI_Finalize();
    return 0;
}
