<#
.SYNOPSIS
  Clean, configure, build, and/or run your Ninja-based project.

.DESCRIPTION
  - -c | -Clean : Remove everything here except this script.
  - -b | -Build : Run CMake configure and ninja.
  - -r | -Run   : Run ninja (if needed) and launch the first .exe.
  
  Behavior:
    • No flags            → configure only  
    • -b                  → configure + build  
    • -r                  → build + run  
    • -b -r               → configure + build + run  
    • + -c at front always cleans first
#>

[CmdletBinding()]
param(
  [Alias('c')][Switch]$Clean,
  [Alias('b')][Switch]$Build,
  [Alias('r')][Switch]$Run
)

# Name of this script, so we don't delete it when cleaning
$scriptName = $MyInvocation.MyCommand.Name

# 1) CLEAN
if ($Clean) {
  Write-Host "🧹 Cleaning directory (excluding $scriptName)..." -ForegroundColor Yellow
  Get-ChildItem -Force |
  Where-Object { $_.Name -ne $scriptName } |
  ForEach-Object {
    try {
      if ($_.PSIsContainer) {
        Remove-Item $_.FullName -Recurse -Force -ErrorAction Stop
      }
      else {
        Remove-Item $_.FullName -Force -ErrorAction Stop
      }
    }
    catch {
      Write-Warning "Could not remove $($_.FullName): $_"
    }
  }
  Write-Host "✅ Clean complete.`n"
}

# Determine if we need to run Ninja at all
$needNinja = $Build -or $Run

# 2) CONFIGURE
# Configure if we're building, or if no build/run flags were given (i.e. default mode)
if ($Build -or (-not $needNinja)) {
  Write-Host "⚙️  Running CMake configure..." -ForegroundColor Cyan
  cmake .. `
    -G "Ninja" `
    -DPREFIXED_RAYLIB=ON `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DCMAKE_C_COMPILER=clang `
    -DCMAKE_CXX_COMPILER=clang++
}

# 3) BUILD
if ($needNinja) {
  Write-Host "`n🔨 Building with ninja..." -ForegroundColor Cyan
  ninja
}

# 4) RUN
if ($Run) {
  Write-Host "`n🔍 Scanning for .exe in $PWD ..." -ForegroundColor Cyan
  $exe = Get-ChildItem -Path . -Filter *.exe -File | Select-Object -First 1

  if (-not $exe) {
    Write-Error "No executable (*.exe) found in current directory!"
    exit 1
  }

  Write-Host "🚀 Launching $($exe.Name) ..." -ForegroundColor Green
  & .\"$($exe.Name)"
}
