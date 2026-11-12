FROM gcc:14

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    openmpi-bin \
    libopenmpi-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
