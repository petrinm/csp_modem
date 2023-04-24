# CSPModem

CSPModem aims to be an all inclusive SDR modem software for users of [libcsp](https://github.com/libcsp/libcsp) with GOMSpace's radios. The software implements so called "GOMspace's Mode-5" compatible physical layer protocol (with help of [suo modem library](https://github.com/petrinm/suo/)), CSP transport layer (SHA1/HMAC, Reed Solomon FEC, Randomizer, CRC32) and can be interfaced via ZMQ pub/sub from a CSP compatible software.



**The software is still under development and no guarantees can be given about its functionalities. If you are interested about the state of software or you're willing to contribute (preferred) please contact author by modern messaging services or via GitHub Issues.**

Features:
* Complete GomSpace Mode 5 modem. Can be modified easily for other modes such as AX.25.  
* Interfaces with the mission control software CSP's ZMQ Hub interface.
* Supports CSP's HMAC, CRC32 and XTEA encryption
* Doppler tracking support (porthouse](https://github.com/aaltosatellite/porthouse) or hamlib's rigctl like interface.


## Design

The CSP modem relies on (Suo modem libary)[] relies

Preferably used with [SoapyShared](https://github.com/petrinm/SoapyShared/). Otherwise you cannot see what is happening on the spectrum.


## Installation

The installation guide can be found from [INSTALL.md](INSTALL.md)


## Licence

The software is licenced under [MIT License](LICENSE.md) and heavily relies on suo modem.