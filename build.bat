@echo off
echo Building ENS160 project...

set ARM_GCC=C:\ArmGCC\bin\arm-none-eabi-gcc
set MSDK_PATH=lib\msdk

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Compile main.c
%ARM_GCC% -c -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 ^
 -O0 -g3 -Wall -std=c11 ^
 -DMAX78000 -DBOARD_FTHR ^
 -I. ^
 -I%MSDK_PATH%\Libraries\Boards\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\CMSIS\Include ^
 -I%MSDK_PATH%\Device\Maxim\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\PeriphDrivers\Include ^
 main.c -o build\main.o

REM Compile ens160.c  
%ARM_GCC% -c -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 ^
 -O0 -g3 -Wall -std=c11 ^
 -DMAX78000 -DBOARD_FTHR ^
 -I. ^
 -I%MSDK_PATH%\Libraries\Boards\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\CMSIS\Include ^
 -I%MSDK_PATH%\Device\Maxim\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\MAX78000\Include ^
 -I%MSDK_PATH%\Libraries\PeriphDrivers\Include ^
 ens160.c -o build\ens160.o

REM Link everything
%ARM_GCC% -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 ^
 -T%MSDK_PATH%\Device\Maxim\MAX78000\Source\GCC\MAX78000.ld ^
 -specs=nosys.specs -Wl,--gc-sections ^
 build\main.o build\ens160.o -o build\ens160.elf

REM Create binary file
C:\ArmGCC\bin\arm-none-eabi-objcopy -O binary build\ens160.elf build\ens160.bin

echo Build complete!
echo Files in build\ folder