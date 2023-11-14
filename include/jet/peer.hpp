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


#pragma once

#include <string>
#include <thread>

#include <json/value.h>

#include "hbk/sys/eventloop.h"

#include "jet/peerasync.hpp"
#include "jet/defines.h"

namespace hbk
{
	namespace jet
	{
		/// C++ jet peer for synchronuous and asynchronuous calls. It has its own thread that does run its own eventloop.
		/// \warning Keep in mind that your code needs to be thread-safe!
		/// \note All methods that do not provide a timeout, have the default timeout of the jet daemon.
		class Peer
		{
		public:

			/// @ingroup anyPeer
			/// @throws std::runtime_error
			/// @param address the ip address or unix domain socket name of the remote jetd depending on port
			/// @param port tcp port of jetd 0 if unix domain socket is to be used
			/// @param name Optional name of the peer
			/// @param debug Optional debug switch. false is default
			Peer(const std::string& address, unsigned int port, const std::string& name="", bool debug=false);
			Peer(const Peer&) = delete;
			Peer& operator=(const Peer&) = delete;

			/// @ingroup anyPeer
			/// on destruction the socket is being closed. The jetd we were connected to will automatically remove all fetches, states, and methods belonging to the disconnected peer.
			~Peer();

			/// @ingroup anyPeer
			/// @return Reference to the asynchronous peer
			PeerAsync& getAsyncPeer()
			{
				return m_peerAsync;
			}

			/// @ingroup anyPeer
			/// try to reconnect to the jet daemon and resume operation
			bool resume();

			/// @ingroup anyPeer
			/// The peer authenticates itself against the daemon
			Json::Value authenticate(const std::string& user, const std::string& password);

			/// @ingroup anyPeer
			/// Retrieve information about the jet daemon
			/// \return a json rpc response object with the result or an error object
			JsonRpcResponseObject info();

			/// @ingroup anyPeer
			/// @param name name of the peer
			/// @param debug enable debug logging
			/// \return a json rpc response object with the result or an error object
			JsonRpcResponseObject config(const std::string& name, bool debug);

			/// @ingroup remotePeer
			/// calls a method of the peer.
			/// @param path path of the merthod to call
			/// @param args nothing, an array with arguments or key value pairs of arguments
			/// \throws hbk::exception::jsonrpcException on error
			/// \returns the result object of the response on success.
			JsonRpcResponseObject callMethod(const std::string& path, const Json::Value& args);

			/// @ingroup remotePeer
			/// calls a method of the peer.
			/// @param path path of the merthod to call
			/// @param args nothing, an array with arguments or key value pairs of arguments
			/// @param timeout_s the timeout in seconds how long a routed request for this method call might last
			/// \throws hbk::exception::jsonrpcException on error
			/// \returns the result object of the response on success.
			JsonRpcResponseObject callMethod(const std::string& path, const Json::Value& args, double timeout_s);

			/// @ingroup remotePeer
			/// the fetch is being deregistered from jetd when the last instance of the returned shared pointer ist being destroyed.
			/// @param match what to fetch.
			/// @param callback being called on matching notifications
			/// \throws hbk::exception::jsonrpcException on error
			/// \return the fetch id
			fetchId_t addFetch(const matcher_t& match, fetchCallback_t callback);

			/// @ingroup remotePeer
			/// \return A snapshot of all matching states
			Json::Value get(const matcher_t& match);

			/// @ingroup remotePeer
			/// set the value of the state/complex state
			/// \throws std::runtime_error on error
			/// \return A warning state if != 0, i.e. value got adapted.
			SetStateResult setStateValue(const std::string& path, const Json::Value& value);

			/// @ingroup remotePeer
			/// set the value of the state/complex state
			/// @param path path of the state to be set
			/// @param value requested value
			/// @param timeout_s the timeout in seconds how long a routed request for this state set might last
			/// \throws hbk::exception::jsonrpcException on error
			/// \return A warning state if != 0, i.e. value got adapted.
			SetStateResult setStateValue(const std::string& path, const Json::Value& value, double timeout_s);

			/// @ingroup remotePeer
			/// set the value of the state/complex state
			/// @param path path of the state to be set
			/// @param value requested value
			/// @param resultCallback called with the response object when operation finished.
			/// \throws std::runtime_error
			void setStateValueAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup remotePeer
			/// set the value of the state/complex state
			/// @param path path of the state to be set
			/// @param value requested value
			/// @param timeout_s the timeout in seconds how long a routed request for this state set might last
			/// @param resultCallback called with the response object when operation finished.
			/// \throws std::runtime_error
			void setStateValueAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback=responseCallback_t());


			/// @ingroup owningPeer
			/// the peer serves a new method on jet
			/// @param path path of the method to be registered
			/// @param callback callback to be performed on method call
			/// \throws hbk::exception::jsonrpcException on error
			void addMethod(const std::string& path, methodCallback_t callback);

			/// @ingroup owningPeer
			/// the peer serves a new method on jet
			/// @param path path of the method to be registered
			/// @param timeout_s the timeout in seconds how long a routed request for this method might last
			/// @param callback callback to be performed on method call
			/// \throws hbk::exception::jsonrpcException on error
			void addMethod(const std::string& path, double timeout_s, methodCallback_t callback);

