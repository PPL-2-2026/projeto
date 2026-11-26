/* Baseline 07 — Mergesort (ordenacao por divisao e conquista)
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 *
 * Versão 2: paraleliza a recursão (msort) E o merge.
 * Mede com o mesmo protocolo do perfilamento do baseline:
 *   Fase 1: tempo de parede da versão paralela, validado contra o baseline
 *   Fase 2: onde o tempo é gasto, por nível (sort e cópia de cada merge)
 *
 * Compilar: g++ -O2 -fopenmp -o openmpV2Perfilamento openmpV2Perfilamento.cpp
 * Executar: OMP_NUM_THREADS=8 ./openmpV2Perfilamento [tamanho_vetor=20000000] [repeticoes_medidas=10] [aquecimentos=2] [corte=2160]
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <ctime>
#include <algorithm>
#include <omp.h>
#include "../../utils/baseline.h"
#include "../../utils/perfilamento.h"
using namespace std;

/* ---------- versão paralela ---------- */

/* Divide a intercalação em trechos independentes.
 * O pivô (meio da metade maior) separa por valor: tudo que vai para a
 * primeira parte é <= tudo que vai para a segunda, e cada parte escreve
 * na sua própria faixa do auxiliar. Por isso as duas podem rodar ao mesmo tempo. */
static void intercala_dividindo(const vector<int>& dados, vector<int>& auxiliar,
                                int inicio_esq, int fim_esq, int inicio_dir, int fim_dir, int inicio_saida, int particoes_do_trecho) {
    int tamanho_esq = fim_esq - inicio_esq, tamanho_dir = fim_dir - inicio_dir;
    if (particoes_do_trecho <= 1 || tamanho_esq + tamanho_dir == 0) { // não divide mais, ou trecho vazio
        intercala(dados, auxiliar, inicio_esq, fim_esq, inicio_dir, fim_dir, inicio_saida);
        return;
    }
    int corte_esq, corte_dir;
    if (tamanho_esq >= tamanho_dir) {   // divide a metade esquerda no meio
        corte_esq = (inicio_esq + fim_esq) / 2;
        corte_dir = lower_bound(dados.begin() + inicio_dir, dados.begin() + fim_dir, dados[corte_esq]) - dados.begin();
    } else {          // divide a metade direita no meio
        corte_dir = (inicio_dir + fim_dir) / 2;
        corte_esq = upper_bound(dados.begin() + inicio_esq, dados.begin() + fim_esq, dados[corte_dir]) - dados.begin();
    }
    int inicio_segunda_saida = inicio_saida + (corte_esq - inicio_esq) + (corte_dir - inicio_dir); // onde a segunda parte começa no auxiliar

    #pragma omp task shared(dados, auxiliar)
    {
        intercala_dividindo(dados, auxiliar, inicio_esq, corte_esq, inicio_dir, corte_dir, inicio_saida, particoes_do_trecho / 2);
    }
    intercala_dividindo(dados, auxiliar, corte_esq, fim_esq, corte_dir, fim_dir, inicio_segunda_saida, particoes_do_trecho - particoes_do_trecho / 2); // na thread atual
    #pragma omp taskwait
}

// Merge dividido em partições: intercalação e cópia de volta.
static void merge_paralelo(vector<int>& dados, vector<int>& auxiliar, int inicio, int meio, int fim, int particoes_do_trecho) {
    if (particoes_do_trecho <= 1) {
        baseline_merge(dados, auxiliar, inicio, meio, fim);
        return;
    }
    intercala_dividindo(dados, auxiliar, inicio, meio, meio, fim, inicio, particoes_do_trecho);
    #pragma omp taskloop num_tasks(particoes_do_trecho) shared(dados, auxiliar)
    for (int indice = inicio; indice < fim; indice++) {
        dados[indice] = auxiliar[indice];
    }
}

/* Cada chamada divide suas partições entre as duas metades. Com uma única
 * partição, a subárvore é ordenada sequencialmente. */
static void msort_paralelo(vector<int>& dados, vector<int>& auxiliar, int inicio, int fim, int particoes_do_trecho) {
    if (particoes_do_trecho <= 1) {
        baseline_msort(dados, auxiliar, inicio, fim);
        return;
    }
    int meio = (inicio + fim) / 2;
    /* A metade esquerda vira task (outra thread executa) e a direita é feita
     * pela thread atual, ao mesmo tempo. */
    #pragma omp task shared(dados, auxiliar)
    {
        msort_paralelo(dados, auxiliar, inicio, meio, particoes_do_trecho / 2);
    }
    msort_paralelo(dados, auxiliar, meio, fim, particoes_do_trecho - particoes_do_trecho / 2);
    #pragma omp taskwait
    merge_paralelo(dados, auxiliar, inicio, meio, fim, particoes_do_trecho);
}

