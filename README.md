# CSPModem

CSPModem aims to be an all inclusive SDR modem software for users of [libcsp](https://github.com/libcsp/libcsp) with GOMSpace's radios. The software implements so called "GOMspace's Mode-5" compatible physical layer protocol (with help of [suo modem library](https://github.com/petrinm/suo/)), CSP transport layer (SHA1/HMAC, Reed Solomon FEC, Randomizer, CRC32) and can be interfaced via ZMQ pub/sub from a CSP compatible software.



**The software is still under development and no guarantees can be given about its functionalities. If you are interested about the state of software or you're willing to contribute (preferred) please contact author by modern messaging services or via GitHub Issues.**

Features:
* Complete GomSpace Mode 5 modem. Can be configured for 
* Interfaces with the mission control software CSP's ZMQ Hub interface.
* Supports HMAC, CRC32, and XTEA
* Doppler tracking support (porthouse](https://github.com/aaltosatellite/porthouse) or hamlib's rigctl like interface.


## Design

Suo-modem relies

Preferably used with (SoapyShared)[https://github.com/petrinm/SoapyShared/]. Otherwise you cannot see what is happening on the spectrum.


## Installation

Install some binaries from you favorite package manager.
```
$ sudo apt install git gcc make automake
$ sudo apt install libsoapysdr-dev libzmq3-dev libliquid-dev
```

Pull the repository and its subrepositories
```
$ git clone https://github.com/petrinm/csp_modem.git
$ cd csp_modem
$ git submodule init
```

Compile libcsp
```
$ cd libcsp
$ ./waf configure --enable-hmac --enable-xtea --enable-if-zmqhub --prefix=install
$ ./waf build install --install-csp
$ cd ..
```

Compile Suo modem library
```
$ cd suo
$ mkdir build && cd build
$ cmake ..
$ make
$ cd ..
```

Compile and launch the CSPModem.
```
$ mkdir build && cd build
$ cmake ..
$ make
$ ./csp_modem
```
