set -x
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
    openssl@1.1 \
    gcc

#find /usr/local/ -name *libssl*
#find /usr/local/ -name *libcrypto*
#
#ln -s $(brew --prefix openssl@1.1)/include/openssl /usr/local/include
#ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.dylib /usr/local/lib
#ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.a /usr/local/lib
#ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.a /usr/local/lib
#ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.dylib /usr/local/lib
#
#ls -la /usr/local//Cellar/openssl@1.1/1.1.1s/lib
