// This code is licenced under the MIT license:
//
// Copyright (c) 2024 Hottinger Brüel & Kjær
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

#ifdef _WIN32
#define basename(x) x
#else
#include <libgen.h>
#endif

#include "hbk/sys/eventloop.h"
#include "hbk/sys/timer.h"

#include "jet/peerasync.hpp"

#include "Device.h"


/// modules with isochronous signals need much longer
static const unsigned int rebootTimeInSeconds = 120;

static hbk::sys::EventLoop s_eventloop;
static hbk::sys::Timer s_timer(s_eventloop);

static void checkAndReboot(hbk::sys::EventLoop& eventloop, hbk::jet::Device& device);


/// connects with a device and executes the following sequence for restartCycles times:
/// Connect with jetd of all routed devices, restart all routed devices wait some time.
static void restartTimerCb(bool fired, hbk::sys::EventLoop& eventloop, hbk::jet::Device& device)
{
	if (fired) {
		std::cerr << "not all devices came up in time!" << std::endl;
		checkAndReboot(eventloop, device);
	}
}


static void checkAndReboot(hbk::sys::EventLoop& eventloop, hbk::jet::Device& device)
{
	static unsigned int restartCycleCount = 0;

	hbk::jet::Device::AppearanceInfos_t appearanceCounts = device.getRoutedDevicesAppearance();

	for (const auto &iterNew: appearanceCounts) {
		std::string uuid = iterNew.first;
		unsigned int appearanceCount = iterNew.second.appearanceCount;
		if (appearanceCount!=restartCycleCount+1) {
			std::cerr << "routed device " << uuid << " was not restarted as expected" << std::endl;
		}
	}

	std::cout << "performing restart of all routed devices" << std::endl;
	++restartCycleCount;
	s_timer.set(std::chrono::milliseconds(1000*rebootTimeInSeconds), true, std::bind(&restartTimerCb, std::placeholders::_1, std::ref(eventloop), std::ref(device)));

	device.restartRoutedDevices();
}


static void restartCompleteCb(hbk::jet::Device& device)
{
	std::cout << "all routed devices have completed restart" << std::endl;
	checkAndReboot(s_eventloop, device);
}



int main(int argc, char* argv[])
{
	std::string address("127.0.0.1");
	std::string syslogDestination;
	unsigned int port = hbk::jet::JETD_TCP_PORT;
	
	if (argc==1) {
		std::cout << "syntax: " << basename(argv[0]) << " < device address > [ < syslog destination address > ]" << std::endl;
		return 0;
	}

	if (argc>1) {
		address = argv[1];
	}
	
	if (argc>2) {
		syslogDestination = argv[2];
	}


	std::thread eventloopWorker(std::bind(&hbk::sys::EventLoop::execute, std::ref(s_eventloop)));

	hbk::jet::PeerAsync peer(s_eventloop, address, port, basename(argv[0]));

	hbk::jet::Device device(s_eventloop, address, peer, &restartCompleteCb);

	size_t routedDeviceCount = device.getRoutedDeviceCount();
	if (routedDeviceCount==0) {
		std::cout << "no routed devices. Ending test" << std::endl;
		s_eventloop.stop();
	} else {
		device.setSyslog(syslogDestination);

		std::cout << routedDeviceCount << " routed devices" << std::endl;
		checkAndReboot(s_eventloop, device);
	}

	eventloopWorker.join();
	return 0;
}
