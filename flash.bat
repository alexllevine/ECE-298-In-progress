@echo off
echo Flashing to MAX78000...

set OPENOCD=C:\OpenOCD\bin\openocd.exe

%OPENOCD% -f interface/cmsis-dap.cfg -f target/max78000.cfg ^
 -c "program build\ens160.bin verify reset exit"

echo Flashing complete!