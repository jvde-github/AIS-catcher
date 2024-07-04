FROM debian:bookworm

# Install build dependencies
RUN apt-get update && apt-get install -y 

# Copy the project files to the container
COPY . /app
WORKDIR /app

# Build the project
RUN ./build.me "cmake build-essential pkg-config" ais-catcher amd64 1.0 ""

# Copy the generated Debian package to the host system
CMD ["cp", "ais-catcher.deb", "/output/"]
