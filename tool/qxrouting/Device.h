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
#ifndef __HBK__TEST__DEVICE_H
#define __HBK__TEST__DEVICE_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "json/value.h"
#include "jet/peerasync.hpp"

#include "hbk/sys/notifier.h"

#include "RoutedDevice.h"

namespace hbk {
	namespace jet {

		class Device
		{
		public:
			using restartCompleteCb_t = std::function < void(Device& device) >;
			
			struct appearanceInfo_t
			{
				appearanceInfo_t()
					: appearanceCount(0)
					, hbkProtocolAvailable(false)
				{
				}

				unsigned int appearanceCount;
				std::chrono::steady_clock::time_point appearanceTimePoint;
				std::chrono::steady_clock::time_point restartTimePoint;
				std::chrono::steady_clock::time_point disappearTimePoint;
				bool hbkProtocolAvailable;
			};

			using AppearanceInfos_t = std::unordered_map < std::string, appearanceInfo_t >;
			Device(hbk::sys::EventLoop& eventloop, const std::string &address, PeerAsync& peer, restartCompleteCb_t restartCompleteCb);

			int setSyslog(const std::string& destination);

			size_t getRoutedDeviceCount() const
			{
				return m_routedDevices.size();
			}

			AppearanceInfos_t getRoutedDevicesAppearance() const;

			void restartRoutedDevices();

		private:
			/// uuid of routed device is key
			using RoutedDevices = std::unordered_map < std::string, std::unique_ptr < RoutedDevice > >;

			void fetchRoutedCb(const Json::Value& notification, int status);
			void fetchResponseCb(const Json::Value& result);
			
			void serviceCb(const std::string& name, bool appears, const std::string &uuid);

			hbk::sys::EventLoop& m_eventloop;
			std::string m_address;
			PeerAsync& m_peer;
			
			RoutedDevices m_routedDevices;
			AppearanceInfos_t m_appearanceInfos;
			std::condition_variable m_fetchDoneCond;
			bool m_fetchDone;
			std::mutex m_fetchDoneCondMtx;
			
			restartCompleteCb_t m_restartCompleteCb;
		};
	}
}
#endif