/* ---------- versão instrumentada (mesmo protocolo do perfilamento.cpp) ---------- */
// Mesmo corte do perfilamento: merges menores que isso não são cronometrados.
static int CORTE = 2160;
static vector<Nivel> niveis;

// Níveis de baixo (cada subárvore dentro de uma thread): igual ao perfilamento do baseline.
static void merge_perfilamento(vector<int>& dados, vector<int>& auxiliar, int inicio, int meio, int fim, int nivel) {
    double inicio_intercalacao = agora();
    intercala(dados, auxiliar, inicio, meio, meio, fim, inicio);
    double fim_intercalacao = agora();
    for (int indice = inicio; indice < fim; indice++) dados[indice] = auxiliar[indice];
    double fim_copia = agora();
    // várias threads atualizam a mesma tabela: cinco campos que mudam juntos (v2.md)
    #pragma omp critical
    {
        registrar(niveis, nivel, fim - inicio, fim_intercalacao - inicio_intercalacao, fim_copia - fim_intercalacao);
    }
}

static void msort_perfilamento(vector<int>& dados, vector<int>& auxiliar, int inicio, int fim, int nivel) {
    if (fim - inicio < CORTE || fim - inicio <= 1) {
        baseline_msort(dados, auxiliar, inicio, fim);
        return;
    }
    int meio = (inicio + fim) / 2;
    msort_perfilamento(dados, auxiliar, inicio, meio, nivel + 1);
    msort_perfilamento(dados, auxiliar, meio, fim, nivel + 1);
    merge_perfilamento(dados, auxiliar, inicio, meio, fim, nivel);
}

// Níveis de cima: o merge paralelo inteiro (todas as partes) é cronometrado como um merge só.
static void merge_paralelo_perfilamento(vector<int>& dados, vector<int>& auxiliar,
                                        int inicio, int meio, int fim, int particoes_do_trecho, int nivel) {
    double inicio_intercalacao = agora();
    intercala_dividindo(dados, auxiliar, inicio, meio, meio, fim, inicio, particoes_do_trecho);
    double fim_intercalacao = agora();
    #pragma omp taskloop num_tasks(particoes_do_trecho) shared(dados, auxiliar)
    for (int indice = inicio; indice < fim; indice++) {
        dados[indice] = auxiliar[indice];
    }
    double fim_copia = agora();
    // várias threads atualizam a mesma tabela: cinco campos que mudam juntos (v2.md)
    #pragma omp critical
    {
        registrar(niveis, nivel, fim - inicio, fim_intercalacao - inicio_intercalacao, fim_copia - fim_intercalacao);
    }
}

static void msort_paralelo_perfilamento(vector<int>& dados, vector<int>& auxiliar,
                                        int inicio, int fim, int particoes_do_trecho, int nivel) {
    if (particoes_do_trecho <= 1) {
        msort_perfilamento(dados, auxiliar, inicio, fim, nivel);
        return;
    }
    int meio = (inicio + fim) / 2;
    #pragma omp task shared(dados, auxiliar)
    {
        msort_paralelo_perfilamento(dados, auxiliar, inicio, meio, particoes_do_trecho / 2, nivel + 1);
    }
    msort_paralelo_perfilamento(dados, auxiliar, meio, fim, particoes_do_trecho - particoes_do_trecho / 2, nivel + 1);
    #pragma omp taskwait
    merge_paralelo_perfilamento(dados, auxiliar, inicio, meio, fim, particoes_do_trecho, nivel);
}

/* ---------- protocolo de medição ---------- */

/* ---- Fase 1: wall time da versão paralela, validado contra o baseline ---- */
static double fase1(vector<int>& dados, vector<int>& auxiliar, const vector<int>& referencia,
                    int max_threads, int repeticoes_medidas, int aquecimentos) {
    int tamanho_vetor = (int)dados.size();
    double tempo_medio = 0.0;
    for (int repeticao = 0; repeticao < aquecimentos + repeticoes_medidas; repeticao++) {
        // Toda repetição usa o mesmo vetor inicial
        gerar_vetor(dados);
        double inicio_ordenacao = agora();
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo(dados, auxiliar, 0, tamanho_vetor, max_threads);
            }
        }
        double duracao_ordenacao = agora() - inicio_ordenacao;
        // Aquecimento é descartado
        if (repeticao >= aquecimentos) tempo_medio += duracao_ordenacao;
    }
    tempo_medio /= repeticoes_medidas; // média das repetições
    // validação: o resultado tem que ser idêntico, elemento a elemento, ao do baseline
    bool igual_ao_baseline = (dados == referencia);
    printf("\n[Fase 1] tempo por ordenacao=%.6f s  igual ao baseline=%s\n", tempo_medio, igual_ao_baseline ? "SIM" : "NAO");
    return tempo_medio;
}

