/* Baseline 07 — Mergesort (MPI + OpenMP), V2 — etapa 3: comunicacao sem cabecalhos, merge paralelo e memoria local.
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Compilar: mpicxx -O2 -fopenmp -o mpi mpi.cpp
 * Executar: OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpi 20000000
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <vector>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"
using namespace std;

enum { TAG_DADOS = 1, TAG_RESULTADO = 2 };
static constexpr int LIMIAR_TAREFA_OPENMP = 2160;
static constexpr int LIMIAR_MERGE_OPENMP = 32768;

static bool ler_tamanho_vetor(const char* argumento, int& tamanho_vetor) {
    errno = 0;
    char* fim_conversao = nullptr;
    long valor = strtol(argumento, &fim_conversao, 10);
    if (errno == ERANGE || fim_conversao == argumento || *fim_conversao != '\0' || valor <= 0 || valor > INT_MAX)
        return false;
    tamanho_vetor = static_cast<int>(valor);
    return true;
}

// Particiona por valor, preservando a ordem dos empates do baseline.
static void intercala_dividindo(const vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio_esquerda, int fim_esquerda, int inicio_direita, int fim_direita, int inicio_saida, int orcamento_tarefas) {
    int tamanho_esquerda = fim_esquerda - inicio_esquerda, tamanho_direita = fim_direita - inicio_direita;
    if (orcamento_tarefas <= 1 || tamanho_esquerda + tamanho_direita < LIMIAR_MERGE_OPENMP) {
        intercala(vetor, vetor_auxiliar, inicio_esquerda, fim_esquerda, inicio_direita, fim_direita, inicio_saida);
        return;
    }
    int pivo_esquerda, pivo_direita;
    if (tamanho_esquerda >= tamanho_direita) {
        pivo_esquerda = inicio_esquerda + tamanho_esquerda / 2;
        pivo_direita = lower_bound(vetor.begin()+inicio_direita, vetor.begin()+fim_direita, vetor[pivo_esquerda]) - vetor.begin();
    } else {
        pivo_direita = inicio_direita + tamanho_direita / 2;
        pivo_esquerda = upper_bound(vetor.begin()+inicio_esquerda, vetor.begin()+fim_esquerda, vetor[pivo_direita]) - vetor.begin();
    }
    int meio_saida = inicio_saida + (pivo_esquerda-inicio_esquerda) + (pivo_direita-inicio_direita);
    #pragma omp task shared(vetor,vetor_auxiliar) firstprivate(inicio_esquerda,pivo_esquerda,inicio_direita,pivo_direita,inicio_saida,orcamento_tarefas)
    intercala_dividindo(vetor,vetor_auxiliar,inicio_esquerda,pivo_esquerda,inicio_direita,pivo_direita,inicio_saida,orcamento_tarefas/2);
    intercala_dividindo(vetor,vetor_auxiliar,pivo_esquerda,fim_esquerda,pivo_direita,fim_direita,meio_saida,orcamento_tarefas-orcamento_tarefas/2);
    #pragma omp taskwait
}

// Deve ser chamada dentro de uma equipe OpenMP, por uma unica tarefa.
static void merge_paralelo(vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio, int meio, int fim, int orcamento_tarefas) {
    if (orcamento_tarefas <= 1 || fim-inicio < LIMIAR_MERGE_OPENMP) {
        baseline_merge(vetor,vetor_auxiliar,inicio,meio,fim);
        return;
    }
    intercala_dividindo(vetor,vetor_auxiliar,inicio,meio,meio,fim,inicio,orcamento_tarefas);
    // taskloop possui taskgroup implicito: a copia termina antes do retorno.
    #pragma omp taskloop num_tasks(orcamento_tarefas) shared(vetor,vetor_auxiliar)
    for (int indice=inicio; indice<fim; ++indice) vetor[indice]=vetor_auxiliar[indice];
}

static void ordenar_com_openmp(vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio, int fim, int orcamento_tarefas) {
    if (fim-inicio <= 1) return;
    if (orcamento_tarefas <= 1 || fim-inicio < LIMIAR_TAREFA_OPENMP) {
        baseline_msort(vetor,vetor_auxiliar,inicio,fim);
        return;
    }
    int meio=inicio+(fim-inicio)/2;
    #pragma omp task shared(vetor,vetor_auxiliar) firstprivate(inicio,meio,orcamento_tarefas)
    ordenar_com_openmp(vetor,vetor_auxiliar,inicio,meio,orcamento_tarefas/2);
    ordenar_com_openmp(vetor,vetor_auxiliar,meio,fim,orcamento_tarefas-orcamento_tarefas/2);
    #pragma omp taskwait
    merge_paralelo(vetor,vetor_auxiliar,inicio,meio,fim,orcamento_tarefas);
}

static void ordenar_com_mpi(vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio, int fim, int processos_disponiveis, int rank_responsavel, int quantidade_threads) {
    if (processos_disponiveis <= 1) {
        #pragma omp parallel
        {
            #pragma omp single
            ordenar_com_openmp(vetor,vetor_auxiliar,inicio,fim,quantidade_threads);
        }
        return;
    }
    int meio=inicio+(fim-inicio)/2;
    int processos_esquerda=processos_disponiveis/2, processos_direita=processos_disponiveis-processos_esquerda;
    int rank_auxiliar=rank_responsavel+processos_direita;
    MPI_Request requisicao_envio;
    MPI_Isend(vetor.data()+inicio,meio-inicio,MPI_INT,rank_auxiliar,TAG_DADOS,MPI_COMM_WORLD,&requisicao_envio);
    ordenar_com_mpi(vetor,vetor_auxiliar,meio,fim,processos_direita,rank_responsavel,quantidade_threads);
    MPI_Wait(&requisicao_envio,MPI_STATUS_IGNORE);
    MPI_Recv(vetor.data()+inicio,meio-inicio,MPI_INT,rank_auxiliar,TAG_RESULTADO,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    // MPI permanece fora das regioes OpenMP (MPI_THREAD_FUNNELED).
    #pragma omp parallel
    {
        #pragma omp single
        merge_paralelo(vetor,vetor_auxiliar,inicio,meio,fim,quantidade_threads);
    }
}

// Reproduz a arvore de distribuicao para descobrir o tamanho local
// antes da medicao, sem adicionar mensagens MPI.
struct DistribuicaoLocal {
    int tamanho_trecho;
    int processos_disponiveis;
    int rank_origem;
};

static DistribuicaoLocal calcular_distribuicao_local(
    int tamanho_trecho,
    int processos_disponiveis,
    int rank_responsavel,
    int rank_atual
) {
    int rank_origem = MPI_PROC_NULL;
    while (rank_atual != rank_responsavel) {
        int tamanho_esquerda = tamanho_trecho / 2;
        int processos_esquerda = processos_disponiveis / 2;
        int processos_direita = processos_disponiveis - processos_esquerda;
        int rank_auxiliar = rank_responsavel + processos_direita;

        if (rank_atual >= rank_auxiliar) {
            tamanho_trecho = tamanho_esquerda;
            processos_disponiveis = processos_esquerda;
            rank_origem = rank_responsavel;
            rank_responsavel = rank_auxiliar;
        } else {
            tamanho_trecho -= tamanho_esquerda;
            processos_disponiveis = processos_direita;
        }
    }
    return {tamanho_trecho, processos_disponiveis, rank_origem};
}

static void executar_processo_auxiliar(
    vector<int>& vetor,
    vector<int>& vetor_auxiliar,
    int rank_atual,
    int quantidade_threads,
    const DistribuicaoLocal& distribuicao_local
) {
    // A origem e o tamanho sao conhecidos antes da comunicacao.
    int rank_origem = distribuicao_local.rank_origem;
    int tamanho_trecho = distribuicao_local.tamanho_trecho;
    int processos_disponiveis = distribuicao_local.processos_disponiveis;
    MPI_Recv(vetor.data(), tamanho_trecho, MPI_INT, rank_origem,
             TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    ordenar_com_mpi(vetor, vetor_auxiliar, 0, tamanho_trecho,
                    processos_disponiveis, rank_atual, quantidade_threads);

    MPI_Send(vetor.data(), tamanho_trecho, MPI_INT, rank_origem,
             TAG_RESULTADO, MPI_COMM_WORLD);
}

int main(int argc,char** argv) {
    int suporte_threads_fornecido,rank_atual,quantidade_processos;
    MPI_Init_thread(&argc,&argv,MPI_THREAD_FUNNELED,&suporte_threads_fornecido);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank_atual);
    MPI_Comm_size(MPI_COMM_WORLD,&quantidade_processos);
    if (suporte_threads_fornecido<MPI_THREAD_FUNNELED) MPI_Abort(MPI_COMM_WORLD,1);
    int tamanho_vetor=20000000;
    if ((argc>1 && !ler_tamanho_vetor(argv[1],tamanho_vetor)) || quantidade_processos>tamanho_vetor) {
        if(rank_atual==0) fprintf(stderr,"Erro: use n positivo e processos <= n.\n");
        MPI_Finalize(); return 1;
    }
    DistribuicaoLocal distribuicao_local = calcular_distribuicao_local(
        tamanho_vetor, quantidade_processos, 0, rank_atual
    );
    vector<int> vetor(distribuicao_local.tamanho_trecho);
    vector<int> vetor_auxiliar(distribuicao_local.tamanho_trecho);
    if(rank_atual==0) gerar_vetor(vetor);
    int quantidade_threads=omp_get_max_threads();
    MPI_Barrier(MPI_COMM_WORLD);
    double inicio_medicao=agora();
    if(rank_atual==0) ordenar_com_mpi(vetor,vetor_auxiliar,0,tamanho_vetor,quantidade_processos,0,quantidade_threads);
    else executar_processo_auxiliar(vetor,vetor_auxiliar,rank_atual,quantidade_threads,distribuicao_local);
    double fim_medicao=agora();
    int resultado_correto=1;
    if(rank_atual==0) {
        // Validacao exata, fora da regiao cronometrada.
        vector<int> referencia(tamanho_vetor);
        gerar_vetor(referencia);
        baseline_msort(referencia,vetor_auxiliar,0,tamanho_vetor);
        resultado_correto=(vetor==referencia);
        printf("n=%d  procs=%d  threads=%d  ordenado=%s  tempo=%.6f s\n",tamanho_vetor,quantidade_processos,quantidade_threads,resultado_correto?"SIM":"NAO",fim_medicao-inicio_medicao);
    }
    MPI_Bcast(&resultado_correto,1,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Finalize(); return resultado_correto?0:1;
}
