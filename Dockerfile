# Stage 1: Build environment with GCC 14 and CMake 3.31+
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-14 \
    g++-14 \
    make \
    git \
    wget \
    libssl-dev \
    libsqlite3-dev \
    libomp-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 14 as default compiler
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 --slave /usr/bin/g++ g++ /usr/bin/g++-14

# Install official CMake 3.31 binary from Kitware (required by Glaze)
RUN wget -q https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5-linux-x86_64.tar.gz && \
    tar -xzf cmake-3.31.5-linux-x86_64.tar.gz -C /usr/local --strip-components=1 && \
    rm cmake-3.31.5-linux-x86_64.tar.gz

WORKDIR /app/Back-end

# Copy Back-end source code
COPY Back-end/ /app/Back-end/

# Build and verify with test suites
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && \
    ./bin/modern_cpp_test && \
    ./bin/e2e_network_test

# Stage 2: Minimal runtime image
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    libssl3 \
    libgomp1 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binaries from builder stage
COPY --from=builder /app/Back-end/build/bin/ /app/bin/

EXPOSE 9999

ENTRYPOINT ["/app/bin/cplusplusproject2022fall"]
CMD ["9999"]