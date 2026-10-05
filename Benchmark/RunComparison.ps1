[CmdletBinding()]
param(
    [ValidateRange(1,50000)][int[]]$ObjectCounts = @(1000),
    [ValidateRange(0,256)][int[]]$LightCounts = @(0,8,32),
    [ValidateSet('Batched','Unbatched')][string[]]$Modes = @('Batched','Unbatched'),
    [ValidateSet('Dispersed','Overlap')][string]$Distribution = 'Dispersed',
    [ValidateRange(1,20)][int]$Repeats = 3,
    [ValidateRange(0,300)][double]$Warmup = 10,
    [ValidateRange(0.1,600)][double]$Seconds = 30,
    [ValidateRange(920,7680)][int]$Width = 1280,
    [ValidateRange(640,4320)][int]$Height = 720,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Smoke
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
if ($Smoke) {
    $ObjectCounts = @(4); $LightCounts = @(0,4); $Modes = @('Batched','Unbatched')
    $Repeats = 1; $Warmup = 0.2; $Seconds = 0.5
}
$executables = @{}
foreach ($renderer in @('Forward','Deffered')) {
    $executables[$renderer] = Join-Path $root "$renderer/bin/x64/$Configuration/$renderer.exe"
    if (!(Test-Path -LiteralPath $executables[$renderer])) { throw "Build $Configuration first: $renderer" }
}
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$output = Join-Path $PSScriptRoot "results/$id"
New-Item -ItemType Directory -Path $output | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Write-Json($path, $value) {
    [System.IO.File]::WriteAllText($path, ($value | ConvertTo-Json -Depth 20), $utf8)
}
$manifest = [ordered]@{
    configuration = $Configuration; smoke = [bool]$Smoke; started = [DateTime]::UtcNow.ToString('o')
    startup_window_style = 'Hidden'; frame_interval_scope = 'CPU loop interval, not displayed monitor FPS'
    git_commit = (& git -C $root rev-parse HEAD); git_status = @(& git -C $root status --short)
    hashes = @(); runs = @()
}
foreach ($renderer in @('Forward','Deffered')) {
    $files = @((Get-Item -LiteralPath $executables[$renderer]))
    $files += @(Get-ChildItem -LiteralPath (Join-Path (Split-Path $executables[$renderer]) 'Renderer/Shader') -Recurse -File)
    $files += @(Get-ChildItem -LiteralPath (Join-Path (Split-Path $executables[$renderer]) 'Asset/Skybox') -Recurse -File)
    foreach ($file in $files) {
        $manifest.hashes += [ordered]@{ path=$file.FullName; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
}
$model = Join-Path $root 'Asset/Mesh/DamagedHelmet/DamagedHelmet.gltf'
foreach ($file in Get-ChildItem -LiteralPath (Split-Path $model) -File) {
    $manifest.hashes += [ordered]@{ path=$file.FullName; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
}
$hasfailure = $false
foreach ($count in $ObjectCounts) {
    $columns = [int][Math]::Ceiling([Math]::Sqrt($count * $Width / $Height))
    $rows = [int][Math]::Ceiling($count / $columns)
    $halfwidth = ($columns - 1) * 0.6
    $halfheight = ($rows - 1) * 0.6
    $distance = [Math]::Max(5, [Math]::Max(($halfheight + 2), ($halfwidth + 2) * $Height / $Width) * 2.6)
    foreach ($lights in $LightCounts) { foreach ($mode in $Modes) { for ($repeat=1; $repeat -le $Repeats; ++$repeat) {
        $run = "n$count-l$lights-$mode-$Distribution-r$repeat"
        $scene = [ordered]@{
            profile='matched_quality'; width=$Width; height=$Height; vsync=0; instancing=($mode -eq 'Batched')
            run_id=$run; output_directory=$output; warmup_seconds=$Warmup; measurement_seconds=$Seconds
            camera_position=@(0,0,-$distance); camera_target=@(0,0,0)
            light_count=$lights; directional_light=$false; seed=12345
            light_center=@(0,0,-3); light_extent=@($halfwidth,$halfheight,1)
            light_radius=12; point_intensity=30
            grid=@{ model=$model; count=$count; columns=$columns; origin=@(-$halfwidth,-$halfheight,0)
                spacing=@(1.2,1.2,0); scale=@(0.4,0.4,0.4) }
        }
        if ($Distribution -eq 'Overlap') {
            $scene.light_extent = @(0.5,0.5,0.5)
            $scene.light_radius = [Math]::Max(12, [Math]::Sqrt($halfwidth*$halfwidth+$halfheight*$halfheight)+5)
        }
        $json = Join-Path $output "$run.scene.json"
        Write-Json $json $scene
        $order = if ($repeat % 2) { @('Forward','Deffered') } else { @('Deffered','Forward') }
        foreach ($renderer in $order) {
            Write-Host "$renderer $run"
            $entry = [ordered]@{ run_id=$run; renderer=$renderer; exit_code=$null; status='failed'; error='' }
            $process = $null
            try {
                $process = Start-Process -FilePath $executables[$renderer] -WorkingDirectory $root -WindowStyle Hidden -ArgumentList @('--benchmark',('"' + $json + '"')) -PassThru
                if (!$process.WaitForExit([int](($Warmup+$Seconds+120)*1000))) {
                    Stop-Process -Id $process.Id -Force
                    $process.WaitForExit()
                    throw 'Timed out during initialization or measurement.'
                }
                $process.Refresh()
                $entry.exit_code = $process.ExitCode
                if ($process.ExitCode -ne 0) { throw "Renderer exited with $($process.ExitCode)" }
                $result = Get-Content -LiteralPath (Join-Path $output "$run-$renderer.json") -Raw | ConvertFrom-Json
                if ($result.status -ne 'complete' -or $result.frames -lt 1) { throw 'Incomplete measurement.' }
                $entry.status = 'complete'
            } catch {
                $hasfailure = $true
                $entry.error = $_.Exception.Message
                Write-Warning $entry.error
            } finally {
                if ($process) { $process.Dispose() }
            }
            $manifest.runs += $entry
            Write-Json (Join-Path $output 'manifest.json') $manifest
        }
    } } }
}
& (Join-Path $PSScriptRoot 'SummarizeComparison.ps1') -Directory $output
Write-Host "Results: $output"
if ($hasfailure) { throw 'Some runs failed. See manifest.json; failed runs are excluded from the comparison.' }
