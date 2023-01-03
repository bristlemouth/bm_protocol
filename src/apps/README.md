To create a new application, create a new application directory with the following:
* **app_main.c** which includes main()
* **CMakeLists.txt** which should include all required files for compilation/include
* **FreeRTOSConfig.h** which will have all of the RTOS configuration

To build with the new app, just add `-DAPP=newappdirname` to your cmake command and you'll be ready to go!

For example:
```
cd src/apps/
cp -r bringup supercoolnewapp
cd ../../build
cmake .. -DCMAKE_TOOL CHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake -DBSP=xMoonJelly_v0 -DAPP=supercoolnewapp
```
