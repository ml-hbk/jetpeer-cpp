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

#include "jet/peer.hpp"

#include "notifier.h"

static void printSyntax()
{
	std::cout << "syntax: jetinfo <address of the jet daemon> <port of the jet daemon (port " << hbk::jet::JETD_TCP_PORT << ")" << std::endl;
	std::cout << "syntax: jetinfo <path to unix domain socket>" << std::endl;
}

/// @ingroup tools
/// Shows information about the jet daemon
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

		// default to unix domain sockets
		std::string address(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME);

		unsigned int port = 0;

		if(argc>1) {
			address = argv[1];
		}

		if(argc>2) {
			port = atoi(argv[2]);
		}

#ifndef _WIN32
		hbk::jet::Peer peer(address, port, basename(argv[0]));
#else
		hbk::jet::Peer peer(address, port, argv[0]);
#endif

		Json::Value info = peer.info();
		std::cout << "'" << info << "'" << std::endl;

	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
	}

	return EXIT_SUCCESS;
}
