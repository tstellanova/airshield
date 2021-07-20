# airshield

This application for the Particle Argon 
reads an attached gas sensor and broadcasts its measurments periodically via 
Bluetooth Low Energy (BLE) advertising.
This example uses the least power possible by sleeping in between sensor measurements,
while continuing to advertise via BLE. 
It is meant to be used with the 
[airgrouper](https://github.com/tstellanova/airgrouper.git)
data collection hub that collects readings from multiple intermittently nearby
airshields and forwards to the Particle Cloud. 

This application uses a 
[compact air quality sensor](https://www.sparkfun.com/products/14348)
to detect volatile organic compounds (VOCs) as well as carbon dioxide.
This application uses the onboard temperature and humidity sensor (BME280)
to dynamically recalibrate the gas sensor. 


### To Build & Flash with Particle Workbench (vscode)

This application may be built with Device OS version 2.1.0 (LTS) and above.

1. Clone this repository 
2. Init & Update Submodules `git submodule update --init --recursive`
3. Open Particle Workbench
4. Run the `Particle: Import Project` command, follow the prompts, to select this project's `project.properties` file and wait for the project to load
5. Run the `Particle: Configure Workspace for Device` command and select a compatible Device OS version and the `argon` platform when prompted ([docs](https://docs.particle.io/tutorials/developer-tools/workbench/#cloud-build-and-flash))
6. Connect your Argon to your computer with a usb cable
7. Compile & Flash using Workbench


### To Build & Flash with Particle CLI

This application may be built with Device OS version 2.1.0 (LTS) and above.

1. Clone this repository 
2. Init & Update Submodules `git submodule update --init --recursive`
3. Cloud build (for Argon board) with CLI :
`particle compile --target 2.1.0 argon --saveTo airshield_argon.bin`

4. Connect your Argon to your computer with a usb cable
5. Use the CLI to flash the device using dfu:
`particle usb dfu && particle flash --usb airshield_argon.bin`

