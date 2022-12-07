# Install required dependencies for Mac OS systems
brew install bison \
    flex \
    gawk \
    libffi \
    pkg-config \
    bash \
    readline \
    ninja \
    wget \
    coreutils \
    openssl
brew upgrade openssl
brew link --force openssl
pkg-config --modversion openssl
