
function ExecuteAction {
    param (
        $path
    )
    Write-Output "path = $path "
    Set-Location $path
    pwsh.exe .\action.ps1
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
New-Variable -Name "verific_path" -Value ".\aptor_Tools\verific_rs"
New-Variable -Name "plugin_path" -Value ".\osys-rs-plugin\"
New-Variable -Name "yosys_verific_rs_path" -Value "."

[string[]]$pathsArray = $yosys_path,$verific_path,$plugin_path,$abc_path

Write-Output "Started action"

for($i = 0;$i -lt $dir_count;++$i){
    Write-Output  "Iteration $i "
    ExecuteAction $pathsArray[$i]
}

#Build YosysVS.
Set-Location $yosys_verific_rs_path
msbuild yosys_verific_rs_VS.sln /t:YosysVS /p:Configuration=Release /p:PlatformTarget=x64


Write-Output "Action End"
