[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [ValidateSet('CPU','GBuffer','LowOverlap','HighOverlap')][string[]]$Cases = @('CPU','GBuffer','LowOverlap','HighOverlap'),
    [ValidateRange(1,10)][int]$Repeats = 3,
    [ValidateRange(0,60)][double]$Warmup = 2,
    [ValidateRange(0.2,120)][double]$Seconds = 5,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
if (!$OutputDirectory) { $OutputDirectory=Join-Path $root ('PortfolioStudy/' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output,(Join-Path $output 'Screenshots') | Out-Null
$utf8=New-Object Text.UTF8Encoding($false)
function Save-Json($name,$value) { [IO.File]::WriteAllText((Join-Path $output $name),($value|ConvertTo-Json -Depth 20),$utf8) }
$manifest=[ordered]@{configuration=$Configuration;warmup=$Warmup;seconds=$Seconds;repeats=$Repeats;commit=(& git -C $root rev-parse HEAD);runs=@()}
$definitions=@{
    CPU=@(
        @{renderer='Forward';label='Forward / unbatched / SRV rebuild';instancing=$false;cache=$false;compact=$false;study=0;workload=0;lights=4;count=4000},
        @{renderer='Forward';label='Forward / unbatched / SRV cache';instancing=$false;cache=$true;compact=$false;study=0;workload=0;lights=4;count=4000},
        @{renderer='Forward';label='Forward / instanced / SRV cache';instancing=$true;cache=$true;compact=$false;study=0;workload=0;lights=4;count=4000})
    GBuffer=@(
        @{renderer='Deffered';label='Deffered / full 40 B';instancing=$true;cache=$true;compact=$false;study=1;workload=0;lights=32;count=400},
        @{renderer='Deffered';label='Deffered / compact 20 B';instancing=$true;cache=$true;compact=$true;study=1;workload=0;lights=32;count=400})
    LowOverlap=@(
        @{renderer='Deffered';label='Deffered / low overlap';instancing=$true;cache=$true;compact=$true;study=2;workload=0;lights=4;count=400},
        @{renderer='Forward';label='Forward / low overlap';instancing=$true;cache=$true;compact=$true;study=2;workload=0;lights=4;count=400})
    HighOverlap=@(
        @{renderer='Forward';label='Forward / 20 layers';instancing=$true;cache=$true;compact=$true;study=2;workload=1;lights=256;count=400},
        @{renderer='Deffered';label='Deffered / 20 layers';instancing=$true;cache=$true;compact=$true;study=2;workload=1;lights=256;count=400})
}
$rows=@()
foreach ($case in $Cases) {
    $variants=$definitions[$case]
    for ($repeat=1; $repeat -le $Repeats; ++$repeat) {
        $order=@(0..($variants.Count-1))
        if ($repeat % 2 -eq 0) { [array]::Reverse($order) }
        foreach ($variant in $order) {
            $definition=$variants[$variant]
            $run="$case-v$variant-r$repeat"
            $columns=[int][Math]::Ceiling([Math]::Sqrt($definition.count*1.5))
            $rowscount=[int][Math]::Ceiling($definition.count/$columns)
            $spacing=24.0/$columns
            $scene=[ordered]@{
                profile='matched_quality';width=1280;height=720;vsync=0;run_id=$run
                output_directory=$output;warmup_seconds=$Warmup;measurement_seconds=$Seconds
                study=$definition.study;workload=$definition.workload;label=$definition.label
                instancing=$definition.instancing;descriptor_cache=$definition.cache;compact_gbuffer=$definition.compact
                camera_position=@(0,0,-30);camera_target=@(0,0,0)
                light_count=$definition.lights;directional_light=$false;seed=12345
                light_center=@(0,0,-3);light_extent=@(12,8,1);light_radius=25;point_intensity=8
                grid=@{model=(Join-Path $root 'Asset/Mesh/DamagedHelmet/DamagedHelmet.gltf')
                    count=$definition.count;columns=$columns
                    origin=@((-($columns-1)*$spacing/2),(-($rowscount-1)*$spacing/2),0)
                    spacing=@($spacing,$spacing,0);scale=@(($spacing*.4),($spacing*.4),($spacing*.4))}
            }
            Save-Json "$run.scene.json" $scene
            $exe=Join-Path $root "$($definition.renderer)/bin/x64/$Configuration/$($definition.renderer).exe"
            $entry=[ordered]@{run=$run;renderer=$definition.renderer;status='failed';error=''}
            $process=$null
            try {
                Write-Host "$run $($definition.renderer)"
                $process=Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Normal -ArgumentList @('--benchmark',('"'+(Join-Path $output "$run.scene.json")+'"')) -PassThru
                if (!$process.WaitForExit([int](($Warmup+$Seconds+60)*1000))) {
                    Stop-Process -Id $process.Id -Force
                    $process.WaitForExit()
                    throw 'Renderer timeout.'
                }
                if ($process.ExitCode -ne 0) { throw "Exit $($process.ExitCode)" }
                $result=Get-Content -LiteralPath (Join-Path $output "$run-$($definition.renderer).json") -Raw | ConvertFrom-Json
                if ($result.status -ne 'complete') { throw 'Incomplete measurement.' }
                $entry.status='complete'
                $rows += [pscustomobject]@{
                    case=$case;variant=$variant;repeat=$repeat;renderer=$definition.renderer;label=$definition.label
                    gpu_ms=$result.summary.gpu_ms.mean;gpu_p95_ms=$result.summary.gpu_ms.p95
                    prepare_ms=$result.summary.prepare_cpu_ms.mean;record_ms=$result.summary.record_cpu_ms.mean
                    mesh_ms=$result.summary.mesh_gpu_ms.mean;light_ms=$result.summary.lighting_gpu_ms.mean
                    draws=$result.summary.mesh_draw_calls.mean;srvs=$result.summary.srv_writes.mean
                    gbuffer_mib=$result.summary.gbuffer_payload_mib.mean;frames=$result.frames
                }
            } catch { $entry.error=$_.Exception.Message; throw }
            finally {
                if ($process) { $process.Dispose() }
                $manifest.runs += $entry
                Save-Json 'manifest.json' $manifest
            }
        }
    }
    # Presentation reruns always use repeat 1 as A, not a cherry-picked best run.
    for ($variant=1; $variant -lt $variants.Count; ++$variant) {
        $scene=Get-Content -LiteralPath (Join-Path $output "$case-v$variant-r1.scene.json") -Raw | ConvertFrom-Json
        $scene.run_id="$case-display-v$variant"
        $scene | Add-Member -NotePropertyName keep_open -NotePropertyValue $true
        $scene | Add-Member -NotePropertyName reference_result -NotePropertyValue (Join-Path $output "$case-v$($variant-1)-r1-$($variants[$variant-1].renderer).json")
        Save-Json "$case-display-v$variant.scene.json" $scene
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $output 'measurements.csv') -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Html -Title 'Renderer study measurements' -PreContent '<h1>Renderer study</h1><p>Repeated runs. GPU and CPU times are milliseconds. No guaranteed winner. G-buffer MiB is logical payload, not total VRAM.</p>' |
    Set-Content -LiteralPath (Join-Path $output 'measurements.html') -Encoding UTF8
Write-Host "Study saved: $output"
