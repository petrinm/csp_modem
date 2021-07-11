# CSPModem

CSPModem



# Installation

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
```

Edit the configuration file `cfg.c` to fit your needs.

Compile and launch the CSPModem.
```
$ make
$ ./csp_modem
```
