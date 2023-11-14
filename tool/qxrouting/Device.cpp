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

#include <chrono>
#include <functional>
#include <iostream>

#include "json/value.h"
#include "json/writer.h"

#include "hbk/sys/eventloop.h"
#include "jet/peerasync.hpp"
#include "Device.h"

namespace hbk {
	namespace jet {
		Device::Device(hbk::sys::EventLoop& eventloop, const std::string& address, PeerAsync& peer, restartCompleteCb_t restartCompleteCb)
			: m_eventloop(eventloop)
			, m_address(address)
			, m_peer(peer)
			, m_fetchDone(false)
			, m_restartCompleteCb(restartCompleteCb)
		{
			matcher_t matcher;
			matcher.startsWith = "devices/routed";
			m_peer.addFetchAsync(matcher,
													 std::bind(&Device::fetchRoutedCb, this, std::placeholders::_1, std::placeholders::_2),
													 std::bind(&Device::fetchResponseCb, this, std::placeholders::_1)
													 );
			{
				std::unique_lock < std::mutex > lock(m_fetchDoneCondMtx);
				while(m_fetchDone==false) {
					m_fetchDoneCond.wait(lock);
				}
			}
		}

		Device::AppearanceInfos_t Device::getRoutedDevicesAppearance() const
		{
			return m_appearanceInfos;
		}


		void Device::restartRoutedDevices()
		{
			for (auto &iter: m_routedDevices) {
				m_appearanceInfos[iter.first].restartTimePoint = std::chrono::steady_clock::now();
				iter.second->restart();
			}
		}
		
		int Device::setSyslog(const std::string& destination)
		{
			Json::Value syslogConfig;
			
			syslogConfig[0]["level"] = 7;
			syslogConfig[0]["destination"] = destination;
			
			m_peer.setStateValueAsync("system/syslog", syslogConfig);
			return 0;
		}

		void Device::fetchRoutedCb(const Json::Value& notification, int status)
		{
			if (status==-1) {
				return;
			}
			const Json::Value announcement = notification[hbk::jet::VALUE];
			const std::string event(notification[hbk::jet::EVENT].asString());
			const std::string uuid = announcement["device"]["uuid"].asString();

			if( (event==hbk::jet::ADD) || (event==hbk::jet::CHANGE) ) {
				if (uuid.empty()) {
					return;
				} else if (m_routedDevices.find(uuid)!=m_routedDevices.end()) {
					// we know this already
					return;
				}
				
				const Json::Value services = announcement["services"];
				unsigned int port = 0;
				for (const Json::Value& entry: services) {
					if (entry["type"].asString()=="jetd") {
						port = entry["port"].asUInt();
						break;
					}
				}

				if (port==0) {
					// jet port not found in announcement"
					return;
				}

				m_routedDevices.emplace(uuid, std::make_unique < RoutedDevice> (m_eventloop, m_address, port));
				m_routedDevices[uuid]->setServiceCb(std::bind(&Device::serviceCb, this, std::placeholders::_1, std::placeholders::_2, uuid));
				{
					AppearanceInfos_t::iterator iter = m_appearanceInfos.find(uuid);
					
					if (iter==m_appearanceInfos.end()) {
						m_appearanceInfos[uuid].appearanceCount = 1;
					} else {
						++m_appearanceInfos[uuid].appearanceCount;
					}
				}
			} else if((event==hbk::jet::REMOVE)) {
				m_routedDevices.erase(uuid);
			}
		}
		void Device::fetchResponseCb(const Json::Value& result)
		{
			const Json::Value& errorObject = result[hbk::jsonrpc::ERR];
			if (errorObject.isObject()) {
				std::cerr << "error " << errorObject[hbk::jsonrpc::CODE].asInt() << " " << errorObject[hbk::jsonrpc::MESSAGE].asString() << std::endl;
			}
			
			{
				std::lock_guard < std::mutex > lock(m_fetchDoneCondMtx);
				m_fetchDone = true;
			}
			m_fetchDoneCond.notify_all();
		}
		
		void Device::serviceCb(const std::string& name, bool appears, const std::string& uuid)
		{
			if (name=="jetd") {
				if (appears) {
					m_appearanceInfos[uuid].appearanceTimePoint = std::chrono::steady_clock::now();
					if (m_appearanceInfos[uuid].disappearTimePoint!=std::chrono::steady_clock::time_point()) {
						unsigned int seconds = std::chrono::duration_cast< std::chrono::seconds > (std::chrono::steady_clock::now() - m_appearanceInfos[uuid].restartTimePoint).count();
						std::cout << uuid << ": service jetd appeared (took " << seconds << " seconds since reboot)"<< std::endl;
					}
				}
			} else if (name== "hbkProtocol") {
				m_appearanceInfos[uuid].hbkProtocolAvailable = appears;
				if (appears) {
					if (m_appearanceInfos[uuid].disappearTimePoint!=std::chrono::steady_clock::time_point()) {
						unsigned int secondsSinceRestart = std::chrono::duration_cast< std::chrono::seconds > (std::chrono::steady_clock::now() - m_appearanceInfos[uuid].restartTimePoint).count();
						unsigned int milliSecondsSinceAppearance = std::chrono::duration_cast< std::chrono::milliseconds > (std::chrono::steady_clock::now() - m_appearanceInfos[uuid].appearanceTimePoint).count();
						std::cout << uuid << ": service HBK Protocol appeared (took " << secondsSinceRestart << " seconds since reboot, " << milliSecondsSinceAppearance << " milliseconds since appearance on jet)" << std::endl;

						
						bool complete = true;
						for (const auto &iter: m_appearanceInfos) {
							if (iter.second.hbkProtocolAvailable==false) {
								complete = false;
								break;
							}
						}
						if (complete) {
							m_restartCompleteCb(*this);
						}
					}

				} else {
					unsigned int milliSeconds = std::chrono::duration_cast< std::chrono::milliseconds > (std::chrono::steady_clock::now() - m_appearanceInfos[uuid].restartTimePoint).count();
					std::cout << uuid << ": service HBK Protocol disappeared after " << milliSeconds << "ms" << std::endl;
				}
			} else if (name.empty()) {
				unsigned int seconds = std::chrono::duration_cast< std::chrono::seconds > (std::chrono::steady_clock::now() - m_appearanceInfos[uuid].restartTimePoint).count();
				std::cout << uuid << ": " << seconds << "s until shutdown of jetd "<< std::endl;
				m_appearanceInfos[uuid].disappearTimePoint = std::chrono::steady_clock::now();
			}
		}
	}
}
