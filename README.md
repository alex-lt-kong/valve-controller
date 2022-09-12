# valve-controller

Program used to control the open/close of a solenoid valve.

## Dependencies
* [Onion HTTP library](https://github.com/davidmoreno/onion)
    * Onion will be `make install`ed to `/usr/local/lib/`, add the directory to `$LD_LIBRARY_PATH`:
 `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/`
* Library search support: `apt install pkg-config`.
* SSL/TLS support: `apt install gnutls-dev libgcrypt-dev`
* JSON support: `apt install libjson-c-dev`.
* Video device manipulation support: `apt-get install ffmpeg`
* Image manipulation: `apt-get install libopencv-dev`
* `Pigpio`: used to manipulate GPIO pins.
```
git clone https://github.com/joan2937/pigpio
cd ./pigpio
mkdir ./build
cd ./build
cmake ../
make
make install
```
