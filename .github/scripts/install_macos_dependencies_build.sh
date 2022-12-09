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

ver=`/usr/local/opt/openssl@1.1/bin/openssl version`
num_ver=${ver:8:5}

ln -s $(brew --prefix openssl@1.1)/include/openssl /usr/local/include
ln -s $(brew --prefix openssl@1.1)/lib/libssl.${num_ver}.dylib /usr/local/lib
ln -s $(brew --prefix openssl@1.1)/lib/libcrypto.${num_ver}.dylib /usr/local/lib
ln -s $(brew --prefix openssl@1.1)/lib/libssl.a /usr/local/lib
ln -s $(brew --prefix openssl@1.1)/lib/libcrypto.a /usr/local/lib
