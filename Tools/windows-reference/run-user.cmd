@echo off
setlocal EnableExtensions DisableDelayedExpansion
pushd "%~dp0" || exit /b 1
if not exist out\client-build.ok goto failure
if not exist out\client\cider-reference.exe goto failure
if not exist results mkdir results
if exist results\user.sha256.txt del results\user.sha256.txt
"out\client\cider-reference.exe" --user >results\user.jsonl 2>results\user.stderr.txt
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
"%SystemRoot%\System32\certutil.exe" -hashfile out\client\cider-reference.exe SHA256 >results\client.sha256.txt
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
"%SystemRoot%\System32\certutil.exe" -hashfile results\user.jsonl SHA256 >results\user.sha256.txt
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
echo Win32 observations saved to results\user.jsonl.
popd
exit /b 0
:failure
echo Client collection failed; inspect results\user.jsonl and results\user.stderr.txt.
popd
exit /b 1
