@echo off
setlocal
pushd "%~dp0"

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vswhere.exe not found. Install Visual Studio 2022 with the C++ workload.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
  echo Visual Studio C++ tools not found.
  exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

if not exist build\plugin mkdir build\plugin
if not exist build\core mkdir build\core
if not exist build\host mkdir build\host
if not exist dist mkdir dist
if not exist package\Plugin\CzeAlign mkdir package\Plugin\CzeAlign

set COMMON=/nologo /MT /std:c++17 /EHsc /utf-8 /W4 /DUNICODE /D_UNICODE /DNOMINMAX /I sdk\aviutl2_sdk /I src

echo [1/5] Building CzeAlign.aux2 ...
cl %COMMON% /LD /Fo:build\plugin\ ^
  /Fe:build\CzeAlign.dll src\plugin.cpp src\ui.cpp src\importer.cpp src\rpp.cpp src\lang.cpp ^
  /link /DLL /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib
if errorlevel 1 exit /b 1
copy /y build\CzeAlign.dll build\CzeAlign.aux2 >nul

echo [2/5] Building core_test.exe ...
cl %COMMON% /Fo:build\core\ tests\core_test.cpp src\importer.cpp src\rpp.cpp ^
  /Fe:build\core_test.exe /link /SUBSYSTEM:CONSOLE user32.lib
if errorlevel 1 exit /b 1

echo [3/5] Building mock_host.exe ...
cl %COMMON% /Fo:build\host\ tests\mock_host.cpp ^
  /Fe:build\mock_host.exe /link /SUBSYSTEM:CONSOLE user32.lib
if errorlevel 1 exit /b 1

echo [4/5] Building ui_preview.exe ...
cl %COMMON% /Fo:build\host\ tests\ui_preview.cpp src\ui.cpp src\rpp.cpp src\importer.cpp src\lang.cpp ^
  /Fe:build\ui_preview.exe /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib
if errorlevel 1 exit /b 1

echo [5/5] Packaging ...
copy /y build\CzeAlign.aux2 package\Plugin\CzeAlign\CzeAlign.aux2 >nul
copy /y build\CzeAlign.aux2 dist\CzeAlign.aux2 >nul
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Compress-Archive -Path 'package\*' -DestinationPath 'dist\CzeAlign-v1.0.0.au2pkg.zip' -Force"
if errorlevel 1 exit /b 1

echo.
echo Build OK.
echo   build\CzeAlign.aux2
echo   build\core_test.exe
echo   build\mock_host.exe
echo   dist\CzeAlign-v1.0.0.au2pkg.zip
echo.
echo Tests:
echo   build\core_test.exe "G:\reaper\123123123.rpp"
echo   build\mock_host.exe build\CzeAlign.aux2
popd
endlocal






