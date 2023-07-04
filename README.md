# Geolocation Firmware

![Иллюстрация к проекту](https://github.com/yuryf1/GeoFirmware/blob/master/My%20Helper%20LLC%20PCB.png)

The most accurate determination of location coordinates based on cellular base stations for GSM / NB-IoT Quectel's modules (OpenCPU Framework).
1. Scanning for base stations
2. JSON request to the Yandex maps. (Comparison of data with users who had GPS enabled in this area. Calculating the coordinates.)
3. Sending to the GeoWebApp.

Main code: custom/main.c
