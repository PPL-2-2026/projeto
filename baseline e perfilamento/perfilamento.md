# Análise de Complexidade

O baseline é um mergesort recursivo de cima para baixo:

- `msort(lo, hi)` divide o trecho `[lo, hi)` ao meio e chama a si mesma para cada metade, até sobrar 1 elemento.
- `merge(lo, mid, hi)` intercala as duas metades ordenadas em `tmp` e depois copia o resultado de volta para `a`.

# Identificação de pontos quentes

## Protocolo de medição
**Repetições**

- 2 execuções de aquecimento que são descartadas.
- 10 execuções medidas sendo que os valores reportados são as médias dessas execuções.

**Tamanhos de entrada**
| n |
|---|
| 100.000 |
| 1.000.000 |
| 5.000.000 |
| 20.000.000 |
| 50.000.000 |

**Fases da nossa medição**

1. **Fase 1 (walltime):** Roda uma cópia identica do baseline pra podermos obter o tempo total da execução. Serve pra calcular o speedup e como referência para a Fase 2.
2. **Fase 2 (onde o tempo é gasto):** uma cópia instrumentada que cronometra cada merge separando o sort (laços `while`) da cópia de `tmp` para `a` (laço `for`), agrupando por cada nível da recursão.

**Corte da instrumentação**

Cada leitura do relógio (`clock_gettime`) custa cerca de 36 ns na maquina que usamos enquanto um merge custa cerca de 5 ns por elemento. Sendo assim, decidimos aplicar um numero minimo de elementos para usar o merge com a instrumentação completa, e qualquer merge com menos elementos que esse numero de corte usa o merge normal (o do baseline).

Definimos que o máximo de tempo gasto com relógio teria de ser 1% do tempo de cada merge cronometrado. Então fizemos o seguinte calculo:

tempo total gasto com relógios em um merge = 36 * 3 = 108 ns
Se 1% é 108 ns então 100% é 10.800 ns
10.800 ns dividido por 5 ns que é o custo por elemento de um merge = 2160 elementos

Ficou assim definido que o valor de corte seria 2160, qualquer merge com menos elementos que 2160 passa então a usar o merge original.

## Tabela de tempo

**Tempo da Fase 1 por tamanho**

| n | Tempo por ordenação (s) |
|---|---|
| 100.000 | 0,00837 |
| 1.000.000 | 0,09921 |
| 5.000.000 | 0,55147 |
| 20.000.000 | 2,42855 |
| 50.000.000 | 6,70504 |

**Onde o tempo é gasto (Fase 2, n = 50.000.000, corte = 2160)**

| Nível | Tamanho do trecho | Merges | Sort (s) | Cópia (s) | Total (s) | % do total | Maior merge (s) |
|---|---|---|---|---|---|---|---|
| 0 | 50.000.000 | 1 | 0,2355 | 0,0457 | 0,2812 | 4,2% | 0,2941 |
| 1 | 25.000.000 | 2 | 0,2373 | 0,0415 | 0,2788 | 4,1% | 0,1489 |
| 2 | 12.500.000 | 4 | 0,2402 | 0,0372 | 0,2774 | 4,1% | 0,0804 |
| 3 | 6.250.000 | 8 | 0,2400 | 0,0331 | 0,2731 | 4,1% | 0,0433 |
| 4 | 3.125.000 | 16 | 0,2401 | 0,0239 | 0,2640 | 3,9% | 0,0208 |
| 5 | 1.562.500 | 32 | 0,2401 | 0,0214 | 0,2615 | 3,9% | 0,0100 |
| 6 | 781.250 | 64 | 0,2402 | 0,0198 | 0,2600 | 3,9% | 0,0050 |
| 7 | 390.625 | 128 | 0,2403 | 0,0195 | 0,2598 | 3,9% | 0,0026 |
| 8 | 195.313 | 256 | 0,2401 | 0,0193 | 0,2594 | 3,8% | 0,0013 |
| 9 | 97.657 | 512 | 0,2398 | 0,0195 | 0,2593 | 3,8% | 0,0007 |
| 10 | 48.829 | 1.024 | 0,2394 | 0,0194 | 0,2588 | 3,8% | 0,0004 |
| 11 | 24.415 | 2.048 | 0,2390 | 0,0192 | 0,2582 | 3,8% | 0,0002 |
| 12 | 12.208 | 4.096 | 0,2387 | 0,0193 | 0,2580 | 3,8% | 0,0001 |
| 13 | 6.104 | 8.192 | 0,2388 | 0,0197 | 0,2586 | 3,8% | 0,0001 |
| 14 | 3.052 | 16.384 | 0,2386 | 0,0198 | 0,2584 | 3,8% | 0,0001 |

