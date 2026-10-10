<#
.SYNOPSIS
    Compile tout le projet : le moteur natif et les plugins (make, via MSYS2), puis l'application (.NET).

.EXAMPLE
    .\build.ps1                Compile le moteur, les plugins et l'application
    .\build.ps1 -Test          Lance aussi les tests des plugins
    .\build.ps1 -Publish       Crée en plus le dossier dist\, copiable sur un autre PC
    .\build.ps1 -Run           Lance l'application une fois compilée
#>
param(
    [switch]$Test,
    [switch]$Publish,
    [switch]$Run,
    # Dossier d'installation de MSYS2
    [string]$Msys2 = "C:\msys64"
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$bash = Join-Path $Msys2 "usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    throw "MSYS2 introuvable dans $Msys2. Installe-le (winget install MSYS2.MSYS2) ou passe -Msys2 <dossier>."
}

$dotnet = (Get-Command dotnet -ErrorAction SilentlyContinue).Source
if (-not $dotnet) { $dotnet = Join-Path $env:ProgramFiles "dotnet\dotnet.exe" }
if (-not (Test-Path $dotnet)) {
    throw "SDK .NET introuvable. Installe-le : winget install Microsoft.DotNet.SDK.8"
}

# L'application verrouille ses DLL tant qu'elle tourne
if (Get-Process VoiceChangerApp -ErrorAction SilentlyContinue) {
    throw "Voice Changer est en cours d'exécution : ferme-le avant de compiler."
}

# make doit tourner dans l'environnement UCRT64 de MSYS2, depuis le dossier du projet
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"

$target = if ($Test) { "all test" } else { "all" }
Write-Host "== Moteur natif et plugins (make $target)" -ForegroundColor Cyan
& $bash -lc "make $target"
if ($LASTEXITCODE -ne 0) { throw "make a échoué (code $LASTEXITCODE)." }

if ($Publish) {
    Write-Host "== Application et dossier dist" -ForegroundColor Cyan
    & $dotnet publish ui -c Release -r win-x64 --self-contained true -o dist --nologo -v q
    $exe = Join-Path $PSScriptRoot "dist\VoiceChangerApp.exe"
} else {
    Write-Host "== Application" -ForegroundColor Cyan
    & $dotnet build ui -c Release --nologo -v q
    $exe = Join-Path $PSScriptRoot "ui\bin\Release\net8.0-windows\win-x64\VoiceChangerApp.exe"
}
if ($LASTEXITCODE -ne 0) { throw "La compilation de l'application a échoué (code $LASTEXITCODE)." }

Write-Host "Terminé : $exe" -ForegroundColor Green
if ($Run) { Start-Process $exe }
