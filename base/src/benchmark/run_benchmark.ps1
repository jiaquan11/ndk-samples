# SPDX-License-Identifier: Apache-2.0

param(
    [switch]$IncludeLogd,
    [switch]$Quick,
    [string]$DeviceSerial = ""
)

$ErrorActionPreference = "Stop"

function Get-AndroidSdkPath {
    if ($env:ANDROID_SDK_ROOT) {
        return $env:ANDROID_SDK_ROOT
    }
    if ($env:ANDROID_HOME) {
        return $env:ANDROID_HOME
    }

    $localProperties = Join-Path $repositoryRoot "local.properties"
    $sdkLine = Get-Content $localProperties |
        Where-Object { $_ -match '^sdk\.dir=' } |
        Select-Object -First 1
    if (-not $sdkLine) {
        throw "未找到 Android SDK，请设置 ANDROID_SDK_ROOT 或检查 local.properties"
    }
    $sdkPath = $sdkLine.Substring("sdk.dir=".Length)
    return $sdkPath.Replace("\:", ":").Replace("\\", "\")
}

function Get-DeviceProperty([string]$name) {
    $value = & $adb -s $selectedDevice shell getprop $name
    if ($LASTEXITCODE -ne 0 -or $null -eq $value) {
        throw "读取设备属性失败: $name"
    }
    return $value.Trim()
}

function Convert-ToCsvValue([string]$value) {
    return $value.Replace(",", "_").Replace("`r", "").Replace("`n", " ")
}

function Get-WorktreeState {
    $status = @(& git status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) {
        throw "读取 Git 工作区状态失败"
    }
    if ($status.Count -eq 0) {
        return [PSCustomObject]@{ Name = "clean"; Fingerprint = "none" }
    }

    $material = [System.Text.StringBuilder]::new()
    [void]$material.AppendLine(($status -join "`n"))
    [void]$material.AppendLine((& git diff --binary HEAD | Out-String))
    $untrackedFiles = @(& git ls-files --others --exclude-standard)
    foreach ($untrackedFile in $untrackedFiles) {
        $fileHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $untrackedFile).Hash
        [void]$material.AppendLine("$untrackedFile=$fileHash")
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($material.ToString())
        $hash = ($sha256.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join ""
    } finally {
        $sha256.Dispose()
    }
    return [PSCustomObject]@{ Name = "dirty"; Fingerprint = $hash.Substring(0, 16) }
}

function Get-BatterySnapshot {
    $battery = & $adb -s $selectedDevice shell dumpsys battery
    if ($LASTEXITCODE -ne 0) {
        throw "读取设备电池与温度信息失败"
    }
    $levelMatch = $battery | Select-String '^\s*level:' | Select-Object -First 1
    $temperatureMatch = $battery | Select-String '^\s*temperature:' | Select-Object -First 1
    if (-not $levelMatch -or -not $temperatureMatch) {
        throw "设备电池信息缺少 level 或 temperature 字段"
    }
    $level = ($levelMatch.Line -replace '^\s*level:\s*', '').Trim()
    $temperature = ($temperatureMatch.Line -replace '^\s*temperature:\s*', '').Trim()
    return [PSCustomObject]@{ Level = $level; Temperature = $temperature }
}

function Get-BenchmarkSourceFingerprint {
    $sourceFiles = @(
        "base/src/main/cpp/include/base/logging.h"
        "base/src/main/cpp/logging.cpp"
        "base/src/main/cpp/logging_internal.h"
        "base/src/main/cpp/logging_splitters.h"
        "base/src/main/cpp/CMakeLists.txt"
        "base/build.gradle.kts"
        "base/src/benchmark/cpp/base_logging_benchmark.cpp"
        "base/src/benchmark/run_benchmark.ps1"
        "unit-test/app/build.gradle"
        "unit-test/app/src/main/cpp/CMakeLists.txt"
        "cmake/AppLibrary.cmake"
        "build-logic/src/main/java/com/android/ndk/samples/buildlogic/AndroidBuildConfig.kt"
        "build-logic/src/main/java/com/android/ndk/samples/buildlogic/AndroidApplicationConventionPlugin.kt"
        "build-logic/src/main/java/com/android/ndk/samples/buildlogic/AndroidLibraryConventionPlugin.kt"
        "build-logic/build.gradle.kts"
        "build.gradle"
        "settings.gradle"
        "gradle/libs.versions.toml"
        "gradle/wrapper/gradle-wrapper.properties"
    )
    $material = [System.Text.StringBuilder]::new()
    foreach ($sourceFile in $sourceFiles) {
        $fileHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceFile).Hash
        [void]$material.AppendLine("$sourceFile=$fileHash")
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($material.ToString())
        $hash = ($sha256.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join ""
    } finally {
        $sha256.Dispose()
    }
    return $hash.Substring(0, 16)
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$sdkRoot = Get-AndroidSdkPath
$adb = Join-Path $sdkRoot "platform-tools\adb.exe"
if (-not (Test-Path $adb)) {
    throw "未找到 adb: $adb"
}

Push-Location $repositoryRoot
try {
    & .\gradlew.bat ':unit-test:app:buildCMakeRelWithDebInfo[arm64-v8a]'
    if ($LASTEXITCODE -ne 0) {
        throw "日志性能基准构建失败"
    }

    $activeCompileDatabase = Join-Path $repositoryRoot `
        "unit-test\app\.cxx\tools\release\arm64-v8a\compile_commands.json"
    if (-not (Test-Path $activeCompileDatabase)) {
        throw "未找到 AGP 当前 arm64-v8a Release 编译数据库"
    }
    $compileCommands = Get-Content -Raw $activeCompileDatabase | ConvertFrom-Json
    if (-not $compileCommands -or -not $compileCommands[0].directory) {
        throw "AGP 当前编译数据库不包含有效构建目录"
    }
    $activeBuildDirectory = Get-Item -LiteralPath $compileCommands[0].directory
    if ($activeBuildDirectory.Name -ne "arm64-v8a") {
        throw "AGP 当前编译数据库指向了非 arm64-v8a 目录"
    }
    $configurationHash = $activeBuildDirectory.Parent.Name
    $artifactPath = Join-Path $repositoryRoot `
        "unit-test\app\build\intermediates\cxx\RelWithDebInfo\$configurationHash\obj\arm64-v8a\base_logging_benchmark"
    $artifact = Get-Item -LiteralPath $artifactPath -ErrorAction SilentlyContinue
    if (-not $artifact) {
        throw "未找到 arm64-v8a 日志性能基准产物"
    }

    $deviceOutput = & $adb devices
    if ($LASTEXITCODE -ne 0) {
        throw "读取 adb 设备列表失败"
    }
    $connectedDevices = @(
        $deviceOutput |
            Select-Object -Skip 1 |
            ForEach-Object {
                if ($_ -match '^(\S+)\s+device$') { $Matches[1] }
            }
    )
    if ($DeviceSerial) {
        if ($connectedDevices -notcontains $DeviceSerial) {
            throw "指定设备未连接: $DeviceSerial"
        }
        $selectedDevice = $DeviceSerial
    } elseif ($connectedDevices.Count -eq 1) {
        $selectedDevice = $connectedDevices[0]
    } else {
        throw "需要且只能自动选择一台在线设备；也可以通过 -DeviceSerial 指定"
    }

    $commit = (& git rev-parse --short=12 HEAD).Trim()
    $worktree = Get-WorktreeState
    $sourceFingerprint = Get-BenchmarkSourceFingerprint
    $runId = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $resultDirectory = Join-Path $repositoryRoot "base\build\benchmark\results"
    New-Item -ItemType Directory -Force -Path $resultDirectory | Out-Null
    $resultStem = "$runId-$commit-$($worktree.Name)-source-$sourceFingerprint-$selectedDevice"
    $resultFile = Join-Path $resultDirectory "$resultStem.csv"
    $logcatFile = Join-Path $resultDirectory "$resultStem-logcat.txt"
    $logcatErrorFile = Join-Path $resultDirectory "$resultStem-logcat.err.txt"

    $model = Get-DeviceProperty "ro.product.model"
    $android = Get-DeviceProperty "ro.build.version.release"
    $sdk = Get-DeviceProperty "ro.build.version.sdk"
    $abi = Get-DeviceProperty "ro.product.cpu.abi"
    if ($abi -ne "arm64-v8a") {
        throw "当前基准产物只支持 arm64-v8a，设备主 ABI 为: $abi"
    }
    $batteryStart = Get-BatterySnapshot

    @(
        "META,host_timestamp,$runId"
        "META,device_serial,$(Convert-ToCsvValue $selectedDevice)"
        "META,device_model,$(Convert-ToCsvValue $model)"
        "META,android_release,$(Convert-ToCsvValue $android)"
        "META,android_sdk,$(Convert-ToCsvValue $sdk)"
        "META,device_abi,$(Convert-ToCsvValue $abi)"
        "META,git_commit,$commit"
        "META,worktree_state,$($worktree.Name)"
        "META,worktree_fingerprint,$($worktree.Fingerprint)"
        "META,benchmark_source_fingerprint,$sourceFingerprint"
        "META,battery_level_start,$(Convert-ToCsvValue $batteryStart.Level)"
        "META,battery_temperature_tenths_c_start,$(Convert-ToCsvValue $batteryStart.Temperature)"
        "META,native_variant,RelWithDebInfo_-O2_-DNDEBUG"
    ) | Set-Content -Encoding utf8 $resultFile

    $remoteDirectory = "/data/local/tmp/ndk-log-benchmark"
    $remoteBinary = "$remoteDirectory/base_logging_benchmark"
    & $adb -s $selectedDevice shell mkdir -p $remoteDirectory
    if ($LASTEXITCODE -ne 0) { throw "创建设备端基准目录失败" }
    & $adb -s $selectedDevice push $artifact.FullName $remoteBinary
    if ($LASTEXITCODE -ne 0) { throw "推送设备端基准程序失败" }
    & $adb -s $selectedDevice shell chmod 755 $remoteBinary
    if ($LASTEXITCODE -ne 0) { throw "设置设备端基准程序权限失败" }

    $arguments = @(
        "--commit=$commit"
        "--run-id=$runId"
    )
    if ($Quick) {
        $arguments += "--quick"
    }

    $logcatProcess = $null
    try {
      if ($IncludeLogd) {
        $arguments += "--logd"
        $logcatProcess = Start-Process `
            -FilePath $adb `
            -ArgumentList @("-s", $selectedDevice, "logcat", "-v", "raw", "BASE-BENCH-LOGD:I", "*:S") `
            -RedirectStandardOutput $logcatFile `
            -RedirectStandardError $logcatErrorFile `
            -WindowStyle Hidden `
            -PassThru
        $logcatProcess.Refresh()
        if ($logcatProcess.HasExited) {
            $captureError = if (Test-Path $logcatErrorFile) { Get-Content -Raw $logcatErrorFile } else { "无错误输出" }
            throw "Logcat 采集器启动失败: $captureError"
        }

        # 固定等待不能证明 logcat 已经建立订阅。反复写入唯一就绪标记，
        # 直到采集文件真正观察到它，再开始正式突发测量。
        $readyMarker = "run=$runId capture_ready=1"
        $readyDeadline = (Get-Date).AddSeconds(5)
        $readyObserved = $false
        while ((Get-Date) -lt $readyDeadline) {
            $logcatProcess.Refresh()
            if ($logcatProcess.HasExited) {
                $captureError = if (Test-Path $logcatErrorFile) { Get-Content -Raw $logcatErrorFile } else { "无错误输出" }
                throw "Logcat 采集器提前退出: $captureError"
            }
            & $adb -s $selectedDevice shell $remoteBinary `
                "--run-id=$runId" "--capture-ready-marker-only" | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "写入 Logcat 采集就绪标记失败"
            }
            Start-Sleep -Milliseconds 100
            if ((Test-Path $logcatFile) -and
                (Select-String -Path $logcatFile -SimpleMatch $readyMarker -Quiet)) {
                $readyObserved = $true
                break
            }
        }
        if (-not $readyObserved) {
            throw "等待 Logcat 采集就绪标记超时，不能开始可靠的突发测量"
        }
      }

        & $adb -s $selectedDevice shell $remoteBinary $arguments |
            Tee-Object -FilePath $resultFile -Append
        if ($LASTEXITCODE -ne 0) {
            throw "设备端日志性能基准执行失败"
        }

        if ($IncludeLogd) {
            & $adb -s $selectedDevice shell $remoteBinary `
                "--run-id=$runId" "--capture-marker-only"
            if ($LASTEXITCODE -ne 0) {
                throw "写入 Logcat 采集完成标记失败"
            }

            $marker = "run=$runId capture_complete=1"
            $deadline = (Get-Date).AddSeconds(5)
            $markerObserved = $false
            while ((Get-Date) -lt $deadline) {
                $logcatProcess.Refresh()
                if ($logcatProcess.HasExited) {
                    $captureError = if (Test-Path $logcatErrorFile) { Get-Content -Raw $logcatErrorFile } else { "无错误输出" }
                    throw "Logcat 采集器提前退出: $captureError"
                }
                if ((Test-Path $logcatFile) -and
                    (Select-String -Path $logcatFile -SimpleMatch $marker -Quiet)) {
                    $markerObserved = $true
                    break
                }
                Start-Sleep -Milliseconds 100
            }
            if (-not $markerObserved) {
                throw "等待 Logcat 采集完成标记超时，不能计算可靠接收比例"
            }
        }
    } finally {
        if ($logcatProcess) {
            $logcatProcess.Refresh()
            if (-not $logcatProcess.HasExited) {
                Stop-Process -Id $logcatProcess.Id
            }
            Wait-Process -Id $logcatProcess.Id -ErrorAction SilentlyContinue
        }
    }

    if ($IncludeLogd) {
        $totalExpected = 0L
        $totalObservedFirstChunks = 0L
        Add-Content -Encoding utf8 -Path $resultFile `
            -Value "FIRST_CHUNK_OBSERVATION,name,expected_calls,observed_first_chunks,ratio"
        $logdResults = Select-String -Path $resultFile -Pattern '^RESULT,logd\.write_burst\.'
        foreach ($logdResult in $logdResults) {
            $fields = $logdResult.Line.Split(',')
            $name = $fields[1]
            $payloadBytes = [int64]$fields[2]
            $threads = [int64]$fields[3]
            $expected = [int64]$fields[4] * [int64]$fields[5]
            $pattern = "run=$runId bytes=$payloadBytes workers=$threads "
            $observed = (Select-String -Path $logcatFile -SimpleMatch $pattern).Count
            $ratio = [double]$observed / [double]$expected
            Add-Content -Encoding utf8 -Path $resultFile `
                -Value "FIRST_CHUNK_OBSERVATION,$name,$expected,$observed,$ratio"
            $totalExpected += $expected
            $totalObservedFirstChunks += $observed
        }
        $programExpectedLine = Select-String -Path $resultFile `
            -Pattern '^META,logd_expected_calls,' | Select-Object -Last 1
        $programExpected = [int64]$programExpectedLine.Line.Split(',')[2]
        if ($totalExpected -ne $programExpected) {
            throw "脚本计算的预期调用数与基准程序输出不一致"
        }
        Add-Content -Encoding utf8 -Path $resultFile `
            -Value "META,logd_observed_first_chunks,$totalObservedFirstChunks"
        Add-Content -Encoding utf8 -Path $resultFile `
            -Value "META,logd_first_chunk_observation_ratio,$([double]$totalObservedFirstChunks / [double]$totalExpected)"
    }

    $batteryEnd = Get-BatterySnapshot
    Add-Content -Encoding utf8 -Path $resultFile -Value "META,battery_level_end,$(Convert-ToCsvValue $batteryEnd.Level)"
    Add-Content -Encoding utf8 -Path $resultFile `
        -Value "META,battery_temperature_tenths_c_end,$(Convert-ToCsvValue $batteryEnd.Temperature)"

    Write-Host "基准结果: $resultFile"
    if ($IncludeLogd) {
        Write-Host "临时 Logcat 采集: $logcatFile"
    }
} finally {
    Pop-Location
}
