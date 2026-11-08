# Uso:
#   make baseline | make mergesort | make mergesect
#   make all                      # roda os três em sequência
#   make mergesect N=1000000 THREADS=8
#   make clean

N       ?= 20000000
THREADS ?= 4

export N THREADS
export UID := $(shell id -u)
export GID := $(shell id -g)

COMPOSE := docker compose run --rm --quiet-pull

.PHONY: all build baseline mergesort mergesect clean

all: baseline mergesort mergesect

build:
	docker compose build

baseline mergesort mergesect: build
	@echo "== $@ (N=$(N), THREADS=$(THREADS))"
	@$(COMPOSE) $@

clean:
	docker run --rm -v "$(CURDIR)":/app -w /app gcc:14 rm -rf bin
