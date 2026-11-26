param([int]$Repeticoes = 5)
$ErrorActionPreference = 'Stop'
if ($Repeticoes -lt 1) { throw 'Repeticoes deve ser positivo.' }
$arquivo = Join-Path (Get-Location) ('benchmarks-v2-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.csv')
$configuracoes = @(
    @{Servico='baseline'; P=0; T=1},
    @{Servico='mpi'; P=2; T=4},
    @{Servico='mpirevisado'; P=2; T=4}
)
foreach ($par in @('1,1','1,2','1,4','1,8','2,1','2,2','2,4','4,1','4,2','8,1')) {
    $p,$t = $par.Split(',')
    $configuracoes += @{Servico='mpiv2'; P=[int]$p; T=[int]$t}
}
$variaveis = @('N','PROCS','THREADS')
$anteriores = @{}
foreach ($v in $variaveis) { $anteriores[$v] = [Environment]::GetEnvironmentVariable($v,'Process') }
try {
    docker compose config --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Docker Compose invalido.' }
    $contador = 0
    $total = 2 * $configuracoes.Count * $Repeticoes
    foreach ($n in @(1000000,20000000)) {
        $env:N = "$n"
        for ($rep=1; $rep -le $Repeticoes; $rep++) {
            foreach ($c in $configuracoes) {
                $env:PROCS = "$( [Math]::Max(1,$c.P) )"
                $env:THREADS = "$($c.T)"
                $contador++
                Write-Host "[$contador/$total] $($c.Servico) n=$n p=$($c.P) t=$($c.T) rep=$rep"
                $saida = @(docker compose run --rm $c.Servico)
                $codigo = $LASTEXITCODE
                $saida | ForEach-Object { Write-Host $_ }
                if ($codigo -ne 0) { throw "Falha no Docker: $($c.Servico), codigo $codigo" }
                $linha = @($saida | Where-Object { $_ -match 'tempo\s*=\s*[0-9]+(?:[.,][0-9]+)?' })
                if ($linha.Count -ne 1) { throw 'Saida inesperada: nao foi encontrado exatamente um tempo.' }
                $tempo = [regex]::Match($linha[0], 'tempo\s*=\s*([0-9]+(?:[.,][0-9]+)?)').Groups[1].Value.Replace(',','.')
                $validacao = [regex]::Match($linha[0], 'ordenado\s*=\s*(SIM|NAO|NÃO)', 'IgnoreCase')
                $status = if ($validacao.Success) { $validacao.Groups[1].Value.ToUpperInvariant() } else { 'NAO_INFORMADO' }
                [pscustomobject]@{implementacao=$c.Servico; n=$n; processos=$c.P; threads_por_processo=$c.T; total_threads=([Math]::Max(1,$c.P)*$c.T); repeticao=$rep; tempo_s=$tempo; ordenado=$status} | Export-Csv -Path $arquivo -NoTypeInformation -Append -Encoding UTF8
                if ($status -ne 'SIM') { throw "Validacao ausente ou negativa. Confira a saida; CSV parcial: $arquivo" }
            }
        }
    }
    Write-Host "Concluido: $arquivo"
} finally {
    foreach ($v in $variaveis) { [Environment]::SetEnvironmentVariable($v,$anteriores[$v],'Process') }
}
