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
#ifndef __HBK__TEST__ROUTEDDEVICE_H
#define __HBK__TEST__ROUTEDDEVICE_H

#include <condition_variable>
#include <mutex>

#include "json/value.h"

#include "hbk/sys/eventloop.h"
#include "jet/peerasync.hpp"

namespace hbk {
	namespace jet {
		class RoutedDevice
		{
		public:
			using ServiceCb = std::function < void(const std::string& name, bool appears) >;
			
			/// \warning boot count is specific for QuantumX
			RoutedDevice(hbk::sys::EventLoop& eventloop, const std::string& address, unsigned int jetPort);
			
			virtual ~RoutedDevice();
			
			void setServiceCb(ServiceCb serviceCb);

			void restart();

		private:
			void fetchServicesCb(const Json::Value& notification, int status);

			PeerAsync m_peer;
			ServiceCb m_serviceCb;
		};
	}
}
#endif
