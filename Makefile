# Uso com Docker Compose (na pasta projeto)
#   make all              # executa baseline, openmpv1 e openmpv2
#   make build            # constrói a imagem compartilhada
#   make clean            # remove os binários de bin/
#
# Execute um alvo com make <alvo>
# Versão            Ordenação      Perfilamento
# Sequencial        baseline       perfilamento
# OpenMP V1         openmpv1       openmpv1perfilamento
# OpenMP V2         openmpv2       openmpv2perfilamento
# MPI V1            mpi            mpiperfilamento
# MPI V1 revisada   mpirevisado    mpiperfilamentorevisado
# MPI V2            mpiv2          mpiv2perfilamento
#
# Exemplos (N = elementos, PROCS = processos MPI, THREADS = threads por processo)
#   make openmpv2 N=10001 THREADS=2
#   make mpiv2 N=10001 PROCS=2 THREADS=2
#   make mpiv2perfilamento N=10001 PROCS=2 THREADS=2 REPS=2 AQUEC=1 CORTE=100

N       ?= 20000000
THREADS ?= 8

export N THREADS PROCS REPS AQUEC CORTE
export UID := $(shell id -u)
export GID := $(shell id -g)

COMPOSE := docker compose run --rm --quiet-pull

SERVICOS := baseline perfilamento openmpv1 openmpv2 \
            openmpv1perfilamento openmpv2perfilamento \
            mpi mpiperfilamento mpirevisado mpiperfilamentorevisado mpiv2 mpiv2perfilamento

.PHONY: all build clean $(SERVICOS)

all: baseline openmpv1 openmpv2

build:
	docker compose build baseline

$(SERVICOS): build
	@echo "== $@ (N=$(N), THREADS=$(THREADS))"
	@$(COMPOSE) $@

clean:
	docker run --rm -v "$(CURDIR)":/app -w /app gcc:14 rm -rf bin
