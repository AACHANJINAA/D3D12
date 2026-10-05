[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath (Join-Path $Directory 'manifest.json') -Raw | ConvertFrom-Json
$rows = @()
$pairs = @()
foreach ($run in $manifest.runs) {
    if ($run.status -ne 'complete') { continue }
    $result = Get-Content -LiteralPath (Join-Path $Directory "$($run.run_id)-$($run.renderer).json") -Raw | ConvertFrom-Json
    if ($result.status -ne 'complete' -or $result.frames -lt 1 -or $result.profile -ne 'matched_quality') { continue }
    $summary = $result.summary
    $rows += [pscustomobject][ordered]@{
        run_id=$run.run_id; renderer=$run.renderer; objects=$result.scene.grid.count
        lights=$result.scene.light_count; instancing=$result.scene.instancing; frames=$result.frames
        gpu_median_ms=$summary.gpu_ms.median; gpu_p95_ms=$summary.gpu_ms.p95; gpu_p99_ms=$summary.gpu_ms.p99
        mesh_gpu_ms=$summary.mesh_gpu_ms.median; lighting_gpu_ms=$summary.lighting_gpu_ms.median
        prepare_cpu_ms=$summary.prepare_cpu_ms.median; record_cpu_ms=$summary.record_cpu_ms.median
        submit_cpu_ms=$summary.submit_cpu_ms.median; present_cpu_ms=$summary.present_cpu_ms.median
        sync_cpu_ms=$summary.sync_cpu_ms.median; frame_interval_ms=$summary.frame_interval_ms.median
        mesh_draw_calls=$summary.mesh_draw_calls.median; primitive_instances=$summary.primitive_instances.median
        gpu=$result.gpu; driver=$result.driver_version_raw; build=$result.build
    }
}
foreach ($group in ($rows | Group-Object run_id)) {
    $forward = @($group.Group | Where-Object renderer -eq 'Forward')
    $deferred = @($group.Group | Where-Object renderer -eq 'Deffered')
    if ($forward.Count -ne 1 -or $deferred.Count -ne 1) { continue }
    $f=$forward[0]; $d=$deferred[0]
    $fm = Get-Content -LiteralPath (Join-Path $Directory "$($group.Name)-Forward.json") -Raw | ConvertFrom-Json
    $dm = Get-Content -LiteralPath (Join-Path $Directory "$($group.Name)-Deffered.json") -Raw | ConvertFrom-Json
    if (($fm.scene | ConvertTo-Json -Depth 20 -Compress) -ne ($dm.scene | ConvertTo-Json -Depth 20 -Compress)) {
        Write-Warning "Scene mismatch excluded: $($group.Name)"
        continue
    }
    if ($f.gpu -ne $d.gpu -or $f.driver -ne $d.driver -or $f.build -ne $d.build -or
        $f.mesh_draw_calls -ne $d.mesh_draw_calls -or $f.primitive_instances -ne $d.primitive_instances) {
        Write-Warning "Mismatched pair excluded: $($group.Name)"
        continue
    }
    $pairs += [pscustomobject][ordered]@{
        run_id=$group.Name; objects=$f.objects; lights=$f.lights; instancing=$f.instancing
        forward_gpu_ms=$f.gpu_median_ms; deferred_gpu_ms=$d.gpu_median_ms
        gpu_saved_ms=($f.gpu_median_ms-$d.gpu_median_ms)
        forward_over_deferred= $(if ($d.gpu_median_ms -gt 0) { $f.gpu_median_ms/$d.gpu_median_ms } else { $null })
        forward_cpu_prepare_record_ms=($f.prepare_cpu_ms+$f.record_cpu_ms)
        deferred_cpu_prepare_record_ms=($d.prepare_cpu_ms+$d.record_cpu_ms)
        mesh_draw_calls=$f.mesh_draw_calls
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $Directory 'summary.csv') -NoTypeInformation -Encoding UTF8
$pairs | Export-Csv -LiteralPath (Join-Path $Directory 'comparison.csv') -NoTypeInformation -Encoding UTF8
$aggregate = @()
foreach ($group in ($rows | Group-Object renderer,objects,lights,instancing)) {
    $values = @($group.Group.gpu_median_ms | Sort-Object)
    $first = $group.Group[0]
    $aggregate += [pscustomobject][ordered]@{
        renderer=$first.renderer; objects=$first.objects; lights=$first.lights; instancing=$first.instancing
        runs=$values.Count; gpu_median_of_runs_ms=$values[[int][Math]::Floor(($values.Count-1)/2)]
        gpu_min_run_ms=$values[0]; gpu_max_run_ms=$values[-1]
    }
}
$aggregate | Export-Csv -LiteralPath (Join-Path $Directory 'aggregate.csv') -NoTypeInformation -Encoding UTF8
$heading = if ($manifest.smoke) { 'Smoke test only - not performance evidence' } else { 'Forward / Deffered comparison' }
$body = '<h1>'+$heading+'</h1><p>Times in milliseconds. Positive saved time means Deffered is faster. Forward mesh time includes lighting. Shared lighting equations; G-buffer quantization differs. No image-equivalence certification. UI GPU cost is included. Compare Release runs on an idle GPU.</p>'
$body += '<h2>GPU time by light count</h2>'
foreach ($group in ($aggregate | Group-Object objects,instancing)) {
    $maxlight = [Math]::Max(1, ($group.Group.lights | Measure-Object -Maximum).Maximum)
    $maxtime = [Math]::Max(0.001, ($group.Group.gpu_max_run_ms | Measure-Object -Maximum).Maximum)
    $body += '<h3>'+[System.Net.WebUtility]::HtmlEncode($group.Name)+'</h3><svg width="640" height="240" viewBox="0 0 640 240" role="img" aria-label="GPU milliseconds by point light count"><path d="M60 10V205H620" fill="none" stroke="#555"/><text x="5" y="20">ms</text><text x="480" y="235">Point lights</text>'
    foreach ($renderer in @('Forward','Deffered')) {
        $color = if ($renderer -eq 'Forward') { '#1679ae' } else { '#cf493e' }
        $points = @()
        foreach ($row in ($group.Group | Where-Object renderer -eq $renderer | Sort-Object lights)) {
            $x = (60+550*$row.lights/$maxlight).ToString('0.##',[Globalization.CultureInfo]::InvariantCulture)
            $y = (205-185*$row.gpu_median_of_runs_ms/$maxtime).ToString('0.##',[Globalization.CultureInfo]::InvariantCulture)
            $points += "$x,$y"
            $body += '<circle cx="'+$x+'" cy="'+$y+'" r="4" fill="'+$color+'"><title>'+$renderer+': '+$row.lights+' lights, '+$row.gpu_median_of_runs_ms+' ms</title></circle>'
        }
        $body += '<polyline points="'+($points -join ' ')+'" fill="none" stroke="'+$color+'" stroke-width="2"/>'
    }
    $body += '</svg><p>Blue: Forward. Red: Deffered. Repetition medians; ranges in the table below.</p>'
}
$body += '<h2>Repeat variability</h2>' + ($aggregate | ConvertTo-Html -Fragment | Out-String)
$body += '<h2>Paired runs</h2>' + ($pairs | ConvertTo-Html -Fragment | Out-String)
$body += '<h2>All successful runs</h2>' + ($rows | ConvertTo-Html -Fragment | Out-String)
$body += '<h2>Execution status</h2>' + ($manifest.runs | ConvertTo-Html -Fragment | Out-String)
$style = '<style>body{font:14px system-ui;margin:24px;color:#222;background:#fff}table{border-collapse:collapse;margin-bottom:24px}td,th{padding:6px 10px;border:1px solid #ddd;text-align:right}th{background:#eee}p{max-width:1000px}h1{font-size:24px}</style>'
ConvertTo-Html -Title 'Renderer comparison' -Head $style -Body $body |
    Set-Content -LiteralPath (Join-Path $Directory 'report.html') -Encoding UTF8
Write-Host "Paired runs: $($pairs.Count). Summary: $(Join-Path $Directory 'report.html')"
