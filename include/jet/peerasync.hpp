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

#include <stdint.h>

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "boost/asio/io_context.hpp"

#include <json/value.h>
#include <json/reader.h>
#include "hbk/jsonrpc/jsonrpc_defines.h"
//#include "hbk/communication/socketnonblocking.h"

#include "stream/Stream.hpp"

#include "jet/defines.h"

namespace Json {
	class CharReader;
}

namespace hbk
{
	namespace sys {
		class Eventloop;
	}
	namespace jet
	{
		/// C++ jet peer for asynchronuous calls. Data is received asynchronuously in the context of the provided event loop which calls the receive method when data is available
		/// \note All methods that do not provide a timeout, have the default timeout of the jet daemon.
		/// \note All callback functions are executed in the eventloop context. Eventloop needs to be running and may not be blocked to have callback functions executed!
		class PeerAsync
		{
			friend class Peer;
		public:
			/** 
			 *  @defgroup remotePeer Methods called by remote jet peer
			 * Remote peer are accessing services of other peers.
			 */

			/** 
			 *  @defgroup owningPeer Methods called by jet peer that own states or methods
			 *  The peer that creates states or methods is the owning peer. Probably, this is the peer that offers the service.
			 */

			/** 
			 *  @defgroup anyPeer Methods called by any jet peer
			 */

			/// @ingroup anyPeer
			/// @throws std::runtime_error
			/// @param eventloop Data is received in the context of this event loop. Response callback functions are also executed int this context.
			/// @param address Ip address of the remote jetd or unix domain socket endpoint
			/// @param port default port is JETD_TCP_PORT, 0 means unix domain socket
			/// @param name Name of the jet peer is optional
			/// @param debug Switch debug log messages
			PeerAsync(boost::asio::io_context &eventloop, const std::string& address, unsigned int port=JETD_TCP_PORT, const std::string& name="", bool debug=false);

			/// may not be move assigned!
			PeerAsync& operator=(PeerAsync&& op) = delete;
			/// may not be moved!
			PeerAsync(PeerAsync&& op) = delete;

			/// may not be assigned!
			PeerAsync& operator=(const PeerAsync& op) = delete;
			/// may not be copied!
			PeerAsync(const PeerAsync& op) = delete;

			/// @ingroup anyPeer
			/// on destruction the socket is being closed. The jetd we were connected to will automatically remove all fetches, states, and methods belonging to the disconnected peer.
			virtual ~PeerAsync();

			/// @ingroup anyPeer
			/// @param resultCallback called with the response object when operation finished.
			void infoAsync(responseCallback_t resultCallback);

