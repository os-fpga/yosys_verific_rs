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

ln -s $(brew --prefix openssl)/include/openssl /usr/local/include
ln -s $(brew --prefix openssl)/lib/libssl* /usr/local/lib
ln -s $(brew --prefix openssl)/lib/libcrypto* /usr/local/lib

ls -la /usr/local/lib
