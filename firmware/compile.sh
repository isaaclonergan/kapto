#!/bin/bash

# Copy modifications into Marlin repository
cp platformio.ini Marlin/platformio.ini
cp Configuration.h Marlin/Marlin/Configuration.h
cp Configuration_adv.h Marlin/Marlin/Configuration_adv.h
cp MarlinCore.cpp Marlin/Marlin/src/MarlinCore.cpp
cp twibus.cpp Marlin/Marlin/src/feature/twibus.cpp
cp twibus.h Marlin/Marlin/src/feature/twibus.h
cp variant.h Marlin/buildroot/share/PlatformIO/variants/MARLIN_F103Rx/variant.h
cp stm32f1.ini Marlin/ini/stm32f1.ini

# Enter Marlin repository and compile
source venv/bin/activate
cd Marlin
platformio run --target clean
platformio run
