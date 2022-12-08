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
    openssl@3.0.7

ln -s $(brew --prefix openssl)/include/openssl /usr/local/include
ln -s $(brew --prefix openssl)/lib/libssl.3.0.7.dylib /usr/local/lib
ln -s $(brew --prefix openssl)/lib/libcrypto.3.0.7.dylib /usr/local/lib

ls -la /usr/local/include
ls -la /usr/local/lib
