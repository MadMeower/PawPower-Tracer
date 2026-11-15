@echo off
setlocal
powershell.exe -NoLogo -NoProfile -Command "try { $bytes = New-Object byte[] 32; $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create(); try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }; Write-Host ([Convert]::ToBase64String($bytes)); Write-Host ''; Write-Host 'Put this key in your secrets.yaml file.' } catch { Write-Error $_; exit 1 }"
if errorlevel 1 echo Key generation failed.
echo.
pause
endlocal
