@echo off
setlocal EnableExtensions DisableDelayedExpansion
if /i not "%EnterpriseWDK%"=="True" goto environment_error
if /i not "%Platform%"=="x64" goto environment_error
set "CIDER_REFERENCE_MSBUILD=%VSINSTALLDIR%MSBuild\Current\Bin\amd64\MSBuild.exe"
if not exist "%CIDER_REFERENCE_MSBUILD%" set "CIDER_REFERENCE_MSBUILD=%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe"
if not exist "%CIDER_REFERENCE_MSBUILD%" goto environment_error
pushd "%~dp0" || exit /b 1
if not exist results mkdir results
if not exist out mkdir out
if exist out\client-build.ok del out\client-build.ok
if exist out\driver-build.ok del out\driver-build.ok
if exist results\user.jsonl del results\user.jsonl
if exist results\user.sha256.txt del results\user.sha256.txt
if not exist results goto failure
echo Building native x64 client...
"%CIDER_REFERENCE_MSBUILD%" client\reference.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /nologo /v:minimal "/flp:LogFile=results\client-build.log;Verbosity=normal"
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
if not exist out\client\cider-reference.exe goto failure
echo client-build-completed>out\client-build.ok
echo Building independent reference driver; signing is off...
"%CIDER_REFERENCE_MSBUILD%" driver\reference.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:SignMode=Off /m:1 /nologo /v:minimal "/flp:LogFile=results\driver-build.log;Verbosity=normal"
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
if not exist out\driver\CiderContractReference.sys goto failure
echo driver-build-completed>out\driver-build.ok
echo Collecting Win32 observations...
call run-user.cmd
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
echo Completed: results\user.jsonl and both build logs.
echo Driver execution and kernel contract results are a separate stage.
popd
exit /b 0
:failure
echo Build or collection failed. The files in results preserve the completed stages.
popd
exit /b 1
:environment_error
echo Open the EWDK amd64 environment, then run this build.cmd.
exit /b 1
