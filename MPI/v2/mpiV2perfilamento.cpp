/* Baseline 07 — Mergesort (MPI + OpenMP), V2 — etapa 3: comunicacao sem cabecalhos, merge paralelo e memoria local.
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Compilar: mpicxx -O2 -fopenmp -o mpiV2perfilamento mpiV2perfilamento.cpp
 * Executar: OMP_NUM_THREADS=4 mpirun --bind-to none -np 2 ./mpi 20000000
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <vector>
#include <string>
#include <mpi.h>
#include <omp.h>
#include "../../utils/baseline.h"
using namespace std;
#include "../../utils/perfilamento.h"
static bool instrumentar = false;
static int corte = 2160;
static vector<Nivel> niveis;
enum { ENVIO, ESPERA, RECEBE_RESULTADO, RECEBE_DADOS, ENVIA_RESULTADO, MERGE_MPI, SORT_LOCAL, TOTAL_METRICAS };
static double metricas[TOTAL_METRICAS] = {};
static double iniciar() { return instrumentar ? agora() : 0.0; }
static void medir(int indice, double inicio) { if (instrumentar) metricas[indice] += agora()-inicio; }


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
    int inicio, int meio, int fim, int orcamento_tarefas, int nivel) {
    bool paralelo = orcamento_tarefas > 1 && fim-inicio >= LIMIAR_MERGE_OPENMP;
    bool medir_merge = instrumentar && fim-inicio >= corte;
    if (!paralelo && !medir_merge) {
        baseline_merge(vetor, vetor_auxiliar, inicio, meio, fim);
        return;
    }
    double inicio_intercalacao = medir_merge ? agora() : 0.0;
    if (paralelo) intercala_dividindo(vetor, vetor_auxiliar, inicio, meio, meio, fim, inicio, orcamento_tarefas);
    else intercala(vetor, vetor_auxiliar, inicio, meio, meio, fim, inicio);
    double fim_intercalacao = medir_merge ? agora() : 0.0;
    if (paralelo) {
        #pragma omp taskloop num_tasks(orcamento_tarefas) shared(vetor,vetor_auxiliar)
        for (int indice=inicio; indice<fim; ++indice) vetor[indice]=vetor_auxiliar[indice];
    } else {
        for (int indice=inicio; indice<fim; ++indice) vetor[indice]=vetor_auxiliar[indice];
    }
    if (medir_merge) {
        double fim_copia = agora();
        #pragma omp critical(registro_niveis)
        registrar(niveis, nivel, fim-inicio, fim_intercalacao-inicio_intercalacao, fim_copia-fim_intercalacao);
    }
}

static void ordenar_sequencial_perfilado(vector<int>& vetor, vector<int>& vetor_auxiliar, int inicio, int fim, int nivel) {
    if (fim-inicio <= 1 || fim-inicio < corte) {
        baseline_msort(vetor, vetor_auxiliar, inicio, fim);
        return;
    }
    int meio = inicio+(fim-inicio)/2;
    ordenar_sequencial_perfilado(vetor, vetor_auxiliar, inicio, meio, nivel+1);
    ordenar_sequencial_perfilado(vetor, vetor_auxiliar, meio, fim, nivel+1);
    merge_paralelo(vetor, vetor_auxiliar, inicio, meio, fim, 1, nivel);
}

static void ordenar_com_openmp(vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio, int fim, int orcamento_tarefas, int nivel) {
    if (fim-inicio <= 1) return;
    if (orcamento_tarefas <= 1 || fim-inicio < LIMIAR_TAREFA_OPENMP) {
        if (instrumentar) ordenar_sequencial_perfilado(vetor, vetor_auxiliar, inicio, fim, nivel);
        else baseline_msort(vetor,vetor_auxiliar,inicio,fim);
        return;
    }
    int meio=inicio+(fim-inicio)/2;
    #pragma omp task shared(vetor,vetor_auxiliar) firstprivate(inicio,meio,orcamento_tarefas)
    ordenar_com_openmp(vetor,vetor_auxiliar,inicio,meio,orcamento_tarefas/2,nivel+1);
    ordenar_com_openmp(vetor,vetor_auxiliar,meio,fim,orcamento_tarefas-orcamento_tarefas/2,nivel+1);
    #pragma omp taskwait
    merge_paralelo(vetor,vetor_auxiliar,inicio,meio,fim,orcamento_tarefas,nivel);
}

static void ordenar_com_mpi(vector<int>& vetor, vector<int>& vetor_auxiliar,
    int inicio, int fim, int processos_disponiveis, int rank_responsavel, int quantidade_threads, int nivel) {
    if (processos_disponiveis <= 1) {
        double inicio_local = iniciar();
        #pragma omp parallel
        {
            #pragma omp single
            ordenar_com_openmp(vetor,vetor_auxiliar,inicio,fim,quantidade_threads,nivel);
        }
        medir(SORT_LOCAL, inicio_local);
        return;
    }
    int meio=inicio+(fim-inicio)/2;
    int processos_esquerda=processos_disponiveis/2, processos_direita=processos_disponiveis-processos_esquerda;
    int rank_auxiliar=rank_responsavel+processos_direita;
    MPI_Request requisicao_envio;
    double inicio_operacao = iniciar();
    MPI_Isend(vetor.data()+inicio,meio-inicio,MPI_INT,rank_auxiliar,TAG_DADOS,MPI_COMM_WORLD,&requisicao_envio);
    medir(ENVIO, inicio_operacao);
    ordenar_com_mpi(vetor,vetor_auxiliar,meio,fim,processos_direita,rank_responsavel,quantidade_threads,nivel+1);
    inicio_operacao = iniciar();
    MPI_Wait(&requisicao_envio,MPI_STATUS_IGNORE);
    medir(ESPERA, inicio_operacao);
    inicio_operacao = iniciar();
    MPI_Recv(vetor.data()+inicio,meio-inicio,MPI_INT,rank_auxiliar,TAG_RESULTADO,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    medir(RECEBE_RESULTADO, inicio_operacao);
    inicio_operacao = iniciar();
    // MPI permanece fora das regioes OpenMP (MPI_THREAD_FUNNELED).
    #pragma omp parallel
    {
        #pragma omp single
        merge_paralelo(vetor,vetor_auxiliar,inicio,meio,fim,quantidade_threads,nivel);
    }
    medir(MERGE_MPI, inicio_operacao);
}

// Reproduz a arvore de distribuicao para descobrir o tamanho local
// antes da medicao, sem adicionar mensagens MPI.
struct DistribuicaoLocal {
    int tamanho_trecho;
    int processos_disponiveis;
    int rank_origem;
    int nivel;
};

static DistribuicaoLocal calcular_distribuicao_local(
    int tamanho_trecho,
    int processos_disponiveis,
    int rank_responsavel,
    int rank_atual
) {
    int rank_origem = MPI_PROC_NULL;
    int nivel = 0;
    while (rank_atual != rank_responsavel) {
        ++nivel;
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
    return {tamanho_trecho, processos_disponiveis, rank_origem, nivel};
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
    double inicio_operacao = iniciar();
    MPI_Recv(vetor.data(), tamanho_trecho, MPI_INT, rank_origem,
             TAG_DADOS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    medir(RECEBE_DADOS, inicio_operacao);
    ordenar_com_mpi(vetor, vetor_auxiliar, 0, tamanho_trecho,
                    processos_disponiveis, rank_atual, quantidade_threads, distribuicao_local.nivel);
    inicio_operacao = iniciar();

    MPI_Send(vetor.data(), tamanho_trecho, MPI_INT, rank_origem,
             TAG_RESULTADO, MPI_COMM_WORLD);
    medir(ENVIA_RESULTADO, inicio_operacao);
}


int main(int argc, char** argv) {
    int suporte, rank, processos;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &suporte);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processos);
    if (suporte < MPI_THREAD_FUNNELED) MPI_Abort(MPI_COMM_WORLD, 1);
    int n=20000000, repeticoes=5, aquecimento=1;
    bool valido = (argc<=1 || ler_tamanho_vetor(argv[1], n)) &&
                  (argc<=2 || ler_tamanho_vetor(argv[2], repeticoes)) &&
                  (argc<=3 || (string(argv[3])=="0" ? (aquecimento=0, true) : ler_tamanho_vetor(argv[3], aquecimento))) &&
                  (argc<=4 || ler_tamanho_vetor(argv[4], corte));
    if (!valido || processos>n || aquecimento>INT_MAX-repeticoes) {
        if (!rank) fprintf(stderr,"Parametros invalidos.\n");
        MPI_Finalize(); return 1;
    }
    int threads=omp_get_max_threads();
    auto distribuicao=calcular_distribuicao_local(n, processos, 0, rank);
    vector<int> vetor(distribuicao.tamanho_trecho), vetor_auxiliar(distribuicao.tamanho_trecho), referencia;
    double memoria=2.0*vetor.size()*sizeof(int)/(1024.0*1024.0), memoria_total=0;
    MPI_Reduce(&memoria,&memoria_total,1,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    if (!rank) {
        referencia.resize(n); gerar_vetor(referencia); baseline_msort(referencia,vetor_auxiliar,0,n);
        printf("n=%d procs=%d threads=%d reps=%d aquecimento=%d corte=%d\n",n,processos,threads,repeticoes,aquecimento,corte);
        printf("Vetores principais: %.2f MiB totais (referencia excluida)\n",memoria_total);
    }
    double tempo_fase1=0;
    int correto=1;
    for (int fase=1; fase<=2; ++fase) {
        instrumentar=(fase==2);
        double soma=0;
        for (int repeticao=0; repeticao<aquecimento+repeticoes; ++repeticao) {
            if (repeticao==aquecimento) { niveis.clear(); fill(metricas,metricas+TOTAL_METRICAS,0.0); }
            if (!rank) gerar_vetor(vetor);
            MPI_Barrier(MPI_COMM_WORLD);
            double inicio=agora();
            if (!rank) ordenar_com_mpi(vetor,vetor_auxiliar,0,n,processos,0,threads,0);
            else executar_processo_auxiliar(vetor,vetor_auxiliar,rank,threads,distribuicao);
            double tempo=agora()-inicio;
            if (!rank) { if (repeticao>=aquecimento) soma+=tempo; correto=correto && (vetor==referencia); }
        }
        if (!rank) {
            printf("[Fase %d] tempo por ordenacao=%.6f s igual ao baseline=%s\n",fase,soma/repeticoes,correto?"SIM":"NAO");
            if (fase==1) tempo_fase1=soma/repeticoes;
            else printf("Variacao vs Fase 1: %+.1f%%\n",100*(soma/repeticoes-tempo_fase1)/tempo_fase1);
        }
    }
    int local=niveis.size(), total=0;
    MPI_Allreduce(&local,&total,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
    double soma_sort=0,soma_copia=0;
    if (!rank) printf("nivel tamanho merges sort copia total maior\n");
    for(int nivel=0;nivel<total;++nivel) {
        double valores[3]={}, somas[3]={}, maximos[2]={}, maiores[2]={};
        if(nivel<local) { auto &d=niveis[nivel]; valores[0]=d.sort; valores[1]=d.copia; valores[2]=d.merges; maximos[0]=d.tamanho; maximos[1]=d.maior; }
        MPI_Reduce(valores,somas,3,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
        MPI_Reduce(maximos,maiores,2,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
        if(!rank) { double sort=somas[0]/repeticoes,copia=somas[1]/repeticoes; soma_sort+=sort;soma_copia+=copia; printf("%5d %10.0f %8.0f %.6f %.6f %.6f %.6f\n",nivel,maiores[0],somas[2]/repeticoes,sort,copia,sort+copia,maiores[1]); }
    }
    double somas[TOTAL_METRICAS]={}, max_sort=0;
    MPI_Reduce(metricas,somas,TOTAL_METRICAS,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
    MPI_Reduce(&metricas[SORT_LOCAL],&max_sort,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
    if(!rank) {
        printf("Merges grandes (soma das duracoes): %.6f s; sort %.6f s; copia %.6f s\n",soma_sort+soma_copia,soma_sort,soma_copia);
        const char* nomes[]={"MPI_Isend","MPI_Wait","MPI_Recv resultado","MPI_Recv dados","MPI_Send resultado","Merge MPI","Ordenacao local"};
        for(int i=0;i<TOTAL_METRICAS;++i) printf("%s: %.6f s\n",nomes[i],somas[i]/repeticoes);
        printf("Maior media de ordenacao local por rank: %.6f s\n",max_sort/repeticoes);
    }
    MPI_Bcast(&correto,1,MPI_INT,0,MPI_COMM_WORLD);
    MPI_Finalize(); return correto?0:1;
}
