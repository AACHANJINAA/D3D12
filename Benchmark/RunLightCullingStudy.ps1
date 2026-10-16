[CmdletBinding()]
param(
    [ValidateRange(0,1024)][int[]]$LightCounts = @(64,256,1024),
    [ValidateRange(0.1,100)][double[]]$Radii = @(2,6),
    [ValidateSet('Flat','Layers')][string[]]$Layouts = @('Flat','Layers'),
    [ValidateRange(1,50000)][int]$ObjectCount = 400,
    [ValidateRange(1,20)][int]$Repeats = 3,
    [ValidateRange(0,300)][double]$Warmup = 5,
    [ValidateRange(0.1,600)][double]$Seconds = 10,
    [ValidateRange(920,7680)][int]$Width = 1920,
    [ValidateRange(640,4320)][int]$Height = 1080,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [switch]$Smoke
)
# Forward vs Deferred full loop vs Deferred Tiled vs Deferred Clustered on identical scenes.
# Flat   : one grid plane, small depth range per tile (best case for tiled depth bounds).
# Layers : 20 layers spread in depth ("20 layers / far-first"), tiles see large depth gaps.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
if ($Smoke) {
    $LightCounts = @(32,256); $Radii = @(3); $Layouts = @('Flat','Layers')
    $Repeats = 1; $Warmup = 0.3; $Seconds = 1; $Width = 1280; $Height = 720
}
$executable = Join-Path $root "Deffered/bin/x64/$Configuration/Deffered.exe"
if (!(Test-Path -LiteralPath $executable)) { throw "Build $Configuration first: Deffered" }
$variants = [ordered]@{
    'Forward' = @('--forward'); 'Deffered' = @(); 'Deffered-Tiled' = @('--tiled'); 'Deffered-Clustered' = @('--clustered')
}
$id = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-culling-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$output = Join-Path $PSScriptRoot "results/$id"
New-Item -ItemType Directory -Path $output | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Write-Json($path, $value) { [System.IO.File]::WriteAllText($path, ($value | ConvertTo-Json -Depth 20), $utf8) }
$manifest = [ordered]@{
    study = 'light_culling'; configuration = $Configuration; smoke = [bool]$Smoke; started = [DateTime]::UtcNow.ToString('o')
    git_commit = (& git -C $root rev-parse HEAD); git_status = @(& git -C $root status --short); runs = @()
}
$model = Join-Path $root 'Asset/Mesh/DamagedHelmet/DamagedHelmet.gltf'
$columns = [int][Math]::Ceiling([Math]::Sqrt($ObjectCount * $Width / $Height))
$rows = [int][Math]::Ceiling($ObjectCount / $columns)
$halfwidth = ($columns - 1) * 0.6
$halfheight = ($rows - 1) * 0.6
$distance = [Math]::Max(5, [Math]::Max(($halfheight + 2), ($halfwidth + 2) * $Height / $Width) * 2.6)
$records = @()
$hasfailure = $false
foreach ($layout in $Layouts) { foreach ($radius in $Radii) { foreach ($lights in $LightCounts) {
    for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
        $run = "$layout-l$lights-r$radius-rep$repeat"
        $scene = [ordered]@{
            profile='matched_quality'; width=$Width; height=$Height; vsync=0; instancing=$true
            run_id=$run; output_directory=$output; warmup_seconds=$Warmup; measurement_seconds=$Seconds
            camera_position=@(0,0,-$distance); camera_target=@(0,0,0)
            light_count=$lights; directional_light=$false; seed=12345
            light_center=@(0,0,-1); light_extent=@($halfwidth,$halfheight,1.5)
            light_radius=$radius; point_intensity=30; study=3
            grid=@{ model=$model; count=$ObjectCount; columns=$columns; origin=@(-$halfwidth,-$halfheight,0)
                spacing=@(1.2,1.2,0); scale=@(0.4,0.4,0.4) }
        }
        if ($layout -eq 'Layers') {
            # Matches BENCHMARK_SCENE::apply_workload(1): 24 x 24 area, z from 8 down to ~0.4.
            $scene.workload = 1
            $scene.camera_position = @(0,0,-24)
            $scene.light_center = @(0,0,4); $scene.light_extent = @(12,12,5)
        }
        $json = Join-Path $output "$run.scene.json"
        Write-Json $json $scene
        $names = @($variants.Keys)
        # Rotate the order every repeat so thermal drift does not always favour one variant.
        $shift = ($repeat - 1) % $names.Count
        $order = @($names[$shift..($names.Count-1)]) + @(if ($shift) { $names[0..($shift-1)] })
        foreach ($renderer in $order) {
            Write-Host "$renderer $run"
            $entry = [ordered]@{ run_id=$run; renderer=$renderer; exit_code=$null; status='failed'; error='' }
            $process = $null
            try {
                $process = Start-Process -FilePath $executable -WorkingDirectory $root -WindowStyle Hidden -PassThru `
                    -ArgumentList (@('--benchmark',('"' + $json + '"')) + $variants[$renderer])
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
                $summary = $result.summary
                $records += [pscustomobject][ordered]@{
                    layout=$layout; lights=$lights; radius=$radius; repeat=$repeat; renderer=$renderer; frames=$result.frames
                    gpu_median_ms=$summary.gpu_ms.median; gpu_p95_ms=$summary.gpu_ms.p95
                    mesh_gpu_ms=$summary.mesh_gpu_ms.median
                    culling_gpu_ms=$(if ($summary.culling_gpu_ms) { $summary.culling_gpu_ms.median } else { $null })
                    lighting_gpu_ms=$(if ($summary.lighting_gpu_ms) { $summary.lighting_gpu_ms.median } else { $null })
                    gpu=$result.gpu; build=$result.build
                }
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
    }
} } }
$records | Export-Csv -LiteralPath (Join-Path $output 'runs.csv') -NoTypeInformation -Encoding UTF8

# Median over repeats per (layout, radius, lights, renderer).
function Median($values) { $sorted = @($values | Where-Object { $null -ne $_ } | Sort-Object); if (!$sorted.Count) { return $null }; $sorted[[int][Math]::Floor(($sorted.Count-1)/2)] }
$aggregate = @()
foreach ($group in ($records | Group-Object layout,radius,lights,renderer)) {
    $first = $group.Group[0]
    $aggregate += [pscustomobject][ordered]@{
        layout=$first.layout; radius=$first.radius; lights=$first.lights; renderer=$first.renderer; runs=$group.Count
        gpu_ms=(Median $group.Group.gpu_median_ms); gpu_min_ms=($group.Group.gpu_median_ms | Measure-Object -Minimum).Minimum
        gpu_max_ms=($group.Group.gpu_median_ms | Measure-Object -Maximum).Maximum
        mesh_ms=(Median $group.Group.mesh_gpu_ms); culling_ms=(Median $group.Group.culling_gpu_ms)
        lighting_ms=(Median $group.Group.lighting_gpu_ms)
    }
}
$aggregate = @($aggregate | Sort-Object layout,radius,lights,renderer)
$aggregate | Export-Csv -LiteralPath (Join-Path $output 'summary.csv') -NoTypeInformation -Encoding UTF8

$colors = @{ 'Forward'='#1679ae'; 'Deffered'='#cf493e'; 'Deffered-Tiled'='#2e9e5b'; 'Deffered-Clustered'='#8a5cc7' }
$heading = if ($Smoke) { 'Smoke test only - not performance evidence' } else { 'Light culling study' }
$body = '<h1>' + $heading + '</h1><p>' + "$Width x $Height, $ObjectCount objects, $Configuration, $Repeats repeat(s). " +
    'GPU medians in milliseconds (median of repeats). Same lights and shading equations in every variant; Tiled/Clustered skip only lights whose radius cannot reach the pixel, so the lit image is identical to the full loop. Forward mesh time includes its lighting.</p>'
foreach ($chart in ($aggregate | Group-Object layout,radius)) {
    $points = @($chart.Group)
    $maximum = [Math]::Max(0.001, ($points.gpu_ms | Measure-Object -Maximum).Maximum)
    $counts = @($points.lights | Sort-Object -Unique)
    $body += '<h2>' + $points[0].layout + ' / radius ' + $points[0].radius + '</h2><svg width="640" height="260" viewBox="0 0 640 260">'
    $body += '<line x1="50" y1="220" x2="620" y2="220" stroke="#999"/><line x1="50" y1="20" x2="50" y2="220" stroke="#999"/>'
    $body += '<text x="5" y="24" font-size="11">' + ('{0:N2}' -f $maximum) + ' ms</text>'
    for ($index = 0; $index -lt $counts.Count; ++$index) {
        $x = 50 + 570 * ($index + 0.5) / $counts.Count
        $body += '<text x="' + ($x - 12) + '" y="238" font-size="11">' + $counts[$index] + '</text>'
    }
    $body += '<text x="300" y="255" font-size="11">point lights</text>'
    foreach ($renderer in $variants.Keys) {
        $line = @($points | Where-Object renderer -eq $renderer | Sort-Object lights)
        $coordinates = @()
        foreach ($row in $line) {
            $x = 50 + 570 * ([Array]::IndexOf($counts, $row.lights) + 0.5) / $counts.Count
            $y = 220 - 200 * $row.gpu_ms / $maximum
            $coordinates += ('{0:N1},{1:N1}' -f $x, $y)
            $body += '<circle cx="' + ('{0:N1}' -f $x) + '" cy="' + ('{0:N1}' -f $y) + '" r="4" fill="' + $colors[$renderer] + '"><title>' + $renderer + ': ' + ('{0:N3}' -f $row.gpu_ms) + ' ms</title></circle>'
        }
        if ($coordinates.Count -gt 1) { $body += '<polyline fill="none" stroke-width="2" stroke="' + $colors[$renderer] + '" points="' + ($coordinates -join ' ') + '"/>' }
    }
    $body += '</svg>'
}
$body += '<p>' + (($variants.Keys | ForEach-Object { '<span style="color:' + $colors[$_] + '">&#9679; ' + $_ + '</span>' }) -join ' &nbsp; ') + '</p>'
$style = '<style>body{font-family:Segoe UI,sans-serif;margin:24px;color:#222}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:4px 8px;text-align:right}</style>'
$table = $aggregate | Select-Object layout,radius,lights,renderer,runs,
    @{n='gpu_ms';e={'{0:N3}' -f $_.gpu_ms}}, @{n='range';e={'{0:N3} - {1:N3}' -f $_.gpu_min_ms,$_.gpu_max_ms}},
    @{n='mesh_ms';e={'{0:N3}' -f $_.mesh_ms}}, @{n='culling_ms';e={if ($null -ne $_.culling_ms) {'{0:N3}' -f $_.culling_ms} else {'-'}}},
    @{n='lighting_ms';e={if ($null -ne $_.lighting_ms) {'{0:N3}' -f $_.lighting_ms} else {'-'}}} |
    ConvertTo-Html -Fragment
ConvertTo-Html -Title 'Light culling study' -Head $style -Body ($body + $table) |
    Out-File -LiteralPath (Join-Path $output 'report.html') -Encoding utf8
Write-Host "Results: $output"
if ($hasfailure) { throw 'Some runs failed. See manifest.json; failed runs are excluded from the summary.' }
