
function ExecuteAction {
    param (
        $submodule_path
    )
    Write-Output "submodule_path = $submodule_path "
    Set-Location $submodule_path
    pwsh.exe -File .\action.ps1 -All -WorkingDirectory $submodule_path
    Set-Location -Path $root
}
$dir_count = 4
$root  = Get-Location
Write-Output $root
<#
$env:PATH += (Test-Path -Path "C:\cygwin64\bin") ? "C:\cygwin64\bin\" : "C:\cygwin\bin\"
$env:PATH -split ";"
#>
New-Variable -Name "yosys_path" -Value ".\yosys"
New-Variable -Name "abc_path" -Value ".\logic_synthesis-rs\abc-rs\"
New-Variable -Name "verific_path" -Value ".\Raptor_Tools\verific_rs"
New-Variable -Name "plugin_path" -Value ".\yosys-rs-plugin\"

[string[]]$pathsArray = $yosys_path,$verific_path,$plugin_path,$abc_path

Write-Output "Started action"

for($i = 0;$i -lt $dir_count;++$i){
    Write-Output  "Iteration $i "
    ExecuteAction $pathsArray[$i]
}

#Build YosysVS.
Set-Location $root
msbuild yosys_verific_rs_VS.sln /t:YosysVS /p:Configuration=Release /p:PlatformTarget=x64


Write-Output "Action End"
