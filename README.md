This project "Cabezal de Riego" (Smart Irrigation Head) is capable to water using the next devices and programs

Hardware:
ESP-32
Solenoid Valve : TORO EZ-FLO-EZP-02-54
Pressure sensor: DC 5V G1
Digital Flowmeter: YF-S201
Oled screen : SSH1106 

Software:
Thingsboard : IoT
Espressif SDK (ESP-IDF)

You have to change MQTT params and Wifi params in main.

Components folder has all the libraries used for the screen OLED, so if you don´t use this screen, this folder is not necessary for you. 

In this project the screen is configured with I2C but you can use SPI.


