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

#include <stdexcept>
#include <cstring>
#include <iostream>
//#include <thread>
#include <chrono>

#ifndef _WIN32
#include <libgen.h>
#endif


#include "json/value.h"
#include "json/writer.h"

#include "jet/peer.hpp"
#include "jet/defines.h"

static Json::StyledWriter writer;

static void fetchCb( const Json::Value& params, int status)
{
	if(status < 0) {
		std::cerr << "Lost connection to jet daemon!" << std::endl;
	} else {
		if(params.isObject()) {
			std::string event = params[hbk::jet::EVENT].asString();
			std::string path = params[hbk::jet::PATH].asString();
			if((event.empty()==false) && params.isMember(hbk::jet::VALUE) && (path.empty()==false)) {
				std::cout << path << " " << event << ": " << std::endl << writer.write(params[hbk::jet::VALUE]) << std::endl;
			}
		}
	}
}


static void printSyntax()
{
		std::cout << "syntax: jetcat <address of the peer> <port of the peer (port " << hbk::jet::JETD_TCP_PORT << ")> <path contains>" << std::endl;
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
		hbk::jet::Peer peer(address, port, basename(argv[0]));
#else
		hbk::jet::Peer peer(address, port, argv[0]);
#endif


		hbk::jet::fetchId_t fetchId = peer.addFetch(match, &fetchCb);


		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		} while (true);


		peer.removeFetchAsync(fetchId);

	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
	}



	return 0;
}
