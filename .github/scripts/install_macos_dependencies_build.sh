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
    openssl@1.1

find /usr/local/ -name *libssl*
find /usr/local/ -name *libcrypto*

ln -s $(brew --prefix openssl@1.1)/include/openssl /usr/local/include
ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.dylib /usr/local/lib/libssl.dylib
ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.a /usr/local/lib/libssl.a
ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.a /usr/local/lib/libcrypto.a
ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.dylib /usr/local/lib/libcrypto.dylib
sudo ln -s $(brew --prefix openssl@1.1)/include/openssl /usr/include
sudo ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.dylib /usr/lib/libssl.dylib
sudo ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libssl.a /usr/lib/libssl.a
sudo ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.a /usr/lib/libcrypto.a
sudo ln -s /usr/local//Cellar/openssl@1.1/1.1.1s/lib/libcrypto.dylib /usr/lib/libcrypto.dylib
