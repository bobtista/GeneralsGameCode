param([string]$Renderer = '')

$ErrorActionPreference = 'Continue'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$gameDir = Split-Path -Parent $here
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path ([Environment]::GetFolderPath('Desktop')) "GO-bgfx-diag-$stamp"
New-Item -ItemType Directory -Force $out | Out-Null

$sys = Join-Path $out 'system.txt'
"date: $(Get-Date -Format o)" | Out-File $sys
"renderer: $(if ($Renderer) { $Renderer } else { 'default (dx11)' })" | Out-File $sys -Append
Get-Content (Join-Path $here 'BUILD_INFO.txt') -ErrorAction SilentlyContinue | Out-File $sys -Append
(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed | Format-List | Out-String) | Out-File $sys -Append
"RAM GB: $([math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1))" | Out-File $sys -Append
(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate | Format-List | Out-String) | Out-File $sys -Append
(Get-CimInstance Win32_PowerPlan -Namespace root\cimv2\power -Filter 'IsActive=True' -ErrorAction SilentlyContinue | Select-Object ElementName | Out-String) | Out-File $sys -Append
if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) { nvidia-smi 2>&1 | Out-File (Join-Path $out 'nvidia-smi-before.txt') }

$env:GGC_BGFX_PERF_LOG = '1'
$env:GGC_BGFX_PERF_DIR = $out
if ($Renderer) { $env:GGC_BGFX_RENDERER = $Renderer }

Write-Host "Starting the game. Play a skirmish or online game for at least 3 minutes, then quit the game normally."
$p = Start-Process -FilePath (Join-Path $here 'GeneralsOnlineZH.exe') -WorkingDirectory $gameDir -PassThru

$gpuLog = Join-Path $out 'gpu-usage.csv'
'time,engine,utilization' | Out-File $gpuLog
$cpuLog = Join-Path $out 'cpu-usage.csv'
'time,process_cpu_percent_of_one_core,working_set_mb' | Out-File $cpuLog
$lastCpu = $null
while (-not $p.HasExited) {
	Start-Sleep -Seconds 10
	$t = Get-Date -Format 'HH:mm:ss'
	try {
		(Get-Counter '\GPU Engine(*engtype_3D)\Utilization Percentage' -ErrorAction Stop).CounterSamples |
			Where-Object { $_.InstanceName -match "pid_$($p.Id)_" -and $_.CookedValue -gt 0 } |
			ForEach-Object { "$t,$($_.InstanceName),$([math]::Round($_.CookedValue, 1))" | Out-File $gpuLog -Append }
	} catch {}
	try {
		$p.Refresh()
		$cpu = $p.TotalProcessorTime.TotalSeconds
		if ($lastCpu -ne $null) { "$t,$([math]::Round(($cpu - $lastCpu) * 10, 1)),$([math]::Round($p.WorkingSet64 / 1MB))" | Out-File $cpuLog -Append }
		$lastCpu = $cpu
	} catch {}
}

if (Get-Command nvidia-smi -ErrorAction SilentlyContinue) { nvidia-smi 2>&1 | Out-File (Join-Path $out 'nvidia-smi-after.txt') }
(Get-CimInstance Win32_VideoController | ForEach-Object { "$($_.Name): AdapterCompatibility=$($_.AdapterCompatibility) PNP=$($_.PNPDeviceID)" }) | Out-File $sys -Append

$data = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Command and Conquer Generals Zero Hour Data'
Copy-Item (Join-Path $data 'Options.ini') $out -ErrorAction SilentlyContinue
Copy-Item (Join-Path $data 'GeneralsOnlineData\*.log') $out -ErrorAction SilentlyContinue
Copy-Item (Join-Path $data 'GeneralsOnlineData\settings.json') $out -ErrorAction SilentlyContinue
Copy-Item (Join-Path $gameDir 'Data\INI\Bgfx.ini') $out -ErrorAction SilentlyContinue
Get-ChildItem (Join-Path $data 'CrashDumps') -Filter *.dmp -ErrorAction SilentlyContinue |
	Where-Object { $_.LastWriteTime -gt (Get-Date).AddHours(-1) -and $_.Length -lt 50MB } |
	Copy-Item -Destination $out -ErrorAction SilentlyContinue

$zip = "$out.zip"
Compress-Archive -Path "$out\*" -DestinationPath $zip -Force
Write-Host ""
Write-Host "Done. Send this file: $zip"
Read-Host "Press Enter to close"
