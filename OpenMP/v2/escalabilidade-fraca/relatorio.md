# Escalabilidade fraca — OpenMP v2

## Protocolo

- Entrada: `n = p × 1.000.000`, com `p ∈ {1, 2, 4, 8, 16}` threads.
- Compilação: GCC 13.3.0, `-O2 -fopenmp`.
- Medição: média de 10 repetições da Fase 1 após 2 aquecimentos. Geração da
  entrada e validação fora do cronômetro; região paralela incluída.
- OpenMP: `OMP_DYNAMIC=false`, `OMP_PROC_BIND=close`, `OMP_PLACES=cores`,
  `OMP_THREAD_LIMIT=16` e `OMP_MAX_ACTIVE_LEVELS=1`.
- Ambiente: Ryzen 7 5700G, 8 núcleos físicos e 16 threads lógicas, em ambiente
  virtualizado Microsoft, sem quota de CPU. A configuração de 16 threads usa SMT.
- Validação: último vetor de cada fase idêntico ao baseline em todas as configurações.

## Resultados

Eficiência fraca: `E(p) = T(1, n₀) / T(p, p × n₀) × 100%`, com `n₀ = 1.000.000`.
A referência é a própria v2 com uma thread. Os valores abaixo são da rodada original.

| Threads | Elementos | Tempo médio (s) | Eficiência fraca |
|---:|---:|---:|---:|
| 1 | 1.000.000 | 0,076268 | 100,00% |
| 2 | 2.000.000 | 0,079606 | 95,81% |
| 4 | 4.000.000 | 0,092123 | 82,79% |
| 8 | 8.000.000 | 0,121138 | 62,96% |
| 16 | 16.000.000 | 0,197049 | 38,71% |

![Eficiência da escalabilidade fraca](eficiencia.svg)

A linha de 100% corresponde a tempo constante ao aumentar proporcionalmente
os dados e as threads. Com 16 threads, o volume cresce 16× e o tempo, 2,58×.

## Análise

A eficiência cai de 95,81% com 2 threads para 38,71% com 16. Para Merge Sort,
`O(n log n)`, manter `n/p` constante aumenta o trabalho teórico por thread na
proporção `log₂(p × n₀) / log₂(n₀)`: aproximadamente 20,1% entre 1 e 16 threads.
Assim, a referência de tempo constante não considera esse crescimento do trabalho.

Os dados não isolam os efeitos de sincronização, memória/cache e SMT. Frequência
da CPU e atividade externa não foram controladas; foram registradas apenas médias,
sem desvio padrão ou intervalo de confiança.

## Reprodução

Na raiz do projeto:

```bash
bash OpenMP/v2/escalabilidade-fraca/executar.sh
```

O script executa ambas as fases, com corte de instrumentação 2160, e exibe apenas
a Fase 1. Novas execuções não atualizam automaticamente a tabela ou o gráfico.
