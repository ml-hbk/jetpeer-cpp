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

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "json/value.h"
#include "json/reader.h"
#include "json/writer.h"

#include "jet/peer.hpp"
#include "jet/defines.h"

static void printSyntax()
{
	std::cout << "syntax: jetexec <address of the jet daemon> <port of the jet daemon (default port " << hbk::jet::JETD_TCP_PORT << ")> <path> [<parameters as json>]" << std::endl;
}

/// @ingroup tools
/// Connects to a jet daemon, calls a jet method and waits for the response
int main(int argc, char* argv[])
{
	try {
		if(argc < 4) {
			printSyntax();
			return EXIT_SUCCESS;
		}

		std::string address = argv[1];
		unsigned int port = atoi(argv[2]);
		std::string path = argv[3];
		
		// we use a synchronous peer because we simply want to do one synchronous call
		hbk::jet::Peer peer(address, port);
		Json::Value params;

		if (argc >= 5) {
			Json::CharReaderBuilder builder;
			builder["collectComments"] = false;
			JSONCPP_STRING errs;
			size_t length = strlen(argv[4]);
			if (length) {
				std::unique_ptr<Json::CharReader> const reader(builder.newCharReader());
				if (!reader->parse(argv[4], argv[4] + length, &params, &errs)) {
					std::cerr << "Could not parse parameters for method: '" << errs << "'" << std::endl;
					return EXIT_FAILURE;
				}
			}
		}

		if (params == Json::Value()) {
			std::cout << "calling without any parameter..." << std::endl;
		}

		static const double timeout = 10;
		Json::Value result = peer.callMethod(path, params, timeout);
		std::cout << "Result: " << result << std::endl;
	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