			/// @ingroup owningPeer
			/// the peer serves a new method on jet
			/// @param path the key onto which the new method will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this method
			/// @param callGroups list of user groups that are allowed to call this method
			/// @param callback function to be called when method is called via jet.
			/// @param timeout_s the timeout in seconds how long a routed request for this method might last
			/// \throws exception on error
			void addMethod(const std::string& path, const userGroups_t& fetchGroups,
			               const userGroups_t& callGroups, methodCallback_t callback,
			               double timeout_s);

			/// @ingroup owningPeer
			/// the peer serves a new method on jet
			/// @param path the key onto which the new method will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this method
			/// @param callGroups list of user groups that are allowed to call this method
			/// @param callback function to be called when method is called via jet.
			/// @param timeout_s the timeout in seconds how long a routed request for this method might last
			/// @param resultCallback called on completion providing the result
			/// \throws exception on error
			void addMethodAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& callGroups,
				methodCallback_t callback, double timeout_s, responseCallback_t resultCallback);

			/// @ingroup owningPeer
			/// The peer serves a new state
			/// @param path path of state in jet
			/// @param callback function to be called when state is set via jet. leave empty for read only states
			/// @param value initial value of the state
			/// \throws hbk::exception::jsonrpcException on error
			void addState(const std::string& path, const Json::Value& value, stateCallback_t callback = stateCallback_t());

			/// @ingroup owningPeer
			/// the peer serves a new state
			/// @param path path of state in jet
			/// @param callback function to be called when state is set via jet. leave empty for read only states
			/// @param value initial value of the state
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			/// \throws hbk::exception::jsonrpcException on error
			void addState(const std::string& path, const Json::Value& value, double timeout_s, stateCallback_t callback = stateCallback_t());

			/// @ingroup owningPeer
			/// the peer serves a new state on jet
			/// @param path the key onto which the new state will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this state
			/// @param setGroups list of user groups that are allowed to set this state
			/// @param value the initial value of the state
			/// @param callback function to be called when state is set via jet. Put nullptr in for read only states.
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			/// \throws exception on error
			void addState(const std::string& path, const userGroups_t& fetchGroups,
			              const userGroups_t& setGroups, const Json::Value& value,
			              double timeout_s, stateCallback_t callback = stateCallback_t());

			/// @ingroup owningPeer
			/// @param path the key onto which the new state will be published
			/// @param value the initial value of the state
			/// @param resultCallback called on completion providing the result
			/// @param callback function to be called when state is set via jet. leave empty for read only states
			/// \throws hbk::exception::jsonrpcException on error
			void addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// @param path the key onto which the new state will be published
			/// @param value the initial value of the state
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			/// @param resultCallback called on completion providing the result
			/// @param callback function to be called when state is set via jet. leave empty for read only states
			/// \throws hbk::exception::jsonrpcException on error
			void addStateAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// the peer serves a new state on jet
			/// @param path the key onto which the new state will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this state
			/// @param setGroups list of user groups that are allowed to set this state
			/// @param value the initial value of the state
			/// @param callback function to be called when state is set via jet. Put nullptr in for read only states.
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			/// @param resultCallback called on completion providing the result
			/// \throws exception on error
			void addStateAsync(const std::string& path, const Json::Value& value, const userGroups_t& fetchGroups, const userGroups_t& setGroups,
				double timeout_s, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// @param fetchId Id of the fetch to be removed
			/// @param resultCb Called when operation finished, nullptr if we are not interested in the response. This results in sending a notification instead of an request (no id).
			void removeFetchAsync(fetchId_t fetchId, responseCallback_t resultCb=responseCallback_t());

			/// @ingroup owningPeer
			/// the peer serves a new method on jet
			/// \throws hbk::exception::jsonrpcException on error
			void removeMethodAsync(const std::string& path, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup owningPeer
			/// @param path Path of the state to be removed from jet
			/// @param resultCb Called when operation finished, nullptr if we are not interested in the response. This results in sending a notification instead of an request (no id).
			void removeStateAsync(const std::string& path, responseCallback_t resultCb=responseCallback_t());

			template <class valueType>
			/// @ingroup owningPeer
			/// called by the jet peer to notify new state value to the jet daemon
			int notifyState(const std::string& path, valueType value)
			{
				return m_peerAsync.notifyState(path, value);
			}

			/// @ingroup anyPeer
			/// The jet peer singleton connecting to the local jet daemon
			static Peer& local();
		private:

			void addStatePrivate(const std::string& path, const Json::Value& value, Json::Value& params, stateCallback_t callback = stateCallback_t());
			void addMethodPrivate(const std::string& path, Json::Value& params, methodCallback_t callback);
			SetStateResult setStateValuePrivate(const std::string& path, const Json::Value& value, Json::Value& params);
			JsonRpcResponseObject callMethodPrivate(const std::string& path, const Json::Value& args, Json::Value& params);

			hbk::sys::EventLoop m_eventloop;
			std::thread m_workerThread;
			PeerAsync m_peerAsync;
		};
	}
}