/* ---- Fase 2: onde o tempo é gasto (média das repetições) ---- */
static void fase2(vector<int>& dados, vector<int>& auxiliar, const vector<int>& referencia,
                  int max_threads, int repeticoes_medidas, int aquecimentos, double tempo_fase1) {
    int tamanho_vetor = (int)dados.size();
    double total_perfilamento = 0.0;
    for (int repeticao = 0; repeticao < aquecimentos + repeticoes_medidas; repeticao++) {
        if (repeticao == aquecimentos) { // descarta o aquecimento
            niveis.clear(); total_perfilamento = 0.0;
        }
        gerar_vetor(dados); // mesma entrada a cada repetição
        double inicio_ordenacao = agora();
        #pragma omp parallel
        {
            #pragma omp single
            {
                msort_paralelo_perfilamento(dados, auxiliar, 0, tamanho_vetor, max_threads, 0);
            }
        }
        total_perfilamento += agora() - inicio_ordenacao;
    }
    bool igual_ao_baseline = (dados == referencia);
    double tempo_medio_instrumentado = total_perfilamento / repeticoes_medidas;

    printf("\n[Fase 2] onde o tempo e gasto (media de %d repeticoes)  igual ao baseline=%s\n",
           repeticoes_medidas, igual_ao_baseline ? "SIM" : "NAO");
    // Cabeçalho da tabela
    printf("  %5s %10s %7s %11s %11s %11s %11s\n",
           "nivel", "tamanho", "merges", "sort", "copia", "total", "maior");
    double soma_sort = 0.0, soma_copia = 0.0, caminho_critico = 0.0;
    // Linhas da tabela
    for (size_t nivel = 0; nivel < niveis.size(); nivel++) {
        double tempo_sort = niveis[nivel].sort / repeticoes_medidas;
        double copia = niveis[nivel].copia / repeticoes_medidas;
        double total = tempo_sort + copia;
        long long merges = niveis[nivel].merges / repeticoes_medidas;
        soma_sort += tempo_sort;
        soma_copia += copia;
        caminho_critico += niveis[nivel].maior;
        printf("  %5zu %10d %7lld %10.6fs %10.6fs %10.6fs %10.6fs\n",
               nivel, niveis[nivel].tamanho, merges, tempo_sort, copia, total, niveis[nivel].maior);
    }
    double soma_merges = soma_sort + soma_copia;
    double percentual_sort = 100.0 * soma_sort / soma_merges;
    double percentual_copia = 100.0 * soma_copia / soma_merges;
    double custo_instrumentacao = 100.0 * (tempo_medio_instrumentado - tempo_fase1) / tempo_fase1;

    // os merges de um mesmo nível rodam ao mesmo tempo, então as somas são tempo de trabalho das threads
    printf("\n  merges grandes, somando as threads: %.6f s\n", soma_merges);
    printf("    sort:                        %.6f s (%5.1f%% dos merges grandes)\n", soma_sort, percentual_sort);
    printf("    copia de volta:              %.6f s (%5.1f%% dos merges grandes)\n", soma_copia, percentual_copia);
    printf("  caminho critico (soma da coluna maior): %.6f s\n", caminho_critico);
    printf("  total instrumentado: %.6f s  (custo da instrumentacao: %+.1f%% vs Fase 1)\n", tempo_medio_instrumentado, custo_instrumentacao);
}

int main(int argc, char** argv) {
    int tamanho_vetor = (argc > 1) ? atoi(argv[1]) : 20000000;
    int repeticoes_medidas = (argc > 2) ? atoi(argv[2]) : 10;
    int aquecimentos = (argc > 3) ? atoi(argv[3]) : 2;
    CORTE = (argc > 4) ? atoi(argv[4]) : 2160;
    int max_threads = omp_get_max_threads();

    vector<int> dados(tamanho_vetor), auxiliar(tamanho_vetor);

    // resultado de referência: o mesmo vetor ordenado pelo msort do baseline (fora do cronômetro)
    vector<int> referencia(tamanho_vetor);
    gerar_vetor(referencia);
    baseline_msort(referencia, auxiliar, 0, tamanho_vetor);

    printf("n=%d  (a + tmp = %.1f MiB)  threads=%d  reps=%d  aquecimento=%d  corte=%d\n",
           tamanho_vetor, 2.0 * tamanho_vetor * sizeof(int) / (1024.0 * 1024.0), max_threads, repeticoes_medidas, aquecimentos, CORTE);

    double tempo_fase1 = fase1(dados, auxiliar, referencia, max_threads, repeticoes_medidas, aquecimentos);
    if (tamanho_vetor < CORTE) {
        printf("\n[Fase 2] ignorada: n menor que o corte (%d)\n", CORTE);
        return 0;
    }
    fase2(dados, auxiliar, referencia, max_threads, repeticoes_medidas, aquecimentos, tempo_fase1);
    return 0;
}
