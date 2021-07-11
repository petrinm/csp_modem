# CSPModem

CSPModem aims to be an all inclusive SDR modem software for users of [libcsp](https://github.com/libcsp/libcsp) with GOMSpace's radios. The software implements so called "GOMspace's Mode-5" compatible physical layer protocol (with help of [suo modem library](https://github.com/tejeez/suo/)), CSP transport layer (SHA1/HMAC, Reed Solomon FEC, Randomizer, CRC32) and can be interfaced via ZMQ pub/sub from a CSP compatible software.

**The software is still under development and no guarantees can be given about its functionalities. If you are interested about the state of software or you're willing to contribute (preferred) please contact author by modern messaging services or via GitHub Issues.**

## Installation

```
$ sudo apt install git gcc make automake
$ sudo apt install libsoapysdr-dev libzmq3-dev libliquid-dev

$ git clone https://github.com/petrinm/csp_modem.git
$ cd csp_modem
$ git submodule init
```

Compile libcsp
```
$ cd libcsp
$ ./waf configure --prefix=install
$ ./waf build install
$ cd ..
```

Compile suo-modem
```
$ cd suo/libsuo
$ make
$ cd ../..
```

Edit the configuration file `cfg.c` to fit your needs.

Compile and launch the CSPModem.
```
$ make
$ ./csp_modem
```
