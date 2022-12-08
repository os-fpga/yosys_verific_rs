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

ls -la /usr/local/include
ls -la /usr/local/lib




sudo ls -la /usr/lib
sudo ls -la /Library/Apple/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/usr/lib/clang/13.1.6/lib
sudo ls -la /Library/Developer/CommandLineTools/usr/lib/clang/14.0.0/lib
sudo ls -la /Library/Developer/CommandLineTools/usr/lib/clang/13.0.0/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/iOSSupport/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/iOSSupport/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Python.framework/Versions/2.7/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/iOSSupport/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/minitest-5.11.3/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/rake-12.3.3/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/net-telnet-0.2.0/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/xmlrpc-0.3.0/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/power_assert-1.1.3/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/did_you_mean-1.3.0/lib
sudo ls -la /Library/Ruby/Gems/2.6.0/gems/test-unit-3.2.9/lib
sudo ls -la /System/iOSSupport/usr/lib
sudo ls -la /System/Library/ProxySigningStubs/Library/Apple/usr/lib
sudo ls -la /System/Library/Perl/5.30/unicore/lib
sudo ls -la /System/Library/Perl/5.34/unicore/lib
sudo ls -la /System/Library/Tcl/8.5/xotcl1.6.6/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/minitest-5.11.3/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/rake-12.3.3/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/net-telnet-0.2.0/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/xmlrpc-0.3.0/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/power_assert-1.1.3/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/did_you_mean-1.3.0/lib
sudo ls -la /System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/test-unit-3.2.9/lib
sudo ls -la /System/Library/Templates/Data/private/var/lib
sudo ls -la /System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/31001/Libraries/lib
sudo ls -la /System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/31001/Libraries/lib/clang/31001.660/lib
sudo ls -la /System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/Libraries/lib
sudo ls -la /System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/templates/newgem/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/share/ri/2.6.0/system/lib
sudo ls -la /System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /System/Library/Filesystems/acfs.fs/Contents/lib
sudo ls -la /System/Volumes/Preboot/Cryptexes/OS/usr/lib
sudo ls -la /System/Volumes/Preboot/Cryptexes/Incoming/OS/usr/lib
sudo ls -la /System/Volumes/Data/Library/Apple/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/usr/lib/clang/13.1.6/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/usr/lib/clang/14.0.0/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/usr/lib/clang/13.0.0/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/iOSSupport/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX13.0.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/iOSSupport/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Python.framework/Versions/2.7/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.1.sdk/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/iOSSupport/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/SDKs/MacOSX12.3.sdk/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/minitest-5.11.3/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/rake-12.3.3/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/net-telnet-0.2.0/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/xmlrpc-0.3.0/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/power_assert-1.1.3/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/did_you_mean-1.3.0/lib
sudo ls -la /System/Volumes/Data/Library/Ruby/Gems/2.6.0/gems/test-unit-3.2.9/lib
sudo ls -la /System/Volumes/Data/System/iOSSupport/usr/lib
sudo ls -la /System/Volumes/Data/System/Library/ProxySigningStubs/Library/Apple/usr/lib
sudo ls -la /System/Volumes/Data/System/Library/Perl/5.30/unicore/lib
sudo ls -la /System/Volumes/Data/System/Library/Perl/5.34/unicore/lib
sudo ls -la /System/Volumes/Data/System/Library/Tcl/8.5/xotcl1.6.6/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/minitest-5.11.3/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/rake-12.3.3/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/net-telnet-0.2.0/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/xmlrpc-0.3.0/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/power_assert-1.1.3/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/did_you_mean-1.3.0/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/Library/Ruby/Gems/2.6.0/gems/test-unit-3.2.9/lib
sudo ls -la /System/Volumes/Data/System/Library/Templates/Data/private/var/lib
sudo ls -la /System/Volumes/Data/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/31001/Libraries/lib
sudo ls -la /System/Volumes/Data/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/31001/Libraries/lib/clang/31001.660/lib
sudo ls -la /System/Volumes/Data/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/Libraries/lib
sudo ls -la /System/Volumes/Data/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/rubygems/resolver/molinillo/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/templates/newgem/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/thor/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/net-http-persistent/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/fileutils/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/2.6.0/bundler/vendor/molinillo/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/CFPropertyList-2.3.6/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/libxml-ruby-3.2.1/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/nokogiri-1.10.1/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/sqlite3-1.3.13/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/lib/ruby/gems/2.6.0/gems/mini_portile2-2.4.0/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/Ruby.framework/Versions/2.6/usr/share/ri/2.6.0/system/lib
sudo ls -la /System/Volumes/Data/System/Library/Frameworks/OpenCL.framework/Versions/A/lib
sudo ls -la /System/Volumes/Data/System/Library/Filesystems/acfs.fs/Contents/lib
sudo ls -la /System/Volumes/Data/System/DriverKit/usr/lib
sudo ls -la /System/Volumes/Data/private/var/lib
sudo ls -la /System/Volumes/Data/Users/vorak/Library/Caches/com.apple.python/Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib
sudo ls -la /System/Volumes/Data/Users/vorak/yosys_verific_rs/logic_synthesis-rs/abc-rs/lib
sudo ls -la /System/Volumes/Data/Users/vorak/yosys_verific_rs/build/logic_synthesis-rs/lib
sudo ls -la /System/Volumes/Data/Users/vorak/yosys_verific_rs/Raptor_Tools/Flex_LM/x64_lsb/activation/lib
sudo ls -la /System/Volumes/Data/Applications/Microsoft Excel.app/Contents/Resources/sdx/FA000000018/cardview/lib
sudo ls -la /System/Volumes/Data/Applications/LibreOffice.app/Contents/Frameworks/LibreOfficePython.framework/Versions/3.8/lib
sudo ls -la /System/Volumes/Data/Applications/Microsoft Teams.app/Contents/Resources/app.asar.unpacked/node_modules/oneauth/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Taps/homebrew/homebrew-cask/cmd/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/test/support/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/webrick-1.7.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/patchelf-1.4.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/sorbet-runtime-0.5.10461/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-sorbet-0.6.11/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/ruby-progressbar-1.11.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/bindata-2.4.14/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/mechanize-2.8.5/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/activesupport-6.1.7/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/simplecov_json_formatter-0.1.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-performance-1.15.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/tzinfo-2.0.5/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/plist-3.6.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/did_you_mean-1.6.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/public_suffix-5.0.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/elftools-1.2.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/addressable-2.8.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/warning-1.3.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-rspec-2.15.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubyntlm-0.6.3/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/zeitwerk-2.6.6/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/ruby-macho-3.0.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/concurrent-ruby-1.1.10/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/i18n-1.12.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rack-3.0.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-rails-2.17.3/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libtiff/4.4.0_1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/gmp/6.2.1_1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libidn2/2.3.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libpng/1.6.39/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/qt@5/5.15.7/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/freetype/2.12.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/mpfr/4.1.0-p13/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libunistring/1.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/glib/2.74.3/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/glib/2.74.3/share/gdb/auto-load/opt/homebrew/Cellar/glib/2.74.3/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/readline/8.2.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/hiredis/1.1.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/lz4/1.9.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/gcc/12.2.0/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/webp/1.2.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/xz/5.2.9/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/bash/5.2.12/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/giflib/5.2.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/openssl@3/3.0.7/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/gawk/5.2.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/gettext/0.21.1/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/zstd/1.5.2/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/isl/0.25/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/pcre2/10.40/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/openssl@1.1/1.1.1s/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/flex/2.6.4_2/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/bison/3.8.2/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libffi/3.4.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/jpeg-turbo/2.1.4/lib
sudo ls -la /System/Volumes/Data/opt/homebrew/Cellar/libmpc/1.2.1/lib
sudo ls -la /System/Volumes/Data/opt/X11/lib
sudo ls -la /private/var/lib
sudo ls -la /Users/vorak/Library/Caches/com.apple.python/Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib
sudo ls -la /Users/vorak/yosys_verific_rs/logic_synthesis-rs/abc-rs/lib
sudo ls -la /Users/vorak/yosys_verific_rs/build/logic_synthesis-rs/lib
sudo ls -la /Users/vorak/yosys_verific_rs/Raptor_Tools/Flex_LM/x64_lsb/activation/lib
sudo ls -la /Applications/Microsoft Excel.app/Contents/Resources/sdx/FA000000018/cardview/lib
sudo ls -la /Applications/LibreOffice.app/Contents/Frameworks/LibreOfficePython.framework/Versions/3.8/lib
sudo ls -la /Applications/Microsoft Teams.app/Contents/Resources/app.asar.unpacked/node_modules/oneauth/lib
sudo ls -la /opt/homebrew/Library/Taps/homebrew/homebrew-cask/cmd/lib
sudo ls -la /opt/homebrew/Library/Homebrew/test/support/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/webrick-1.7.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/patchelf-1.4.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/sorbet-runtime-0.5.10461/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-sorbet-0.6.11/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/ruby-progressbar-1.11.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/bindata-2.4.14/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/mechanize-2.8.5/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/activesupport-6.1.7/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/simplecov_json_formatter-0.1.4/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-performance-1.15.1/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/tzinfo-2.0.5/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/plist-3.6.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/did_you_mean-1.6.1/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/public_suffix-5.0.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/elftools-1.2.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/addressable-2.8.1/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/warning-1.3.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-rspec-2.15.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubyntlm-0.6.3/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/zeitwerk-2.6.6/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/ruby-macho-3.0.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/concurrent-ruby-1.1.10/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/i18n-1.12.0/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rack-3.0.1/lib
sudo ls -la /opt/homebrew/Library/Homebrew/vendor/bundle/ruby/2.6.0/gems/rubocop-rails-2.17.3/lib
sudo ls -la /opt/homebrew/lib
sudo ls -la /opt/homebrew/Cellar/libtiff/4.4.0_1/lib
sudo ls -la /opt/homebrew/Cellar/gmp/6.2.1_1/lib
sudo ls -la /opt/homebrew/Cellar/libidn2/2.3.4/lib
sudo ls -la /opt/homebrew/Cellar/libpng/1.6.39/lib
sudo ls -la /opt/homebrew/Cellar/qt@5/5.15.7/lib
sudo ls -la /opt/homebrew/Cellar/freetype/2.12.1/lib
sudo ls -la /opt/homebrew/Cellar/mpfr/4.1.0-p13/lib
sudo ls -la /opt/homebrew/Cellar/libunistring/1.0/lib
sudo ls -la /opt/homebrew/Cellar/glib/2.74.3/lib
sudo ls -la /opt/homebrew/Cellar/glib/2.74.3/share/gdb/auto-load/opt/homebrew/Cellar/glib/2.74.3/lib
sudo ls -la /opt/homebrew/Cellar/readline/8.2.1/lib
sudo ls -la /opt/homebrew/Cellar/hiredis/1.1.0/lib
sudo ls -la /opt/homebrew/Cellar/lz4/1.9.4/lib
sudo ls -la /opt/homebrew/Cellar/gcc/12.2.0/lib
sudo ls -la /opt/homebrew/Cellar/webp/1.2.4/lib
sudo ls -la /opt/homebrew/Cellar/xz/5.2.9/lib
sudo ls -la /opt/homebrew/Cellar/bash/5.2.12/lib
sudo ls -la /opt/homebrew/Cellar/giflib/5.2.1/lib
sudo ls -la /opt/homebrew/Cellar/openssl@3/3.0.7/lib
sudo ls -la /opt/homebrew/Cellar/gawk/5.2.1/lib
sudo ls -la /opt/homebrew/Cellar/gettext/0.21.1/lib
sudo ls -la /opt/homebrew/Cellar/zstd/1.5.2/lib
sudo ls -la /opt/homebrew/Cellar/isl/0.25/lib
sudo ls -la /opt/homebrew/Cellar/pcre2/10.40/lib
sudo ls -la /opt/homebrew/Cellar/openssl@1.1/1.1.1s/lib
sudo ls -la /opt/homebrew/Cellar/flex/2.6.4_2/lib
sudo ls -la /opt/homebrew/Cellar/bison/3.8.2/lib
sudo ls -la /opt/homebrew/Cellar/libffi/3.4.4/lib
sudo ls -la /opt/homebrew/Cellar/jpeg-turbo/2.1.4/lib
sudo ls -la /opt/homebrew/Cellar/libmpc/1.2.1/lib
sudo ls -la /opt/X11/lib