| Parte | Tempo (s) | % |
|---|---|---|
| Merges grandes (níveis 0–14) | 3,966 | 58,9% |
| ↳ sort | 3,588 | 90,5% dos merges grandes |
| ↳ cópia de volta | 0,378 | 9,5% dos merges grandes |
| Abaixo do corte (< 2160 elementos) | 2,773 | 41,1% |
| **Total instrumentado** | **6,739** | +0,5% em relação à Fase 1 (dentro da variação entre execuções) |

## Pontos quentes

Os pontos quentes aparecem quando olhamos o que acontece **dentro** de cada merge e **quantos** merges existem por nível.

### 1. O laço de sort (intercalação)

- **Onde:** os laços `while` do `merge`.
- **Evidência:** 90,5% do tempo dos merges grandes (3,588 s de 3,966 s).
- **Comportamento:** o tempo do sort fica praticamente igual em todos os níveis (0,236–0,240 s).
- **Conclusão:** o custo não depende de onde os dados estão, então o laço não é limitado pela memória. Os dados indicam que o limite é a comparação `a[i] <= a[j]`.

### 2. A cópia de volta (`tmp` → `a`)

- **Onde:** o laço `for` no fim do `merge`.
- **Evidência:** 9,5% do tempo dos merges grandes (0,378 s).
- **Comportamento:** é a única coluna que cresce nos níveis de cima: ~0,019 s nos níveis 6–14 e 0,046 s no nível 0 (2,4× mais).
- **Conclusão:** a cópia é limitada pela memória. Mesmo eliminando-a por completo, o ganho máximo seria de ~10% do tempo do merge.

### 3. Os merges dos níveis de cima (gargalo sequencial)

- **Onde:** os merges dos níveis 0, 1, 2…, que juntam trechos grandes.
- **Evidência:** a coluna "maior merge". O nível 0 é um único merge de 0,294 s, e o nível 1 são dois merges de 0,149 s.
- **Comportamento:** cada merge é feito por uma thread só. Nos níveis de cima há menos merges que threads (1, 2, 4, …), então paralelizar só as chamadas de `msort` deixa threads paradas nesses níveis.
- **Conclusão:** somando o maior merge de cada nível vamos ter que o caminho crítico é ≈ 0,61 s. Se paralelizarmos só a recursão, a ordenação não fica abaixo disso mesmo com infinitas threads.


## Fração paralelizavel (P)

Podemos paralelizar os merges de um mesmo nivel, o merge mais demorado de cada nível é o caminho crítico então se somarmos esse tempo para todos os niveis vamos descobrir o tempo sequencial.

A soma que encontramos dos niveis 0 a 14 foi de 0,608.

| Parte | Tempo (s) |
|---|---|
| Tempo sequencial total (Fase 1, n = 50.000.000) | 6,705 |
| Caminho crítico (soma da coluna "maior merge") | 0,608 |

1 − P = 0,608 / 6,705 ≈ 0,091 → **P ≈ 0,909**

Ou seja, 9% do tempo continuaria sequencial, concentrado nos merges dos níveis de cima.

## Speedup máximo

Seguindo a lei de Amdahl:

speedup max = 1 / (1 - P)
speedup max = 1 / 0,091
speedup max ≈ **11×**

Com p threads, a lei de Amdahl dá speedup(p) = 1 / ((1 − P) + P / p):

| Threads (p) | speedup |
|---|---|
| 2 | 1,83× |
| 4 | 3,14× |
| 8 | 4,89× |
| 16 | 6,78× |


# Conclusão.
Da forma em que o código está, podemos paralelizar os merges que acontecem em um mesmo nivel sem alterar o código do baseline, mas se quisermos obter ainda mais eficiencia no paralelismo vamos ter de alterar o código que executa um merge especifico para que ele possa também ser paralelizado.
