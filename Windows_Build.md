# Windows Raptor build
----
### You need to install the following dependencies
*	- python3
*	- visual studio 2019(with C++ Desktop development tool chain)
*	- cygwin64  https://www.cygwin.com/setup-x86_64.exe
* 	- OpenSSL -  https://slproweb.com/download/Win64OpenSSL-3_0_7.exe
*	- PowerShell - https://github.com/PowerShell/PowerShell/releases/download/v7.3.0/PowerShell-7.3.0-win-x64.msi

----
## Build yosys_verific_rs
Open yosys_verific_rs directory in Visual Studio and follow the guidence at https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170 to switch to the CMake targets, set the configuratio Release and CMake command arguments -DPRODUCTION_BUILD=1 in the CMakeSettings.json and build the project.

