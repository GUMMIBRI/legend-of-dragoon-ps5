param(
    [Parameter(Mandatory)]
    [ValidatePattern('^dragoon-[a-z0-9]+(?:-[a-z0-9]+)*$')]
    [string]$BuildId,
    [switch]$DryRun
)

# Reuse the retained PS5 build tree and native link inputs.
$platformRoot = 'C:/.pe/ps5-native/gpu-platform-20261030'
$llvmBin = "$platformRoot/deps/llvm/bin"
$nativeTool = "$platformRoot/native-candidates/tool/ps5-native-tool.exe"
$stubDirectory = "$platformRoot/deps/ps5-payload-sdk/target/lib"
$ninja = 'C:/.pe/tools/ninja/ninja.exe'
$linkResponse = 'dragoon-pe1-isolated.rsp'
$previousPath = $env:Path
$previousVulkanSdk = $env:VULKAN_SDK

function Invoke-BuildStep([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE."
    }
}

Push-Location -LiteralPath (Join-Path $PSScriptRoot 'local-codegen/build-ps5-title') -ErrorAction Stop
try {
    if (!$DryRun -and (Test-Path -LiteralPath "$BuildId.bin")) {
        throw "Build $BuildId already exists. Use a new build ID."
    }
    $env:Path = "$llvmBin;$previousPath"
    $env:VULKAN_SDK = "$platformRoot/deps/radv-release"
    $targets = @(
        Get-Content -LiteralPath $linkResponse -ErrorAction Stop | ForEach-Object {
            if ($_ -match '^"((?:CMakeFiles/.*\.o|_deps/.*\.a|libpsx_texture_image_decode\.a))"$') {
                $Matches[1]
            }
        }
    )
    if ($targets.Count -eq 0) {
        throw "No compile targets found in $linkResponse."
    }
    for ($offset = 0; $offset -lt $targets.Count; $offset += 30) {
        $last = [Math]::Min($offset + 29, $targets.Count - 1)
        $ninjaArguments = @()
        if ($DryRun) {
            $ninjaArguments += '-n'
        }
        $ninjaArguments += $targets[$offset..$last]
        Invoke-BuildStep $ninja $ninjaArguments
    }
    if ($DryRun) {
        return
    }

    Invoke-BuildStep "$llvmBin/ld.lld.exe" @("@$linkResponse", 'ps5_startup_log.o', '-o', "$BuildId.elf")
    Invoke-BuildStep $nativeTool @('link', '--in', "$BuildId.elf", '--out', "$BuildId-app.elf",
                                    '--stub-dir', $stubDirectory, '--file-name', 'eboot.elf')
    Invoke-BuildStep $nativeTool @('self', '--sign', '--in', "$BuildId-app.elf",
                                    '--out', "$BuildId.bin", '--magic', '0x1D3D154F')
    Invoke-BuildStep $nativeTool @('self', '--inspect', '--file', "$BuildId.bin")
    Get-FileHash -LiteralPath "$BuildId.elf", "$BuildId-app.elf", "$BuildId.bin" -Algorithm SHA256
} finally {
    $env:Path = $previousPath
    $env:VULKAN_SDK = $previousVulkanSdk
    Pop-Location
}
