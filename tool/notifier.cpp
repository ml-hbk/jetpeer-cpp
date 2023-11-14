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

#include "notifier.h"

namespace hbk {
namespace jet {
namespace tool {
notifier::notifier(hbk::jet::PeerAsync& peer)
	: m_peer(peer)
{
}

notifier::~notifier()
{
	m_peer.removeFetchAsync(m_fetchId);
}

void notifier::start(cb_t addCb, cb_t changeCb, cb_t removeCb, const hbk::jet::matcher_t& match)
{
	// check once and complain instead of checking every time without complain!
	if (!addCb || !changeCb || !removeCb) {
		std::cerr << "Invalid callback function for jet notifier!" << std::endl;
	}
	m_addCb = addCb;
	m_changeCb = changeCb;
	m_removeCb = removeCb;
	m_fetchId = m_peer.addFetchAsync(match, std::bind(&notifier::fetchCb, this, std::placeholders::_1, std::placeholders::_2));
}

void notifier::stop()
{
	m_peer.removeFetchAsync(m_fetchId);
}

void notifier::fetchCb(const Json::Value& params, int status)
{
	if (status < 0)
	{
		return;
	}

	if (!params.isObject())
	{
		return;
	}

	std::string event = params[hbk::jet::EVENT].asString();
	std::string path = params[hbk::jet::PATH].asString();
	const Json::Value value = params[hbk::jet::VALUE];

	if (event == hbk::jet::CHANGE)
	{
		m_changeCb(path, value);
	}
	else if (event == hbk::jet::ADD)
	{
		m_addCb(path, value);
	}
	else if (event == hbk::jet::REMOVE)
	{
		m_removeCb(path, value);
	}
}
}
}
}