			/// @ingroup anyPeer
			/// @param name User defined name
			/// @param debug Switch debug log messages
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			void configAsync(const std::string& name, bool debug, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup anyPeer
			/// try to reconnect to jetd after loss of connection
			bool resume();

			/// @ingroup anyPeer
			/// the peer authenticates itself against the daemon
			/// @param user user name
			/// @param password password of user
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			void authenticateAsync(const std::string& user, const std::string& password, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup remotePeer
			/// calls a method of the remote peer.
			/// @param path path of the method to call
			/// @param args nothing or an array with arguments
			/// @param resultCb called called on completion or error providing the result. Executed in eventloop eontext.
			void callMethodAsync(const std::string& path, const Json::Value& args, responseCallback_t resultCb);

			/// @ingroup remotePeer
			/// calls a method of the remote peer.
			/// @param path path of the method to call
			/// @param args nothing or an array with arguments
			/// @param timeout_s the timeout in seconds how long a routed request for this call might last
			/// @param resultCb called on completion or error providing the result. Executed in eventloop eontext.
			void callMethodAsync(const std::string& path, const Json::Value& args, double timeout_s, responseCallback_t resultCb);

			/// @ingroup remotePeer
			/// Subscribes to all changes made to states matching the filter criteria
			/// \param match the filter used
			/// \param callback Called on any matching notification. Executed in eventloop eontext.
			/// \param resultCb called on completion or error providing the result. Executed in eventloop eontext.
			/// \return the fetch id
			fetchId_t addFetchAsync(const matcher_t& match, fetchCallback_t callback, responseCallback_t resultCb=responseCallback_t());

			/// @ingroup remotePeer
			/// @param fetchId id of the fetch to remove
			/// @param resultCb called on completion or error providing the result. Executed in eventloop eontext.
			void removeFetchAsync(fetchId_t fetchId, responseCallback_t resultCb=responseCallback_t());

			/// @ingroup remotePeer
			/// resultCallback will get a snapshot of all matching remote states. Result contains the data as an array of objects:
			/// \code
			/// {
			///		"result":
			///		[
			///			{ "path": "path/one", "value": "1"},
			///			{ "path": "path/two", "value": "2"}
			///		]
			/// }
			/// \param match the filter used
			/// \endcode
			void getAsync(const matcher_t& match, responseCallback_t resultCallback);

			/// @ingroup remotePeer
			/// set the value of the remote state
			void setStateValueAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup remotePeer
			/// set the value of the remote state
			/// @param path Path of the state to set
			/// @param value Requested value for the state
			/// @param timeout_s Timeout in seconds how long to wait for the response
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			void setStateValueAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup owningPeer
			/// The peer serves a new method on jet. Other peers can call the method.
			/// @param callback Callback function executed when registered method gets called. Executed in eventloop eontext.
			void addMethodAsync(const std::string& path, responseCallback_t resultCallback, methodCallback_t callback);

			/// @ingroup owningPeer
			/// The peer serves a new method on jet. Other peers can call the method.
			/// @param path Path of the state to set
			/// @param timeout_s the timeout in seconds how long a routed request for this method might last
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			/// @param callback Callback function executed when registered method gets called. Executed in eventloop eontext.
			void addMethodAsync(const std::string& path, double timeout_s, responseCallback_t resultCallback, methodCallback_t callback);

			/// @ingroup owningPeer
			/// The peer serves a new method on jet. Other peers can call the method.
			/// @param path the key onto which the new method will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this method
			/// @param callGroups list of user groups that are allowed to call this method
			/// @param callback Callback function executed when registered method gets called. Executed in eventloop eontext.
			/// @param timeout_s the timeout in seconds how long a routed request for this method might last
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			void addMethodAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& callGroups,
				methodCallback_t callback, double timeout_s, responseCallback_t resultCallback);

			/// @ingroup owningPeer
			/// The peer no longer serves the method.
			void removeMethodAsync(const std::string& path, responseCallback_t resultCallback=responseCallback_t());

			/// @ingroup owningPeer
			/// The peer serves a new state on jet. Other peers can fetch or set the state.
			/// @param path Path of the new state
			/// @param value Initial value of the state
			/// @param resultCallback called on completion or error providing the result
			/// @param callback function to be called when state is set via jet. Executed in eventloop eontext. leave empty for read only states.
			void addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// The peer serves a new state on jet Other peers can fetch or set the state.
			/// @param path Path of the new state
			/// @param value Initial value of the state
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			/// @param callback function to be called when state is set via jet. Executed in eventloop eontext. leave empty for read only states
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			void addStateAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// The peer serves a new state on jet Other peers can fetch or set the state.
			/// @param path the key onto which the new state will be published
			/// @param fetchGroups list of user groups that are allowed to fetch this state
			/// @param setGroups list of user groups that are allowed to set this state
			/// @param value the initial value of the state
			/// @param callback function to be called when state is set via jet. Executed in eventloop eontext. Put nullptr in for read only states.
			/// @param timeout_s the timeout in seconds how long a routed request for this state might last
			/// @param resultCallback called on completion or error providing the result. Executed in eventloop eontext.
			void addStateAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& setGroups,
				const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback);

			/// @ingroup owningPeer
			/// @param path Path of the state to be removed
			/// The peer no longer serves the state
			/// @param resultCb called when operation finished, nullptr if we are not interested in the response. This results in sending a notification instead of an request (no id).
			void removeStateAsync(const std::string& path, responseCallback_t resultCb=responseCallback_t());

			/// @ingroup owningPeer
			/// the peer serving the state notifies a change to the jet daemon. All other fetching peers are notified.
			template <class valueType>
			int notifyState(const std::string& path, valueType value);

			/// @param value payload to be send
			/// \throws exception on error
			void sendMessage(const Json::Value &value);

//			/// If using yout own event loop, wait for this to get readable before calling receive()
//			sys::event getReceiverEvent() const
//			{
//				return m_socket.getEvent();
//			}

//			hbk::sys::EventLoop& getEventLoop() const
//			{
//				return m_eventLoop;
//			}

			/// Called by event loop if data is available for read.
			/// receives and processes messages until there is nothing to be received.
			/// If a message was received partially, it is being resumed on the next call.
			/// \warning non-reentrant
			/// \return -1: error, 0: nothing to be read
			int receive();

			void onSizeReceive(const boost::system::error_code&);
			void onPayloadReceive(const boost::system::error_code&);


