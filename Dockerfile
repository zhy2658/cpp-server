# --- Build Stage ---
FROM debian:bookworm-slim AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY . .

# Build the project (Release mode)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release -j$(nproc)

# --- Runtime Stage ---
FROM debian:bookworm-slim

# Create a non-root user for security
RUN useradd -m -u 1000 serveruser
WORKDIR /app

# Copy artifacts from builder
COPY --from=builder /app/build/server /app/server
COPY --from=builder /app/config.yaml /app/config.yaml

# Set ownership
RUN chown -R serveruser:serveruser /app

# Switch to non-root user
USER serveruser

# Expose UDP port
EXPOSE 8888/udp

# Run the server
CMD ["./server"]