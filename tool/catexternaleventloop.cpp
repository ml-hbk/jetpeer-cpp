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

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>

#ifndef _WIN32
#include <libgen.h>
#endif



#include "json/value.h"
#include "json/writer.h"

#include "jet/defines.h"
#include "jet/peerasync.hpp"

#include "notifier.h"

static void print(const std::string& path, const Json::Value& value, const::std::string& description)
{
	if (value.isNull()) {
		std::cout << "method '" << path << "' " << description << std::endl;
	} else {
		std::cout << "state '" << path << "' " << description << std::endl;
		std::cout << value << std::endl;
	}
}

static void printSyntax()
{
	std::cout << "syntax: jetcat <address of the jet daemon> <port of the jet daemon (port " << hbk::jet::JETD_TCP_PORT << ")> <path contains>" << std::endl;
	std::cout << "syntax: jetcat <path to unix domain socket> <path contains>" << std::endl;
}

/// @ingroup tools
/// Connects to a jet daemon and fetches all states and methods
/// In this example, the provided eventloop is not used for receiving data. Instead an external eventloop is used for that purpose.
int main(int argc, char* argv[])
{
	try {
		if(argc==2) {
			if(strcmp(argv[1],"-h")==0) {
				printSyntax();
				return 0;
			}
		}
		hbk::jet::matcher_t match;

		// default to unix domain sockets (under windows, this falls back to tcp on localhost using the default port
		std::string address(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME);

		unsigned int port = 0;

		if(argc>1) {
			address = argv[1];
		}

		if(argc>2) {
			port = static_cast < unsigned int > (atoi(argv[2]));
		}

		if(argc>3) {
			match.contains = argv[3];
		}

		hbk::sys::EventLoop eventloop;
		hbk::sys::EventLoop dummyEventloop;

#ifndef _WIN32
		hbk::jet::PeerAsync peer(dummyEventloop, address, port, basename(argv[0]));
#else
		hbk::jet::PeerAsync peer(dummyEventloop, address, port, argv[0]);
#endif
		
		// by getting the event to get notfified for available data, any other eventloop mechanism can be used.
		hbk::sys::event event = peer.getReceiverEvent();
		eventloop.addEvent(event, std::bind(&hbk::jet::PeerAsync::receive, &peer));

		// of course you may have several notifiers referencing to the same jet peer.
		hbk::jet::tool::notifier notifier(peer);
		notifier.start(
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "added"),
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "changed"),
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "removed"),
					match
										);

		eventloop.execute();

		std::cout << "done!";

	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
	}



	return EXIT_SUCCESS;
}
