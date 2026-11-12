# MergeSort paralelo

Projeto em C++ que compara o desempenho do MergeSort sequencial com versões paralelas usando OpenMP e MPI + OpenMP.

## O que está implementado

- **Baseline sequencial:** ordenação por divisão e conquista, usada como referência nas comparações.
- **OpenMP V1:** paralelização da recursão com tarefas; o merge permanece sequencial.
- **OpenMP V2:** paralelização da recursão, da intercalação e da cópia dos resultados.
- **MPI + OpenMP V1:** distribuição de trechos entre processos MPI e ordenação com tarefas OpenMP dentro de cada processo; o merge permanece sequencial.
- **Perfilamento:** versões instrumentadas para medir o tempo de execução e o custo dos merges por nível da recursão.
- **Análises de desempenho:** relatórios e gráficos de tempo, speedup e eficiência.

Os programas geram vetores de inteiros e verificam se o resultado está ordenado.

## Organização

| Pasta | Conteúdo |
|---|---|
| [baseline e perfilamento](baseline%20e%20perfilamento/) | Implementação sequencial, perfilamento e análise inicial |
| [OpenMP/v1](OpenMP/v1/) | Recursão paralela e medições |
| [OpenMP/v2](OpenMP/v2/) | Recursão e merge paralelos, medições e análise de escalabilidade |
| [MPI/v1](MPI/v1/) | Implementação híbrida MPI + OpenMP e medições |
| [utils](utils/) | Funções compartilhadas de ordenação, geração de dados e perfilamento |

## Execução com Docker

Requer Docker com suporte a contêineres Linux e Docker Compose. A imagem inclui GCC 14 e OpenMPI; o código é compilado a cada execução.

```sh
docker compose build
docker compose run --rm baseline
docker compose run --rm perfilamento
docker compose run --rm mpi
docker compose run --rm mpiperfilamento
```

Parâmetros configuráveis por variáveis de ambiente:

| Variável | Padrão | Uso |
|---|---|---|
| `N` | `20000000` | Quantidade de elementos |
| `THREADS` | `4` | Threads OpenMP por processo |
| `PROCS` | `2` | Processos MPI |
| `REPS` | `5` | Repetições no perfilamento MPI |
| `AQUEC` | `1` | Aquecimentos no perfilamento MPI |
| `CORTE` | `2160` | Tamanho mínimo do trecho instrumentado no perfilamento MPI |

Para executar as versões OpenMP, compile os arquivos de `OpenMP/v1` ou `OpenMP/v2` em um ambiente Linux com GCC e OpenMP. Exemplo para a V2:

```sh
mkdir -p bin
g++ -O2 -fopenmp -o bin/openmp-v2 OpenMP/v2/openmp.cpp
OMP_NUM_THREADS=4 ./bin/openmp-v2 1000000
```
