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

#include "json/value.h"

#include "hbk/sys/eventloop.h"
#include "hbk/string/split.h"

#include "jet/peerasync.hpp"

#include "RoutedDevice.h"


namespace hbk {
	namespace jet {
		RoutedDevice::RoutedDevice(sys::EventLoop &eventloop, const std::string& address, unsigned int jetPort)
			: m_peer(eventloop, address, jetPort)
			, m_serviceCb()
		{
		}
		
		RoutedDevice::~RoutedDevice()
		{
			m_serviceCb = ServiceCb();
		}
		
		void RoutedDevice::setServiceCb(ServiceCb serviceCb)
		{
			m_serviceCb = serviceCb;
			matcher_t matcher;
			matcher.startsWith = "net/services";
			m_peer.addFetchAsync(matcher,
													 std::bind(&RoutedDevice::fetchServicesCb, this, std::placeholders::_1, std::placeholders::_2),
													 responseCallback_t()
													 );
		}

		void RoutedDevice::restart()
		{
			m_peer.callMethodAsync("system/restart", Json::Value(), responseCallback_t());
		}
		
		void RoutedDevice::fetchServicesCb(const Json::Value& notification, int status)
		{
			if (status<0) {
				if (m_serviceCb) {
					m_serviceCb("", false);
				}
				return;
			}
			hbk::string::tokens pathTokens = hbk::string::split(notification[hbk::jet::PATH].asString(), '/');
			if (pathTokens.size()==3) {
				const std::string event(notification[hbk::jet::EVENT].asString());
				if (event==hbk::jet::ADD) {
					if (m_serviceCb) {
						m_serviceCb(pathTokens[2], true);
					}
				} else if (event==hbk::jet::REMOVE) {
					if (m_serviceCb) {
						m_serviceCb(pathTokens[2], false);
					}
				}
			}
		}
	}
}
