@echo off
rem OutRunTaxi.bat: doble clic. Pide el disc.gdi de tu OutRun de Dreamcast con el taxi, (opcional) tu ISO de PS2 para el
rem agua de las ruedas y una carpeta, y hace alli el GD-ROM nuevo de OutRun Mod Rev 1.0 (disc.gdi).
rem Tambien: arrastra el disc.gdi encima de este fichero, o OutRunTaxi.bat disc.gdi carpeta [--iso tu.iso]
setlocal
title OutRun Mod Rev 1.0 (con el taxi) para Dreamcast
"%~dp0programa\python\python.exe" "%~dp0programa\outrun_taxi.py" %*
echo.
pause
