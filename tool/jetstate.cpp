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

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "json/value.h"
#include "json/writer.h"


#include "hbk/sys/eventloop.h"

#include "jet/peerasync.hpp"


static Json::Value value;

static void printSyntax()
{
	std::cout << "syntax: jetstateasync <jet path> <bool|int|double|string|json> <new value of the state>" << std::endl << std::endl;
	std::cout << "Creates a single jet state on the local machine" << std::endl;
}


/// @ingroup tools
/// a state is being registered on the jet daemon running on the local machine. Afterwards the process simply exists and serves...
int main(int argc, char* argv[])
{
#ifdef _WIN32
	WSADATA data;
	WSAStartup(2, &data);
#endif

	if (argc!=4) {
		printSyntax();
		return EXIT_SUCCESS;
	}
	const char* pPath = argv[1];
	const char* pType = argv[2];
	const char* pValue = argv[3];

	hbk::sys::EventLoop eventloop;
	// since the jet daemon runs on the local machine, we use Unix Domain Socket communication for better performance (TCP under Windows)
	hbk::jet::PeerAsync peer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0);

	if(strcmp(pType, "bool")==0) {
		if (strcmp(pValue, "false")==0) {
			value = false;
		} else if (strcmp(pValue, "true")==0) {
			value = true;
		} else {
			std::cerr << "invalid value for boolean expecting 'true'', or 'false'" << std::endl;
			return EXIT_FAILURE;
		}
	} else if(strcmp(pType, "int")==0) {
		value = atoi(pValue);
	} else if(strcmp(pType,  "double")==0) {
		value = strtod(pValue, nullptr);
	} else if(strcmp(pType, "string")==0) {
		value = pValue;
	} else if(strcmp(pType, "json")==0) {
		Json::CharReaderBuilder rBuilder;
		if(!rBuilder.newCharReader()->parse(pValue, pValue+strlen(pValue), &value, nullptr)) {
			std::cerr << "error while parsing json!" << std::endl;
			return EXIT_FAILURE;
		}
		std::cout << "adding state '" << pPath << "'..." << std::endl;

	} else {
		printSyntax();
		return EXIT_SUCCESS;
	}
	
	auto resultCb = [](const Json::Value& result)
	{
		if (result.isMember("error")) {
			std::cerr << "adding state failed!" << std::endl;
			std::cerr << result << std::endl;
		} else {
			std::cout << "added state" << std::endl;
		}
	};
	
	auto setCb = [](const Json::Value& requestedValue, const std::string&) -> hbk::jet::SetStateCbResult
	{
		hbk::jet::SetStateCbResult result;
		if (value == requestedValue) {
			std::cout << "state stays on previous value " << value << std::endl;
		} else {
			value = requestedValue;
			std::cout << "set state to " << value << std::endl;
			result.value = value;
		}
		return result;
	};

	peer.addStateAsync(pPath, value, 3.1415927, resultCb, setCb);
	eventloop.execute();
	peer.removeStateAsync(pPath);

	return EXIT_SUCCESS;
}
