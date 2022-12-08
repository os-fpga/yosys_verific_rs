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
    openssl@3

# sudo find / -type d -name lib | xargs ls -la

ver=`/usr/local/opt/openssl@3/bin/openssl version`
num_ver=${ver:8:5}

ln -s $(brew --prefix openssl)/include/openssl /usr/local/include
ln -s $(brew --prefix openssl)/lib/libssl.${num_ver}.dylib /usr/local/lib
ln -s $(brew --prefix openssl)/lib/libcrypto.${num_ver}.dylib /usr/local/lib
ln -s $(brew --prefix openssl)/lib/libssl.${num_ver}.a /usr/local/lib
ln -s $(brew --prefix openssl)/lib/libcrypto.${num_ver}.a /usr/local/lib

ls -la -R $(brew --prefix openssl)
ls -la -R /usr/local/Cellar
ls -la -R /usr/local/opt

ls -la /usr/local/include
ls -la /usr/local/lib
