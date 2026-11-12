# Teste de escalabilidade fraca — OpenMP v2

## Procedimento

Entrada de 1.000.000 de elementos por thread, com 1, 2, 4, 8 e 16 threads.
Foi compilado o programa original `OpenMP/v2/openmpV2Perfilamento.cpp`, sem
alterações, usando `g++ -O2 -fopenmp` (GCC 13.3.0). Cada configuração foi executada
sequencialmente, com dois aquecimentos descartados, dez repetições medidas e
corte de instrumentação 2160. O tempo apresentado é exclusivamente a média da
Fase 1; a Fase 2 também é executada pelo programa, mas seus tempos não entram
na análise. Geração da entrada e validação ficam fora do cronômetro; a região
paralela está dentro dele.

Configuração: `OMP_NUM_THREADS=p`, `OMP_DYNAMIC=false`, `OMP_PROC_BIND=close`,
`OMP_PLACES=cores`, `OMP_THREAD_LIMIT=16`, `OMP_MAX_ACTIVE_LEVELS=1`.
O vetor inicial determinístico é regenerado a cada repetição. O programa compara
o último resultado de cada fase com uma referência sequencial; todas as
comparações retornaram `SIM`. Não há validação individual de cada repetição.

## Ambiente

AMD Ryzen 7 5700G, 8 núcleos físicos e 16 threads lógicas, conforme topologia
exposta pelo ambiente virtualizado Microsoft. CPUs 0–15 permitidas; cgroup sem
quota de CPU (`cpu.max = max 100000`). A configuração com 16 threads usa SMT,
portanto não equivale a 16 núcleos físicos. Não se controlaram frequência da CPU
nem atividade externa ao processo. O ambiente completo e a configuração OpenMP
impressa pelo runtime estão em `ambiente.txt` e nos logs de cada execução.

## Resultados

| Threads | Elementos | Elementos/thread | Tempo médio Fase 1 (s) | Eficiência fraca |
|---:|---:|---:|---:|---:|
| 1 | 1.000.000 | 1.000.000 | 0,076268 | 100,00% |
| 2 | 2.000.000 | 1.000.000 | 0,079606 | 95,81% |
| 4 | 4.000.000 | 1.000.000 | 0,092123 | 82,79% |
| 8 | 8.000.000 | 1.000.000 | 0,121138 | 62,96% |
| 16 | 16.000.000 | 1.000.000 | 0,197049 | 38,71% |

Eficiência fraca convencional: `100 × T(1, n0) / T(p, p × n0)`.
A referência é a própria v2 com uma thread e um milhão de elementos, e não o
tempo do baseline usado nos gráficos de escalabilidade forte. Os cálculos usam
os tempos impressos pelo programa, arredondados a seis casas decimais.

## Interpretação e limitações

Com duas threads, o volume dobra e o tempo cresce cerca de 4,38%. Com oito,
o volume é oito vezes maior e o tempo cresce cerca de 58,83%. Com dezesseis,
o tempo é aproximadamente 2,58 vezes o inicial para um volume dezesseis vezes
maior. A capacidade de processar elementos por segundo aumenta, mas o tempo
não se mantém constante.

Merge Sort tem trabalho O(n log n). Manter n/p constante não mantém exatamente
o trabalho por thread constante: pelo modelo n log2(n), o crescimento relativo
é log2(p × n0)/log2(n0). Para n0=1.000.000 e p=16, esse fator é aproximadamente
1,201, ou 20,1% mais trabalho por thread. Isso é uma referência teórica de trabalho,
não uma previsão exata do walltime.

Os resultados não isolam as causas do aumento: custos de tarefas e sincronização,
memória/cache, SMT e variação do ambiente podem contribuir. O programa conserva
apenas as médias, portanto não há desvio padrão ou intervalos de confiança.
Esta rodada mede este ambiente e não substitui as medições históricas dos relatórios
v1/v2, que podem ter sido feitas sob outras condições.

## Reprodução e evidências

Na raiz do projeto:

```bash
python3 OpenMP/v2/escalabilidade-fraca/executar.py
```

O script compila em diretório temporário e substitui os resultados desta pasta
por uma nova rodada. `resultados.csv` contém os dados tabulares; `threads-*.log`
contém a saída completa do programa original e a configuração do runtime;
`ambiente.txt` registra compilador, topologia, limites e hash do fonte principal.
