# floor-heater
Esp8266 floor heater controller with web server(main controller, wifi credentials setup and pin code for authorization)

Controller handle 4 relay via ULN2303a. 
4th channel controll button (Bedroom) is hidden and can be activated by invisible "Easter Egg" button(click 8times in top left corner).
DS18B20 - which used in project has terrible accuracy (+5\*C),require calibrtion. 
All "set state" operations require key, which can be obtain with pin code. Login(key fetching) will be automatically perform during first set operation and will stored to browser cookies.  
![alt text](https://github.com/once2go/floor-heater/blob/master/screenshots/img1.png)
![alt text](https://github.com/once2go/floor-heater/blob/master/screenshots/img2.png)
