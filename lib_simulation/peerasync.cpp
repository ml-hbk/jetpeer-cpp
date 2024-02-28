/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#include <atomic>

#include "jet/peerasync.hpp"
#include "jet/defines.h"


namespace hbk
{
	namespace jet
	{
		static void sendResult(responseCallback_t resultCb)
		{
			if (resultCb != nullptr) {
				Json::Value result;
				result[hbk::jsonrpc::RESULT] = 1;
				resultCb(result);
			}
		}

		std::atomic <fetchId_t > PeerAsync::m_sfetchId(0);

		fetcher_t::fetcher_t()
			: callback()
			, matcher()
		{
		}

		fetcher_t::fetcher_t(fetchCallback_t cb, matcher_t m)
			: callback(cb)
			, matcher(m)
		{
		}


		PeerAsync::PeerAsync(sys::EventLoop& eventloop, const std::string &address, unsigned int port, const std::string& name, bool debug)
			: m_address(address)
			, m_port(port)
			, m_name(name)
			, m_debug(debug)
			, m_eventLoop(eventloop)
			, m_socket(eventloop)
			, m_stopped(false)
			, m_lengthBufferLevel(0)
			, m_dataBuffer()
			, m_dataBufferLevel(0)
		{
		}

		PeerAsync::~PeerAsync()
		{
		}

		void PeerAsync::start()
		{
		}

		void PeerAsync::stop()
		{
		}

		bool PeerAsync::resume()
		{
			return true;
		}

		int PeerAsync::receive()
		{
			return 0;
		}

		void PeerAsync::configAsync(const std::string&, bool, responseCallback_t resultCb)
		{
			sendResult(resultCb);
		}

		void PeerAsync::callMethodAsync(const std::string&, const Json::Value&, responseCallback_t resultCb)
		{
			sendResult(resultCb);
		}

		void PeerAsync::addMethodAsync(const std::string&, responseCallback_t resultCb, methodCallback_t)
		{
			sendResult(resultCb);
		}
		
		void PeerAsync::addMethodAsync(const std::string&, double, responseCallback_t resultCb, methodCallback_t)
		{
			sendResult(resultCb);
		}

		void PeerAsync::removeMethodAsync(const std::string&, responseCallback_t resultCb)
		{
			sendResult(resultCb);
		}

		void PeerAsync::addStateAsync(const std::string&, const Json::Value&, responseCallback_t resultCb, stateCallback_t)
		{
			sendResult(resultCb);
		}
		
		void PeerAsync::addStateAsync(const std::string&, const Json::Value&, double, responseCallback_t resultCb, stateCallback_t)
		{
			sendResult(resultCb);
		}

		void PeerAsync::removeStateAsync(const std::string&, responseCallback_t resultCb)
		{
			sendResult(resultCb);
		}

		void PeerAsync::addMethodResultCb(const Json::Value&, const std::string&)
		{
		}

		void PeerAsync::addStateResultCb(const Json::Value&, const std::string&)
		{
		}

		void PeerAsync::addFetchResultCb(const Json::Value&, fetchId_t)
		{
		}


		void PeerAsync::registerFetch(fetchId_t, const fetcher_t&)
		{
		}

		void PeerAsync::registerMethod(const std::string&, methodCallback_t)
		{
		}

		void PeerAsync::registerState(const std::string&, stateCallback_t)
		{
		}

		void PeerAsync::unregisterFetch(fetchId_t)
		{
		}

		void PeerAsync::unregisterMethod(const std::string&)
		{
		}

		void PeerAsync::unregisterState(const std::string&)
		{
		}

		void PeerAsync::setStateValueAsync(const std::string&, const Json::Value&, responseCallback_t responseCb)
		{
			sendResult(responseCb);
		}
		
		fetchId_t PeerAsync::createFetchId()
		{
			return ++m_sfetchId;
		}

		void PeerAsync::getAsync(const matcher_t&, responseCallback_t resultCb)
		{
			sendResult(resultCb);
		}

		fetchId_t PeerAsync::addFetchAsync(const matcher_t&, fetchCallback_t, responseCallback_t responseCb)
		{
			sendResult(responseCb);
			return createFetchId();
		}

		void PeerAsync::restoreFetch(const matcher_t&, fetchId_t)
		{
		}

//		void PeerAsync::addFetchAsyncResultCb(const Json::Value&, fetchId_t, responseCallback_t responseCb)
//		{
//			sendResult(responseCb);
//		}

//		void PeerAsync::addStateAsyncResultCb(const Json::Value&, const std::string&, responseCallback_t responseCb)
//		{
//			sendResult(responseCb);
//		}

//		void PeerAsync::addMethodAsyncResultCb(const Json::Value&, const std::string&, responseCallback_t responseCb)
//		{
//			sendResult(responseCb);
//		}

		void PeerAsync::removeFetchAsync(fetchId_t, responseCallback_t responseCb)
		{
			sendResult(responseCb);
		}

//		int PeerAsync::notifyState(const std::string& path, const std::string& value)
//		{
//			return notifyState(path, Json::Value(value));
//		}

//		int PeerAsync::notifyState(const std::string& path, const char* value)
//		{
//			return notifyState(path, Json::Value(value));
//		}

		template<>
		int PeerAsync::notifyState<uint64_t>(const std::string& path, uint64_t value)
		{
			return notifyState(path, Json::Value(static_cast < Json::UInt64 > (value)));
		}

		template<>
		int PeerAsync::notifyState<int64_t>(const std::string& path, int64_t value)
		{
			return notifyState(path, Json::Value(static_cast < Json::Int64 > (value)));
		}


//		int PeerAsync::notifyState(const std::string&, const Json::Value&)
//		{
//			return 0;
//		}

		void PeerAsync::sendMessage(const Json::Value&)
		{
		}


		void PeerAsync::receiveCallback(const Json::Value& data)
		{
			Json::ValueType type = data.type();
			switch(type) {
			case Json::arrayValue:
				// batch
				for (Json::ValueConstIterator iter = data.begin(); iter!= data.end(); ++iter) {
					const Json::Value& element = *iter;
					handleMessage(element);
				}
				break;
			case Json::objectValue:
				// a single object
				handleMessage(data);
				break;
			default:
				break;
			}
		}

		void PeerAsync::handleMessage(const Json::Value&)
		{
		}
	}
}
