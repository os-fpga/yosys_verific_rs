
Step by step see the WindowsBuild.md or action.ps1 files for yosys_verific_rs Windows Version
You need to install the following dependencies
	- python3
	- visual studio 2019(with C++ Desktop development tool chain)
	- cygwin64  https://www.cygwin.com/setup-x86_64.exe

	
	1: yosys_verific_rs\Raptor_Tools\verific_rs\Windows_Build.md or action.ps1
	2: yosys_verific_rs\logic_synthesis-rs\abc-rs\Windows_Build.md or action.ps1
	3: yosys_verific_rs\yosys\Windows_Build.md or action.ps1
	4: yosys_verific_rs\yosys-rs-plugin\Windows_Build.md or action.ps1  
	5: run on PowerShell Terminal in yosys_verific_rs directory   
	```bash
		 devenv yosys_verific_rs_VS.sln
	```
	6: build YosysVS in Visual Studio 
