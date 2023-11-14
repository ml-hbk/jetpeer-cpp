# A C++ implementation of a jet peer

Every process that wants to interact using the jet protocol needs to use a jet peer.
Using a jet peer always requires a running jet daemon to connect to.
An implementation of a jet daemon can be found [here](https://github.com/gatzka/cjet).
For documentation about the jet protocol, please refer [here](http://hbm.github.io/jet/).
This C++ jet peer comes as a library (shared or static).

## Build

cmake is used as build system. To build under Linux, create build directory somwhere, change into this directory and call the following commands:

```
cmake < path to source root directory >
make -j8
sudo make install
```

Aterwards some shared libraries are installed on your system. Invoke `sudo ldconfig` to have the shared library cache updated to have them available.


To switch between building the static or shared version use:

```
cmake -DBUILD_SHARED_LIBS=<ON|OFF> < path to source root directory >
```


Under Windows, MSVC is able to build cmake projects.

### Prerequisits ##

cmake tries to find the required libraries on the system. If those are not installed, the desired sources are fetched.
The following libraries are required:

- jsoncpp: Can be found on GitHub [here](https://github.com/open-source-parsers/jsoncpp).
- HBK Library: This one can be found on GitHub [here](https://github.com/hbkworld/libhbk).

## Examples and Tools 

Those are to be found in the directory `tools`
If you want to build examples and tools, add the cmake option FEATURE_TOOLS 

```
cmake -DFEATURE_TOOLS=ON < path to source root directory >
```
Follow the build steps above to build and install the programs.


## Unit Tests

Those are to be found in the directory `test`. A jet daemon has to be running on the local machine in order to perform the tests.
If you want to build unit tests, add the cmake option FEATURE_POST_BUILD_UNITTEST

```
cmake -DFEATURE_POST_BUILD_UNITTEST=ON < path to source root directory >
```
After setting enabling the unit tests, use the following command sequence to build and execute the tests.

```
cmake < path to source root directory >
make -j4
ctest
```

## Testing your Own Code

When testing your own code that uses the c++ jet peer, the class PeerAsyncMock can be used.
It is usefull to test the behaviour of your state and method callback methods without communicating with a jet daemon.

This removes the requirement of having a jet daemon available on the machine running your tests.

Using this disables IPC-functionality also fetch-functionality is not available.

## UNIX Domain Sockets

When running on Linux, the jet daemon on the local might be reachable using UNIX domain sockets. `cjet` for example supports UNIX domain sockets.

Use the following command the get the address of the UNIX domain socket:

```
sudo netstat -a -p --unix | grep cjet
```

You will get something like:

```
unix  2      [ ACC ]     STREAM     HÖRT         32746    1089/cjet            @/var/run/jet.socket
```

The leading `@` on the UNIX domain socket name means that this is in abstract namespace.
As a result there are no access requirements in the filesystem. There is even no file visible.

