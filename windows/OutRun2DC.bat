@echo off
rem OutRun2DC.bat: doble clic. Pide tu ISO de PS2 y una carpeta, y hace alli el GD-ROM de Dreamcast (disc.gdi).
rem Tambien: arrastra la ISO encima de este fichero, o OutRun2DC.bat tu.iso carpeta [--jobs N]
setlocal
title OutRun 2006 Coast 2 Coast para Dreamcast
"%~dp0programa\python\python.exe" "%~dp0programa\outrun2dc.py" %*
echo.
pause
