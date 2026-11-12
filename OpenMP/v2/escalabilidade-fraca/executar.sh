#!/usr/bin/env bash
set -euo pipefail

# Resolve o fonte mesmo quando o script é chamado de outra pasta.
pasta=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
binario=$(mktemp /tmp/escalabilidade-fraca.XXXXXX)
trap 'rm -f -- "$binario"' EXIT

g++ -O2 -fopenmp "$pasta/../openmpV2Perfilamento.cpp" -o "$binario"

unset GOMP_CPU_AFFINITY GOMP_SPINCOUNT
export OMP_DYNAMIC=false OMP_PROC_BIND=close OMP_PLACES=cores
export OMP_THREAD_LIMIT=16 OMP_MAX_ACTIVE_LEVELS=1 OMP_DISPLAY_ENV=false

for p in 1 2 4 8 16; do
    n=$((p * 1000000))
    echo "Threads: $p | Elementos: $n"
    # 10 repetições medidas, 2 aquecimentos e corte de 2160.
    # A Fase 2 também roda; mostramos somente o resultado da Fase 1.
    OMP_NUM_THREADS="$p" "$binario" "$n" 10 2 2160 | grep '^\[Fase 1\]'
done
