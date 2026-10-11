@echo off
setlocal EnableExtensions DisableDelayedExpansion
if /i not "%Platform%"=="x64" goto environment_error
if "%VSINSTALLDIR%"=="" goto environment_error
set "CIDER_TASK_MSBUILD=%VSINSTALLDIR%MSBuild\Current\Bin\amd64\MSBuild.exe"
if not exist "%CIDER_TASK_MSBUILD%" set "CIDER_TASK_MSBUILD=%VSINSTALLDIR%MSBuild\Current\Bin\MSBuild.exe"
if not exist "%CIDER_TASK_MSBUILD%" goto environment_error
pushd "%~dp0" || exit /b 1
"%CIDER_TASK_MSBUILD%" runner.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m:1 /nologo /v:minimal
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
"%CIDER_TASK_MSBUILD%" runner.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:Fixture=true /m:1 /nologo /v:minimal
if errorlevel 1 goto failure
if not errorlevel 0 goto failure
if not exist ..\..\out\windows-task-runner\cider-task-runner.exe goto failure
if not exist ..\..\out\windows-task-runner\cider-task-fixture.exe goto failure
echo Both ordinary CLI programs built; nothing was executed.
popd
exit /b 0
:failure
echo Task runner build failed; no Windows tasks were launched.
popd
exit /b 1
:environment_error
echo Open the existing x64 EWDK or VS2022 environment first.
exit /b 1
