/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>

#include "json/value.h"

#include "jet/peer.hpp"

/// @ingroup tools
/// This program crates a jet states, notifies many times, sets many times, calculates and outputs the average time for notifying and setting
/// It can be run using tcp/ip or unix domain socket communication with the jet daemon


static const std::string STATE_PATH = "testSpeed/value";
static const unsigned int cycleCount = 1000;

// callback function does not do anything but acknowledging the value
static hbk::jet::SetStateCbResult stateCb(const Json::Value& value, const std::string&)
{
	hbk::jet::SetStateCbResult result;
	result.value = value;
	return result;
}

auto fetchCb = [](const Json::Value&, int)
{
	return;
};

static void measureSetNotify(const std::string& address, unsigned int port)
{
	std::cout << "*****" << std::endl;
	std::cout << "set/notify a single state." << std::endl;
	std::cout << "-Setting a state equals a request from one jet peer over the jet daemon to another jet peer and getting the response back..." << std::endl;
	std::cout << "-Notifying equals pushing a new value of an existing jet state from the jet peer to the jet daemon" << std::endl;
	// Instances of hbk::jet::Peer have their own receiver thread.
	hbk::jet::Peer jetPeer(address, port);

	try {
		jetPeer.addStateAsync(STATE_PATH, Json::Value(), hbk::jet::responseCallback_t(), &stateCb);
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}

	std::chrono::high_resolution_clock::time_point t1;
	std::chrono::high_resolution_clock::time_point t2;

	t1 = std::chrono::high_resolution_clock::now();
	try {
		for (unsigned int cycle = 0; cycle<cycleCount; ++cycle) {
			// setting the state instead of using notifying forces a request over the jet daemon
			// notifying just pushes the new value of the state to the daemon. Since we expect tcp/ip to be used, we do not expect a response.
			jetPeer.notifyState(STATE_PATH, cycle);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}

	t2 = std::chrono::high_resolution_clock::now();
	std::chrono::microseconds diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
	std::cout << "average time (" << cycleCount << " cycles) for notifying a state: " << diff.count()/cycleCount << "µs" << std::endl;

	try {
		t1 = std::chrono::high_resolution_clock::now();
		for (unsigned int cycle = 0; cycle<cycleCount; ++cycle) {
			// setting the state instead of using notifying forces a request over the jet daemon. Requests gets send to the daemon,
			// daemon routes request back to ourself, state callback is being called, response goes back to jet daemon and jet daemon routes the response finally back to us.
			jetPeer.setStateValue(STATE_PATH, cycle);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}

	t2 = std::chrono::high_resolution_clock::now();
	diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
	std::cout << "average time (" << cycleCount << " cycles) for setting a state: " << diff.count()/cycleCount << "µs" << std::endl;
}

static void measureFetch(const std::string& address, unsigned int port)
{
	static const unsigned int STATE_COUNT = 10000;
	static const unsigned int FETCH_COUNT = 100;
	std::cout << "*****" << std::endl;
	std::cout << "Create " << STATE_COUNT << " states and fetch them (This is done " << FETCH_COUNT << " times)..." << std::endl;
	// Instances of hbk::jet::Peer have their own receiver thread.
	hbk::jet::Peer jetPeer(address, port);
	std::vector < hbk::jet::fetchId_t > fetchIds(FETCH_COUNT);
	
	try {
		for (size_t stateIndex = 0; stateIndex<STATE_COUNT; ++stateIndex) {
			std::string statePath = STATE_PATH + std::to_string(stateIndex);
			jetPeer.addStateAsync(statePath, Json::Value(), hbk::jet::responseCallback_t(), &stateCb);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}

	std::chrono::high_resolution_clock::time_point t1;
	std::chrono::high_resolution_clock::time_point t2;
	std::chrono::microseconds diff;
	
	hbk::jet::matcher_t matcher;

	t1 = std::chrono::high_resolution_clock::now();
	for (unsigned int fetchIndex = 0; fetchIndex<FETCH_COUNT; ++fetchIndex) {
		fetchIds[fetchIndex] = jetPeer.addFetch(matcher, fetchCb);
	}
	t2 = std::chrono::high_resolution_clock::now();
	diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
	std::cout << "average time (" << FETCH_COUNT << " cycles) for fetching " << STATE_COUNT << " states: " << diff.count()/FETCH_COUNT << "µs (" << diff.count()/(FETCH_COUNT*STATE_COUNT) << "µs per state)" << std::endl;

	for (unsigned int fetchIndex = 0; fetchIndex<FETCH_COUNT; ++fetchIndex) {
		jetPeer.removeFetchAsync(fetchIds[fetchIndex]);
	}

	try {
		for (size_t stateIndex = 0; stateIndex<STATE_COUNT; ++stateIndex) {
			std::string statePath = STATE_PATH + std::to_string(stateIndex);
			jetPeer.removeStateAsync(statePath, hbk::jet::responseCallback_t());
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}

	return;
}

/// measures the time to create/destroy complex jet states synchronously
static void measureCreateStates(const std::string& address, unsigned int port)
{
	std::cout << "*****" << std::endl;
	static const unsigned int STATE_COUNT = 5000;
	hbk::jet::Peer jetPeer(address, port);

	std::cout << "Creating " << STATE_COUNT << " jet complex states synchronously" << std::endl;
	std::chrono::high_resolution_clock::time_point t1;
	std::chrono::high_resolution_clock::time_point t2;
	std::chrono::microseconds diff;

	t1 = std::chrono::high_resolution_clock::now();
	try {
		for (size_t stateIndex = 0; stateIndex<STATE_COUNT; ++stateIndex) {
			Json::Value value;
			value["asNumber"] = stateIndex;
			value["asString"] = std::to_string(stateIndex);
			std::string statePath = STATE_PATH + std::to_string(stateIndex);
			jetPeer.addState(statePath, Json::Value(), &stateCb);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}
	t2 = std::chrono::high_resolution_clock::now();
	diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
	std::cout << "This took " << diff.count()/STATE_COUNT << " µs per state" << std::endl;

	try {
		for (size_t stateIndex = 0; stateIndex<STATE_COUNT; ++stateIndex) {
			std::string statePath = STATE_PATH + std::to_string(stateIndex);
			jetPeer.removeStateAsync(statePath);
		}
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}
}

int main(int argc, char *argv[])
{
	std::string address;
	unsigned int port = 0;
	if (argc == 2) {
		std::cout << "using unix domain sockets" << std::endl;
		address = argv[1];
	} else if (argc == 3) {
		std::cout << "using tcp/ip" << std::endl;
		address = argv[1];
		port = strtoul(argv[2], nullptr, 10);
	} else {
		std::cout << "Syntax:" << std::endl;
		std::cout << "measurespeed <address> <port> for tcp/ip default port is " << hbk::jet::JETD_TCP_PORT << std::endl;
		std::cout << "measurespeed <name> for unix domain socket default port is " << hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME << std::endl;
		return EXIT_SUCCESS;
	}

	try {
		measureSetNotify(address, port);
		measureFetch(address, port);
		measureCreateStates(address, port);
	} catch (const std::runtime_error &e) {
		std::cerr << __FUNCTION__ << ": Caught exception: " << e.what() << "!" << std::endl;
	}
	return EXIT_SUCCESS;
}
