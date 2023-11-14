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

#include <functional>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include <mutex>
#include <unordered_map>

#ifndef _WIN32
#include <libgen.h>
#endif



#include "json/value.h"
#include "json/writer.h"

#include "hbk/sys/eventloop.h"

#include "jet/peerasync.hpp"
#include "jet/defines.h"

#include "cache.h"

namespace hbk {
	namespace jet {
		cache::cache(hbk::jet::PeerAsync& peer, hbk::jet::matcher_t match)
			: m_peer(peer)
			, m_match(match)
			, m_cache()
			, m_cacheMtx()
		{
			m_fetchId = m_peer.addFetchAsync(match, std::bind(&cache::fetchCb, this, std::placeholders::_1, std::placeholders::_2));
		}

		cache::~cache()
		{
			m_peer.removeFetchAsync(m_fetchId);
		}

		void cache::setCbs(Cb addCb, Cb changeCb, Cb removeCb)
		{
			m_addCb = addCb;
			m_changeCb = changeCb;
			m_removeCb = removeCb;
		}

		Json::Value cache::getEntry(const std::string& path) const
		{
			std::lock_guard < std::mutex > lock(m_cacheMtx);
			const auto iter = m_cache.find(path);
			if (iter!=m_cache.end()) {
				return iter->second;
			}
			return Json::Value();
		}

		void cache::fetchCb( const Json::Value& params, int status)
		{
			if(status < 0) {
				std::cerr << "Lost connection to jet daemon!" << std::endl;
			} else {
				if(params.isObject()) {
					std::string event = params[hbk::jet::EVENT].asString();
					std::string path = params[hbk::jet::PATH].asString();
					const Json::Value value = params[hbk::jet::VALUE];

					if (event==hbk::jet::CHANGE) {
						std::lock_guard < std::mutex > lock(m_cacheMtx);
						m_cache[path] = value;
						if (m_changeCb) {
							m_changeCb( path, value);
						}
					}
					else if (event == hbk::jet::ADD) {
						std::lock_guard < std::mutex > lock(m_cacheMtx);
						m_cache[path] = value;
						if (m_addCb) {
							m_addCb( path, value);
						}
						std::cout << "cache does contain " << m_cache.size() << " element(s)" << std::endl;
					} else if (event==hbk::jet::REMOVE) {
						std::lock_guard < std::mutex > lock(m_cacheMtx);
						m_cache.erase(path);
						if (m_removeCb) {
							m_removeCb( path, value);
						}
						std::cout << "cache does contain " << m_cache.size() << " element(s)" << std::endl;
					}
				}
			}
		}
	}
}

static void print(const std::string& path, const Json::Value&, const::std::string& description)
{
	std::cout << "state '" << path << "' " << description << std::endl;
}

static void printSyntax()
{
	std::cout << "syntax: jetcache <address of the peer> <port of the peer (port " << hbk::jet::JETD_TCP_PORT << ")> <path contains>" << std::endl;
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

		std::string address(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME);
		unsigned int port = 0;

		if(argc>1) {
			address = argv[1];
		}

		if(argc>2) {
			port = atoi(argv[2]);
		}

		if(argc>3) {
			match.contains = argv[3];
		}

		hbk::sys::EventLoop eventloop;

#ifndef _WIN32
		hbk::jet::PeerAsync peer(eventloop, address, port, basename(argv[0]));
#else
		hbk::jet::PeerAsync peer(eventloop, address, port, argv[0]);
#endif

		// of course you may have several notifiers referencing to the same jet peer.
		hbk::jet::cache cache(peer, match);
		cache.setCbs(
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "added"),
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "changed"),
					std::bind(&print, std::placeholders::_1, std::placeholders::_2, "removed")
										);

		eventloop.execute();
	} catch(const std::runtime_error& exc) {
		std::cerr << exc.what() << std::endl;
	}

	return 0;
}