		protected:
			/// the path of the method is the key.
			using methodCallbacks_t = std::unordered_map < std::string, methodCallback_t >;
			/// the path of the state is the key.
			using stateCallbacks_t = std::unordered_map < std::string, stateCallback_t >;

			/// fetch id is the key
			using fetchers_t = std::unordered_map < fetchId_t, fetcher_t >;

			/// Connect to jet daemon and start jet peer
			/// \throws std::runtime_error
			void start();
			/// Disconnect from jet daemon and stop jet peer
			void stop();

		private:
			void addMethodResultCb(const Json::Value& result, const std::string& path);
			void addStateResultCb(const Json::Value& result, const std::string& path);
			void addFetchResultCb(const Json::Value& result, fetchId_t fetchId);

			void registerFetch(fetchId_t fetchId, const fetcher_t& fetcher);
			void registerMethod(const std::string& path, methodCallback_t callback);
			void registerState(const std::string& path, stateCallback_t callback);

			void unregisterFetch(fetchId_t fetchId);
			void unregisterMethod(const std::string& path);
			void unregisterState(const std::string& path);

			static void addPathInformation(Json::Value& params, const matcher_t& match);
			
			/// create a new fetch id
			static fetchId_t createFetchId();

			void addMethodAsyncPrivate(const std::string& path, Json::Value& params, responseCallback_t resultCallback, methodCallback_t callback);

			/// restore a fetch already known in the internal structures. This is done when reconnecting after loosing connection to jetd.
			void restoreFetch(const matcher_t& match, fetchId_t fetchId);

			/// called when a complete packet arrived. This might contain a single jet message or a batch of several jet messages.
			void receiveCallback(const Json::Value& data);

			/// Handles all kinds of messages coming in
			/// 
			/// setting a state and executing a method look the same:
			/// \code
			/// {
			///		"id" : "<transaction id (Important for the response. Might be omitted when there should be no response)>",
			///		"method" : "<path of the state to change/method to call>",
			///		"params" :
			///		{
			///			<object with the requested state value/method parameters
			///		}
			/// }
			/// \endcode
			void handleMessage(const Json::Value& data);

			void addStateAsyncPrivate(const std::string& path, const Json::Value& value, Json::Value& params, responseCallback_t resultCallback, stateCallback_t callback);
			void setStateValueAsyncPrivate(const std::string& path, const Json::Value& value, Json::Value& params, responseCallback_t resultCallback);
			void callMethodAsyncPrivate(const std::string& path, const Json::Value& args, Json::Value& params, responseCallback_t resultCb);

			/// name or tcp address of jetd
			/// name of unix domain socket
			std::string m_address;
			/// tcp port of jetd or 0 if using unix domain sockets
			unsigned int m_port;
			std::string m_name;
			bool m_debug;

			boost::asio::io_context& m_eventLoop;
			std::unique_ptr<daq::stream::Stream> m_stream;
			//hbk::communication::SocketNonblocking m_socket;
			volatile bool m_stopped;


			std::mutex m_sendMutex;
			std::mutex m_receiveMutex; /// is needed when working with ThreadPools and external eventloops. e.g. Qt

			/// buffer for length information of each received jet telegram
			uint32_t m_bigEndianLengthBuffer;
			size_t m_lengthBufferLevel;


			stateCallbacks_t m_stateCallbacks;
			/// user defined callback triggered from handleObject might create or detroy a state.
			std::recursive_mutex m_mtx_stateCallbacks;
			methodCallbacks_t m_methodCallbacks;
			/// user defined callback triggered from handleObject might create or detroy a method.
			std::recursive_mutex m_mtx_methodCallbacks;
			fetchers_t m_fetchers;
			/// user defined callback triggered from handleObject might create or detroy a fetch.
			std::recursive_mutex m_mtx_fetchers;


			/// payload size of jet telegram
			size_t m_payloadSize;

			/// this is use in a synchronized sequence. Hence we create in only once and reuse it.
			std::unique_ptr<Json::CharReader> const m_reader;
			std::string parseErrors;

			static std::atomic <fetchId_t > m_sfetchId;
		};

		template <class valueType>
		/// we tell the jet daemon about the new value of the state. We do not send an id, hence jetd will not give us an response. This increases performance a lot.
		int PeerAsync::notifyState(const std::string& path, valueType value)
		{
			Json::Value data;

			data[jsonrpc::METHOD] = CHANGE;
			Json::Value& params = data[jsonrpc::PARAMS];
			params[PATH] = path;
			params[VALUE] = value;

			try {
				sendMessage(data);
			} catch(...) {
				return -1;
			}
			return 0;
		}
	}
}
