# public-address-client

A RESTful-based public address client designed to run on embedded devices.

## Dependencies
* [Onion HTTP library](https://github.com/davidmoreno/onion)
    * Onion will be `make install`ed to `/usr/local/lib/`, add the directory to `$LD_LIBRARY_PATH`:
 `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/`
* Library search support: `apt install pkg-config`.
* SSL/TLS support: `apt install gnutls-dev libgcrypt-dev`
* JSON support: `apt install libjson-c-dev`.
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
* OpenCV: Install following [this link](https://github.com/alex-lt-kong/q-rtsp-viewer#opencv-installation-and-reference).

/usr/local/lib/arm-linux-gnueabihf/

install ffmpeg before compiling opencv to open rtsp sources.
apt install libopencv-dev# valve-controller
# valve-controller
