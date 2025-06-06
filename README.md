# Media Server

A multi-thread & high-performance media server written in C++, using Boost.Beast for asynchronous HTTP/WebSocket.

## Features
- Video streaming via multipart/x-mixed-replace http
- Boost.Asio & Boost.Beast async server
- Fast logging with spdlog
- MJPEG live streaming over HTTP
- Platform: Linux

## Dependencies

* OS: Linux
* Boost library (at least 1.74) for asynchronous HTTP/WebSocket
* Gstreamer library for screen capture
* Spdlog for feature-rich formatting

## Installing dependencies

* sudo add-apt-repository ppa:mhier/libboost-latest
* sudo apt update
* sudo apt install libboost1.74-dev
* cat /usr/include/boost/version.hpp | grep "BOOST_LIB_VERSION" //this should output: #define BOOST_LIB_VERSION "1_74"
* sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
* sudo apt install libspdlog-dev

## Instructions

* Clone the repo
```
git clone https://github.com/lbnguyen11/media-server.git
cd media-server
```

* Build from source
```
./01-compile.sh
```

* Run the server
```
// Usage: ./02-run.sh <doc_root> <threads>
./02-run.sh . 4
// then access http://localhost:8080/openning.mp4 for video streaming and http://localhost:8080/stream for MJPEG streaming
firefox http://localhost:8080/openning.mp4
firefox http://localhost:8080/stream
```

* Test performance via multiple curl's requests
```
03-spawn-curl.sh
```

* Test performance via multiple browser's requests
```
04-spawn-browser.sh
```

## Demo
[
media-server.webm](https://github.com/lbnguyen11/media-server/blob/main/media-server.webm)

https://lbnguyen11.github.io/webm-demo/

## Design overview

* TBU

