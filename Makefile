# Uso:
#   make baseline | make mergesort | make mergesect | make perfilamento
#   make all                      # roda baseline, mergesort e mergesect
#   make mergesect N=1000000 THREADS=8
#   make clean

N       ?= 20000000
THREADS ?= 4

export N THREADS
export UID := $(shell id -u)
export GID := $(shell id -g)

COMPOSE := docker compose run --rm --quiet-pull

.PHONY: all build baseline mergesort mergesect perfilamento clean

all: baseline mergesort mergesect

build:
	docker compose build

baseline mergesort mergesect perfilamento: build
	@echo "== $@ (N=$(N), THREADS=$(THREADS))"
	@$(COMPOSE) $@

clean:
	docker run --rm -v "$(CURDIR)":/app -w /app gcc:14 rm -rf bin
