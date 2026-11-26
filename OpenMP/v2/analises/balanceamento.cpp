#include <cstdio>
#include <cstdlib>
#include <omp.h>
#include <algorithm>
#include "../../../utils/baseline.h"

/* Divide a intercalação em trechos independentes.
 * O pivô (meio da metade maior) separa por valor: tudo que vai para a
 * primeira parte é <= tudo que vai para a segunda, e cada parte escreve
 * na sua própria faixa do auxiliar. Por isso as duas podem rodar ao mesmo tempo. */
template<bool medir_carga = false>
static void intercala_dividindo(const std::vector<int>& dados, std::vector<int>& auxiliar,
                                int inicio_esq, int fim_esq, int inicio_dir, int fim_dir, int inicio_saida, int particoes_do_trecho, int* cargas = nullptr) {
    int tamanho_esq = fim_esq - inicio_esq, tamanho_dir = fim_dir - inicio_dir;
    if (particoes_do_trecho <= 1 || tamanho_esq + tamanho_dir == 0) { // não divide mais, ou trecho vazio
        if (medir_carga) cargas[0] = tamanho_esq + tamanho_dir;
        intercala(dados, auxiliar, inicio_esq, fim_esq, inicio_dir, fim_dir, inicio_saida);
        return;
    }
    int corte_esq, corte_dir;
    if (tamanho_esq >= tamanho_dir) {   // divide a metade esquerda no meio
        corte_esq = (inicio_esq + fim_esq) / 2; // encontra o valor do meio do primeiro trecho
        corte_dir = std::lower_bound(dados.begin() + inicio_dir, dados.begin() + fim_dir, dados[corte_esq]) - dados.begin(); // busca no segundo trecho o primeiro valor maior ou igual ao valor que está no meio do primeiro trecho
    } else {          // divide a metade direita no meio
        corte_dir = (inicio_dir + fim_dir) / 2;
        corte_esq = std::upper_bound(dados.begin() + inicio_esq, dados.begin() + fim_esq, dados[corte_dir]) - dados.begin(); // Busca no primeiro trecho o primeiro valor estritamente maior que esse valor do meio
    }
    int inicio_segunda_saida = inicio_saida + (corte_esq - inicio_esq) + (corte_dir - inicio_dir); // onde a segunda parte começa no auxiliar

    #pragma omp task shared(dados, auxiliar)
    {
        intercala_dividindo<medir_carga>(dados, auxiliar, inicio_esq, corte_esq, inicio_dir, corte_dir, inicio_saida, particoes_do_trecho / 2, cargas);
    }
    intercala_dividindo<medir_carga>(dados, auxiliar, corte_esq, fim_esq, corte_dir, fim_dir, inicio_segunda_saida,
                                    particoes_do_trecho - particoes_do_trecho / 2,
                                    medir_carga ? cargas + particoes_do_trecho / 2 : nullptr); // na thread atual
    #pragma omp taskwait
}

template<bool medir_carga>
double executar(int tamanho, int threads, bool alternados, std::vector<int>& cargas) {
    std::vector<int> dados(tamanho), auxiliar(tamanho, -1);
    int meio = tamanho / 2;
    for (int i = 0; i < meio; ++i) {
        dados[i] = alternados ? 2 * i : i;
        dados[meio + i] = alternados ? 2 * i + 1 : meio + i;
    }
    double tempo;
    #pragma omp parallel num_threads(threads)
    {
        #pragma omp single
        {
            if (omp_get_num_threads() != threads) std::abort();
            double inicio = agora();
            intercala_dividindo<medir_carga>(dados, auxiliar, 0, meio, meio, tamanho,
                                            0, threads, cargas.data());
            tempo = agora() - inicio;
        }
    }
    for (int i = 0; i < tamanho; ++i)
        if (auxiliar[i] != i) std::abort();
    return tempo;
}

int main() {
    omp_set_dynamic(0);
    constexpr int tamanho = 50000000;
    std::vector<int> teste(2);
    executar<true>(8, 2, true, teste);
    if (teste != std::vector<int>{4, 4}) return 1;
    executar<true>(8, 2, false, teste);
    if (teste != std::vector<int>{2, 6}) return 1;

    std::puts("threads,entrada,repeticao,segundos,cargas");
    for (int threads : {1, 2, 4, 8}) {
        std::vector<std::vector<int>> cargas(2, std::vector<int>(threads));
        for (int tipo : {0, 1})
            executar<true>(tamanho, threads, tipo == 0, cargas[tipo]);
        for (int rodada = 0; rodada < 12; ++rodada) {
            for (int ordem : {0, 1}) {
                int tipo = (rodada + ordem) % 2;
                double tempo = executar<false>(tamanho, threads, tipo == 0, cargas[tipo]);
                if (rodada < 2) continue;
                std::printf("%d,%s,%d,%.9f,", threads, tipo == 0 ? "alternados" : "separados",
                            rodada - 1, tempo);
                for (int i = 0; i < threads; ++i)
                    std::printf("%s%d", i ? " " : "", cargas[tipo][i]);
                std::puts("");
            }
        }
    }
}
