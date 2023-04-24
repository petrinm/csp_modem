
# Install general dependencies

```
$ sudo apt install git gcc make automake
$ sudo apt install libsoapysdr-dev libzmq3-dev libliquid-dev
```


# Pull the repo and initialize submodules

```
$ git clone https://github.com/petrinm/csp_modem.git
$ cd csp_modem
$ git submodule init
```


# Compile and install CSP library

If you have already compiled and installed the libcsp to the system, these steps can be skipped.
```
$ git clone https://github.com/libcsp/libcsp.git
$ cd libcsp
$ ./waf configure --enable-hmac --enable-xtea --enable-if-zmqhub --prefix=install
$ ./waf build install --install-csp
$ cd ..
```
Note: If the libcsp is compiled inside the git submodule folder there's no need to install it system wide.
If you don't want to compile it here nor install it system wide, please hint the location libcsp using `-DLIBCSP=/home/location` when compiling CSPModem.


# Compile and install suo modem libary

If you have already compiled and installed the libsuo to the system, these steps can be skipped.

Compile Suo modem library
```
$ cd suo
$ mkdir build && cd build
$ cmake ..
$ make
$ cd ../..
```
Note: If the suo is compiled inside the git submodule folder there's no need to install it system wide.
If you don't want to compile it here nor install it system wide, please hint the location libcsp using `-DSUO_GIT=/home/location` when compiling CSPModem.


# Compile and launch the CSPModem.

```
$ mkdir build && cd build
$ cmake ..
$ make
$ ./csp_modemgit
$ cd libcsp
$ ./waf configure --enable-hmac --enable-xtea --enable-if-zmqhub --prefix=install
$ ./waf build install --install-csp
$ cd ..
```



After this CSPModem works as CSP ZMQHub and can be for example operated with csp-client. 
```
$ ./csp-client -z 127.0.0.1
csp-client # ping 5 1000 20
```


# Setting up the HMAC key

To enable use of "external secret" by 
Create `secret.hpp` file to the root of the repository. (This file has been configured to be ignored by git!)
Type your secret HMAC key in following format to file. 
```
uint8_t secret_key[16] = {0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x??, 0x?? };
```

After this (re)compile the project with `EXTERNAL_SECRET` flag.
```
$ cmake .. -DEXTERNAL_SECRET
```


# Compiling with AMQP/porthouse tracking support

To install CSPModem with AMQP and porthouse support, first install [AMQP-CPP](https://github.com/CopernicaMarketingSoftware/AMQP-CPP) with following commands.
```
$ sudo apt-get install libssl-dev
$ git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
$ cd AMQP-CPP
$ mkdir build && cd build
$ cmake .. -DAMQP-CPP_BUILD_SHARED=ON -DAMQP-CPP_LINUX_TCP=ON
$ cmake --build . --target install
$ sudo make install
$ sudo ldconfig
```


Configure the AMQP URL and target name in main() in csp_modem.c file.
```
PorthouseTracker::Config tracker_conf;
tracker_conf.amqp_url = "amqp://guest:guest@localhost/";
tracker_conf.target_name = "Suomi-100";
```

After this (re)compile the project with `USE_PORTHOUSE_TRACKER` flag.
```
$ cmake .. -DUSE_PORTHOUSE_TRACKER
```


# Compiling with fake rigctl interface

Compile the project with `RIGCTL_INTERFACE` flag.
```
$ cmake .. -DRIGCTL_INTERFACE
```
After this the CSPModem listens TCP port

