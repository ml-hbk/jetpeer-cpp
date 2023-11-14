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

#include <cstring>
#include <iostream>
#include <json/value.h>
#include <json/writer.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "hbk/sys/eventloop.h"

#include "jet/peerasync.hpp"

static void resultCb( const Json::Value& result)
{
	if(result.isMember("error")) {
		std::cerr << "adding method failed!" << std::endl;
	} else if (result.isMember("result")) {
		std::cout << "added method" << std::endl;
	} else {
		std::cerr << "unexpected response" << std::endl;
	}
}

static Json::Value methodCb(const Json::Value& value)
{
	std::cout << "method call with: " << value << std::endl;
	return value;
}

/// @ingroup tools
/// Connects to a jet daemon and creates a jet method that can be executed (see jetexec) by other jet peers
int main()
{
#ifdef _WIN32
	WSADATA data;
	WSAStartup(2, &data);
#endif
	try {
		hbk::sys::EventLoop eventloop;
		hbk::jet::PeerAsync peer(eventloop, "::1", hbk::jet::JETD_TCP_PORT); // connect to default port on localhost (ipv6)
		static const std::string methodName("theMethod");
		std::cout << "adding method '" << methodName << "'..." << std::endl;
		peer.addMethodAsync(methodName, 3.1415927, &resultCb, &methodCb);
		eventloop.execute();
		peer.removeMethodAsync(methodName);
		return EXIT_SUCCESS;
	} catch(std::runtime_error &e){
		std::cerr << e.what();
		return EXIT_FAILURE;
	}
}
