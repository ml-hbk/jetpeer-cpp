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

#include <json/value.h>


#include "jet/peer.hpp"
#include "jet/peerasync.hpp"

namespace hbk
{
	namespace jet
	{
		static unsigned int m_fetchId = 0;

		hbk::jet::Peer& Peer::local()
		{
			static Peer _instance("127.0.0.1", JETD_TCP_PORT);
			return _instance;
		}

		Peer::Peer(const std::string &address, unsigned int port, const std::string& name, bool debug)
			: m_eventloop()
			, m_peerAsync(m_eventloop, address, port, name, debug)
		{
		}

		Peer::~Peer()
		{
		}

		Json::Value Peer::info()
		{
			Json::Value result;
			return result;
		}

		bool Peer::resume()
		{
			return true;
		}

		Json::Value Peer::config(const std::string&, bool)
		{
			Json::Value response;
			return response[jsonrpc::RESULT] = 0;
		}
		
		void Peer::configAsync(const std::string&, bool, responseCallback_t)
		{
		}

		Json::Value Peer::callMethod(const std::string&, const Json::Value&)
		{
			Json::Value response;
			return response[jsonrpc::RESULT] = 0;
		}

		Json::Value Peer::callMethod(const std::string&, const Json::Value&, double)
		{
			Json::Value response;
			return response[jsonrpc::RESULT] = 0;
		}

		void Peer::callMethodAsync(const std::string& path, const Json::Value& args, responseCallback_t responseCb)
		{
			m_peerAsync.callMethodAsync(path, args, responseCb);
		}

		void Peer::callMethodAsync(const std::string& path, const Json::Value& args, double , responseCallback_t responseCb)
		{
			m_peerAsync.callMethodAsync(path, args, responseCb);
		}

		void Peer::addMethod(const std::string&, methodCallback_t)
		{
		}

		void Peer::addState(const std::string&, const Json::Value&, stateCallback_t)
		{
		}
		
		/// @param resultCallback called on completion providing the result
		/// @param callback function to be called when state is set via jet. leave empty for read only states
		/// \throws hbk::exception::jsonrpcException on error
		void Peer::addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t responseCb, stateCallback_t stateCb)
		{
			m_peerAsync.addStateAsync(path, value, responseCb, stateCb);
		}

		void Peer::removeFetchAsync(fetchId_t id, responseCallback_t responseCb)
		{
			m_peerAsync.removeFetchAsync(id, responseCb);
		}

		/// the peer serves a new method on jet
		/// \throws hbk::exception::jsonrpcException on error
		void Peer::removeMethodAsync(const std::string& path, responseCallback_t responseCb)
		{
			m_peerAsync.removeMethodAsync(path, responseCb);
		}

		/// @param resultCb called when operation finished, NULL if we are not inerested in the response. This results in sending a notification instead of an request (no id).
		void Peer::removeStateAsync(const std::string& path, responseCallback_t responseCb)
		{
			m_peerAsync.removeStateAsync(path, responseCb);
		}

		fetchId_t Peer::addFetch(const matcher_t&, fetchCallback_t)
		{
			return ++m_fetchId;
		}

		fetchId_t Peer::addFetchAsync(const matcher_t& matcher, fetchCallback_t fetchCb, responseCallback_t responseCb)
		{
			m_peerAsync.addFetchAsync(matcher, fetchCb, responseCb);
			return ++m_fetchId;
		}

		void Peer::setStateValue(const std::string&, const Json::Value&)
		{
		}

		void Peer::setStateValue(const std::string&, const Json::Value&, double)
		{
		}

		/// set the value of the state/complex state
		/// \throws std::runtime_error
		void Peer::setStateValueAsync(const std::string& path, const Json::Value& value, responseCallback_t responseCb)
		{
			m_peerAsync.setStateValueAsync(path, value, responseCb);
		}
	}
}
