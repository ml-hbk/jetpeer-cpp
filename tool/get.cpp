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

#include "boost/asio/io_context.hpp"

#include "json/value.h"
#include "json/writer.h"

#include "jet/peerasync.hpp"

#include "notifier.h"

static boost::asio::io_context eventloop;


static void responseCb(const Json::Value& value)
{
	// value contains the data as an array of objects
	const Json::Value& resultArrary = value[hbk::jsonrpc::RESULT];
	for (const Json::Value &item: resultArrary) {
		std::cout << "path " << item[hbk::jet::PATH] << std::endl;
		std::cout << "value " << item[hbk::jet::VALUE] << std::endl;
	}
	// stop whole program afterwards
	eventloop.run();
}

static void printSyntax()
{
		std::cout << "syntax: jetget <address of the jet daemon> <port of the jet daemon (port " << hbk::jet::JETD_TCP_PORT << ")> <path contains>" << std::endl;
}

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

		std::string address("127.0.0.1");
		unsigned int port = hbk::jet::JETD_TCP_PORT;

		if(argc>1) {
			address = argv[1];
		}

		if(argc>2) {
			port = atoi(argv[2]);
		}

		if(argc>3) {
			match.contains = argv[3];
		}

#ifndef _WIN32
		hbk::jet::PeerAsync peer(eventloop, address, port, basename(argv[0]));
#else
		hbk::jet::PeerAsync peer(eventloop, address, port, argv[0]);
#endif

		peer.getAsync(match, &responseCb);

		eventloop.run();
	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
	}


	return 0;
}
