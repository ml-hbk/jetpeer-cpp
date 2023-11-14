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

#include <cstring>
#include <stdexcept>
#include <functional>
#include <mutex>


#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <poll.h>
#include <syslog.h>
#else
#include <WinSock2.h>
#undef max
#undef min
#define syslog fprintf
#define LOG_INFO stdout
#define LOG_DEBUG stdout
#define LOG_WARNING stdout
#define LOG_ERR stderr
#define LOG_CRIT stderr
#endif

#include "jet/peerasync.hpp"
#include "jet/defines.h"
#include "asyncrequest.h"

#include "hbk/sys/eventloop.h"


/// A valid composed message always starts with '{' and ends with '}'

/// Composition of empty json values produces the special string "null". 
/// This is not valid but does not result in an error since cjet and receiving jet peer will also just ignore it.
/// Those cases are logged.
/// 
/// All other invalid cases are logged and an exception is thrown
//#define CHECK_COMPOSED_MESSAGE_BEFORE_SENDING

namespace hbk {
	namespace jet {
		static Json::CharReaderBuilder rBuilder;
		static Json::StreamWriterBuilder wBuilder;

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
			, m_reader(rBuilder.newCharReader())
		{
			// Compose json without indentation. This saves lots of bandwidth and time!
			wBuilder.settings_["indentation"] = "";
			start();
		}

		PeerAsync::~PeerAsync()
		{
			stop();
			{
				// jet daemon automatically unregisters all fetches on disconnect we simply forget all known fetches
				std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
				m_fetchers.clear();
			}
		}

		void PeerAsync::start()
		{
			// clear all buffers. Important for reconnect.
			m_lengthBufferLevel = 0;
			m_dataBuffer.clear();
			m_dataBufferLevel = 0;



			int retVal;

			if (m_port!=0) {
				// connect via tcp
				std::string portString(std::to_string(m_port));
				retVal = m_socket.connect(m_address, portString);
				if (retVal<0) {
					std::string msg;
					msg = "jet peerAsync could not connect to jetd (tcp: " + m_address + ":" + portString + ")!";
					throw std::runtime_error(msg);
				}
			} else {
				// unix domain socket (not supported by windows)
#ifdef _WIN32
				retVal = m_socket.connect("127.0.0.1", std::to_string(hbk::jet::JETD_TCP_PORT));
				if (retVal<0) {
					std::string msg;
					msg = "jet peerAsync could not connect to jetd (tcp: localhost:" + std::to_string(hbk::jet::JETD_TCP_PORT) + ")!";
					throw std::runtime_error(msg);
				}
#else
				retVal = m_socket.connect(m_address);
				if (retVal<0) {
					std::string msg;
					msg = "jet peerAsync could not connect to jetd (" + m_address + ")!";
					throw std::runtime_error(msg);
				}
#endif
			}
			m_socket.setDataCb(std::bind(&PeerAsync::receive, this));

			configAsync(m_name, m_debug);
			{
				// restore all known fetches
				std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
				for (const auto &iter: m_fetchers) {
					const fetcher_t& fetcher = iter.second;
					try {
						restoreFetch(fetcher.matcher, iter.first);
					} catch(const std::runtime_error& e) {
						::syslog(LOG_ERR, "restoration of previous fetches failed ('%s')!", e.what());
					} catch(...) {
						::syslog(LOG_ERR, "restoration of previous fetches failed");
					}
				}
			}
		}

		void PeerAsync::stop()
		{
			syslog(LOG_DEBUG, "jet peer '%s' %s:%u: Stopping...", m_name.c_str(), m_address.c_str(), m_port);
			m_stopped = true;

			m_socket.disconnect();

			// Notify all fetchers
			Json::Value empty;
			{
				std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
				for (auto &iter: m_fetchers) {
					fetchCallback_t& callback = iter.second.callback;
					try {
						callback(empty, -1);
					} catch(...)
					{
						// catch and ignore all exceptions
					}
				}
			}

			// all states and methods registered are to be removed!
			{
				std::lock_guard < std::recursive_mutex > lock(m_mtx_stateCallbacks);
				m_stateCallbacks.clear();
			}

			{
				std::lock_guard < std::recursive_mutex > lock(m_mtx_methodCallbacks);
				m_methodCallbacks.clear();
			}

			size_t clearedRequestCount = AsyncRequest::clear();
			if (clearedRequestCount>0) {
				::syslog(LOG_WARNING, "%zu open request(s) left on destruction of jet peer %s. All open requests have been canceled!", clearedRequestCount, m_address.c_str());
			}
		}

		bool PeerAsync::resume()
		{
			try {
				start();
				return true;
			} catch(...) {
				return false;
			}
		}

		int PeerAsync::receive()
		{

			std::lock_guard < std::mutex > lck(m_receiveMutex);

			while (true) {
				// Receive until error or EWOULDBLOCK. It is important to read from jet damon as fast possible.
				while (m_lengthBufferLevel < sizeof(m_bigEndianLengthBuffer)) {
					// read length information
					uint8_t* plengthBuffer = reinterpret_cast < uint8_t* > (&m_bigEndianLengthBuffer);
					ssize_t retVal = m_socket.receive(plengthBuffer+m_lengthBufferLevel, sizeof(m_bigEndianLengthBuffer)-m_lengthBufferLevel);
					if (retVal<0) {
#ifdef _WIN32
						int lastError = WSAGetLastError();
						if ((lastError == WSAEWOULDBLOCK) || (lastError == ERROR_IO_PENDING)) {
#else
						if(errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
							return 0;
						}
						syslog(LOG_ERR, "jet peer %s:%u: Error on receive '%s'", m_address.c_str(), m_port, strerror(errno));
						stop();
						return -1;
					} else if (retVal == 0) {
						syslog(LOG_DEBUG, "jet peer %s:%u: Connection closed", m_address.c_str(), m_port);
						stop();
						return 0;
					}
					m_lengthBufferLevel += static_cast < size_t > (retVal);

					if (m_lengthBufferLevel == sizeof(m_bigEndianLengthBuffer)) {
						// length information is complete: Prepare data buffer
						size_t len = ntohl(m_bigEndianLengthBuffer);
						if (len>MAX_MESSAGE_SIZE) {
							syslog(LOG_ERR, "jet peer %s:%u: Received message size (%zu) exceeds maximum message size (%zu). Closing connection!", m_address.c_str(), m_port, len, MAX_MESSAGE_SIZE);
							stop();
							return -1;
						}

						m_dataBuffer.resize(len);
					}
				}

				while(m_dataBufferLevel<m_dataBuffer.size()) {
					// length information is complete, proceed reading data
					ssize_t retVal = m_socket.receive(m_dataBuffer.data()+m_dataBufferLevel, m_dataBuffer.size()-m_dataBufferLevel);
					if(retVal<0) {
#ifdef _WIN32
						int lastError = WSAGetLastError();
						if ((lastError == WSAEWOULDBLOCK) || (lastError == ERROR_IO_PENDING)) {
#else
						if(errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
							return 0;
						}
						syslog(LOG_ERR, "jet peer %s:%u: Error on receive '%s'", m_address.c_str(), m_port, strerror(errno));
						stop();
						return -1;
					} else if (retVal == 0) {
						stop();
						return 0;
					}
					m_dataBufferLevel += static_cast < size_t > (retVal);
				}

				// data package is complete. Process data and clear buffers.
				Json::Value data;
				// add space for the end to point to.
				m_dataBuffer.push_back('\0');
				if (m_reader->parse(m_dataBuffer.data(), &m_dataBuffer[m_dataBuffer.size()-1], &data, &parseErrors)) {
					receiveCallback(data);
				} else {
					if (m_dataBuffer.size() <= 2048 ) {
						// Don't put more into syslog!
						// Most likely we are somewhat lost in the stream. Have also a binary dump to allow forensic analysis
						std::stringstream binaryDump;
						binaryDump << std::hex;
						int binaryCharacter;

						for(auto iter : m_dataBuffer) {
							binaryCharacter = iter;
							binaryDump << binaryCharacter; //+= std::to_string(character);
							binaryDump << " ";
						}
						syslog(LOG_ERR, "jet peer %s:%u: Error '%s' while parsing received telegram (%zu byte) %s", m_address.c_str(), m_port, parseErrors.c_str(), m_dataBuffer.size(), binaryDump.str().c_str());
						syslog(LOG_ERR, "jet peer %s:%u: Error '%s' while parsing received telegram (%zu byte) '%.*s'", m_address.c_str(), m_port, parseErrors.c_str(), m_dataBuffer.size(), static_cast< int > (m_dataBuffer.size()), m_dataBuffer.data());
					} else {
						syslog(LOG_ERR, "jet peer %s:%u: Error '%s' while parsing received telegram (%zu byte)", m_address.c_str(), m_port, parseErrors.c_str(), m_dataBuffer.size());
					}
				}
				m_dataBuffer.clear();
				m_lengthBufferLevel = 0;
				m_dataBufferLevel = 0;
			}
		}

		void PeerAsync::infoAsync(responseCallback_t resultCallback)
		{
			Json::Value params;
			AsyncRequest method(INFO, params);
			method.execute(*this, resultCallback);
		}

		void PeerAsync::configAsync(const std::string& name, bool debug, responseCallback_t resultCallback)
		{
			Json::Value params;
			params[NAME] = name;
			params[DBG] = debug;
			AsyncRequest method(CONFIG, params);
			method.execute(*this, resultCallback);
		}

		void PeerAsync::callMethodAsync(const std::string& path, const Json::Value& args, responseCallback_t resultCb)
		{
			Json::Value params;
			callMethodAsyncPrivate(path, args, params, resultCb);
		}

		void PeerAsync::callMethodAsync(const std::string& path, const Json::Value& args, double timeout_s, responseCallback_t resultCb)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			callMethodAsyncPrivate(path, args, params, resultCb);
		}

		void PeerAsync::callMethodAsyncPrivate(const std::string& path, const Json::Value& args, Json::Value& params, responseCallback_t resultCb)
		{
			params[PATH] = path;

			if(!args.isNull()) {
				params[ARGS] = args;
			}
			AsyncRequest method(CALL, params);
			method.execute(*this, resultCb);
		}

		void PeerAsync::addMethodAsync(const std::string& path, responseCallback_t resultCallback, methodCallback_t callback)
		{
			Json::Value params;
			addMethodAsyncPrivate(path, params, resultCallback, callback);
		}

		void PeerAsync::addMethodAsync(const std::string& path, double timeout_s, responseCallback_t resultCallback, methodCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;;
			addMethodAsyncPrivate(path, params, resultCallback, callback);
		}

		void PeerAsync::addMethodAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& callGroups,
			methodCallback_t callback, double timeout_s, responseCallback_t resultCallback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			if (!fetchGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string& it: fetchGroups) {
					groups.append(it);
				}

				params[ACCESS][FETCH_GROUPS] = groups;
			}

			if (!callGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string& it: callGroups) {
					groups.append(it);
				}

				params[ACCESS][CALL_GROUPS] = groups;
			}

			addMethodAsyncPrivate(path, params, std::move(resultCallback), std::move(callback));
		}

		void PeerAsync::addMethodAsyncPrivate(const std::string& path, Json::Value& params, responseCallback_t resultCallback, methodCallback_t callback)
		{
			params[PATH] = path;

			registerMethod(path, callback);
			AsyncRequest request(ADD, params);
			if (!resultCallback) {
				request.execute(*this);
			} else {
				auto lambda = [this, path, resultCallback](const Json::Value& result)
				{
					addMethodResultCb(result, path);
					if (resultCallback) {
						try {
							resultCallback(result);
						} catch(...) {
						}
					}
				};
				request.execute(*this, lambda);
			}
		}

		void PeerAsync::removeMethodAsync(const std::string& path, responseCallback_t resultCallback)
		{
			if(path.empty()) {
				return;
			}

			unregisterMethod(path);

			Json::Value params;
			params[PATH] = path;
			AsyncRequest method(REMOVE, params);
			method.execute(*this, resultCallback);
		}

		void PeerAsync::addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback, stateCallback_t callback)
		{
			Json::Value params;
			addStateAsyncPrivate(path, value, params, resultCallback, callback);
		}


		void PeerAsync::addStateAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			addStateAsyncPrivate(path, value, params, resultCallback, callback);
		}

		void PeerAsync::addStateAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& setGroups,
		                              const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			if (!fetchGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string& it: fetchGroups) {
					groups.append(it);
				}

				params[ACCESS][FETCH_GROUPS] = groups;
			}

			if (!setGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string& it: setGroups) {
					groups.append(it);
				}

				params[ACCESS][SET_GROUPS] = groups;
			}

			addStateAsyncPrivate(path, value, params, resultCallback, callback);
		}

		void PeerAsync::addStateAsyncPrivate(const std::string& path, const Json::Value& value, Json::Value& params, responseCallback_t resultCallback, stateCallback_t callback)
		{
			params[PATH] = path;
			params[VALUE] = value;
			if (!callback) {
				params[FETCHONLY] = true;
			}

			AsyncRequest request(ADD, params);
			registerState(path, std::move(callback));
			if (!resultCallback) {
				request.execute(*this);
			} else {
				auto lambda = [this, path, resultCallback](const Json::Value& result)
				{
					addStateResultCb(result, path);
					try {
						resultCallback(result);
					} catch(...) {
					}
				};
				request.execute(*this, lambda);
			}
		}

		void PeerAsync::removeStateAsync(const std::string& path, responseCallback_t resultCb)
		{
			if(path.empty()) {
				return;
			}


			unregisterState(path);

			Json::Value params;
			params[PATH] = path;
			AsyncRequest method(REMOVE, params);
			method.execute(*this, std::move(resultCb));
		}

		void PeerAsync::addMethodResultCb(const Json::Value& result, const std::string& path)
		{
			if (result.isMember(jsonrpc::ERR)) {
				unregisterMethod(path);
			}
		}

		void PeerAsync::addStateResultCb(const Json::Value& result, const std::string& path)
		{
			if (result.isMember(jsonrpc::ERR)) {
				unregisterState(path);
			}
		}

		void PeerAsync::addFetchResultCb(const Json::Value& result, fetchId_t fetchId)
		{
			if (result.isMember(jsonrpc::ERR)) {
				unregisterFetch(fetchId);
			}
		}


		void PeerAsync::registerFetch(fetchId_t fetchId, const fetcher_t& fetcher)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
			m_fetchers[fetchId] = fetcher;
		}

		void PeerAsync::registerMethod(const std::string& path, methodCallback_t callback)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_methodCallbacks);
			m_methodCallbacks[path] = callback;
		}

		void PeerAsync::registerState(const std::string& path, stateCallback_t callback)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_stateCallbacks);
			m_stateCallbacks[path] = callback;
		}

		void PeerAsync::unregisterFetch(fetchId_t fetchId)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
			m_fetchers.erase(fetchId);
		}

		void PeerAsync::unregisterMethod(const std::string& path)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_methodCallbacks);
			m_methodCallbacks.erase(path);
		}

		void PeerAsync::unregisterState(const std::string& path)
		{
			std::lock_guard < std::recursive_mutex > lock(m_mtx_stateCallbacks);
			m_stateCallbacks.erase(path);
		}


		void PeerAsync::setStateValueAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback)
		{
			Json::Value params;
			setStateValueAsyncPrivate(path, value, params, std::move(resultCallback));
		}

		void PeerAsync::setStateValueAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			setStateValueAsyncPrivate(path, value, params, std::move(resultCallback));
		}

		void PeerAsync::setStateValueAsyncPrivate(const std::string& path, const Json::Value& value, Json::Value& params, responseCallback_t resultCallback)
		{
			params[PATH] = path;
			params[VALUE] = value;

			AsyncRequest method(SET, params);
			method.execute(*this, resultCallback);
		}

		fetchId_t PeerAsync::createFetchId()
		{
			return ++m_sfetchId;
		}

		void PeerAsync::authenticateAsync(const std::string& user, const std::string& password, responseCallback_t resultCallback)
		{
			Json::Value params;

			params[USER] = user;
			params[PASSWORD] = password;

			AsyncRequest method(AUTHENTICATE, params);
			method.execute(*this, resultCallback);
		}

		void PeerAsync::addPathInformation(Json::Value& params, const matcher_t& match)
		{
			if (!match.contains.empty()) {
				params[PATH][CONTAINS] = match.contains;
			}
			if (!match.startsWith.empty()) {
				params[PATH][STARTSWITH] = match.startsWith;
			}
			if (!match.endsWith.empty()) {
				params[PATH][ENDSWITH] = match.endsWith;
			}
			if (!match.equals.empty()) {
				params[PATH][EQUALS] = match.equals;
			}
			if (!match.equalsNot.empty()) {
				params[PATH][EQUALSNOT] = match.equalsNot;
			}
			if (!match.containsAllOf.empty()) {
				for (const auto& iter : match.containsAllOf) {
					params[PATH][CONTAINSALLOF].append(iter);
				}
			}
			if (match.caseInsensitive) {
				params[PATH][CASEINSENSITIVE] = true;
			}
		}

		void PeerAsync::getAsync(const matcher_t& match, responseCallback_t resultCallback)
		{
			Json::Value params;
			addPathInformation(params, match);

			AsyncRequest request(GET, params);
			request.execute(*this, resultCallback);
		}

		fetchId_t PeerAsync::addFetchAsync(const matcher_t& match, fetchCallback_t callback, responseCallback_t resultCb)
		{
			Json::Value params;
			fetchId_t fetchId = createFetchId();
			params[jsonrpc::ID] = fetchId;
			addPathInformation(params, match);

			registerFetch(fetchId, fetcher_t(std::move(callback), match));
			AsyncRequest request(FETCH, params);
			if (!resultCb) {
				request.execute(*this);
			} else {
				auto lambda = [this, fetchId, resultCb](const Json::Value& result)
				{
					addFetchResultCb(result, fetchId);
					if (resultCb) {
						try {
							resultCb(result);
						} catch (...) {
						}
					}
				};
				request.execute(*this, lambda);
			}

			return fetchId;
		}

		void PeerAsync::restoreFetch(const matcher_t& match, fetchId_t fetchId)
		{
			Json::Value params;
			params[jsonrpc::ID] = fetchId;
			addPathInformation(params, match);

			AsyncRequest request(FETCH, params);
			request.execute(*this, responseCallback_t());
		}

		void PeerAsync::removeFetchAsync(fetchId_t fetchId, responseCallback_t resultCb)
		{
			unregisterFetch(fetchId);

			Json::Value params;

			params[jsonrpc::ID] = fetchId;
			AsyncRequest request(UNFETCH, params);
			request.execute(*this, resultCb);
		}

		void PeerAsync::sendMessage(const Json::Value& value)
		{
			int result;
			
			
			std::string msg = Json::writeString(wBuilder, value);
			size_t len = msg.length();
			if (len>MAX_MESSAGE_SIZE) {
				std::string errorMsg;
				errorMsg = "Message size " + std::to_string(len) + " exceeds maximum message size (" + std::to_string(MAX_MESSAGE_SIZE) + ") and will not be send!";
				syslog(LOG_ERR, "%s", errorMsg.c_str());
				throw hbk::exception::jsonrpcException(-1, errorMsg);
			}
			uint32_t lenBig = htonl(static_cast < uint32_t > (len));
			communication::dataBlock_t dataBlocks[] = {
				{ &lenBig, sizeof(lenBig) },
				{ msg.c_str(), msg.length()},
			};

			{
				// synchronize sending complete message!!!
				std::lock_guard < std::mutex > lock(m_sendMutex);
				result = static_cast < int > (m_socket.sendBlocks(dataBlocks, sizeof(dataBlocks)/sizeof(communication::dataBlock_t), false));
			}
			if (result < 0) {
				std::string msg;
				msg = std::string("could not send message: '") + strerror(errno) + "'";
				syslog(LOG_ERR, "%s", msg.c_str());
				throw hbk::exception::jsonrpcException(-1, msg);
			}
		}

		void PeerAsync::receiveCallback(const Json::Value &data)
		{
			Json::ValueType type = data.type();
			switch(type) {
			case Json::arrayValue:
				// batch
				for (const Json::Value& element: data) {
					handleMessage(element);
				}
				break;
			case Json::objectValue:
				// a single object
				handleMessage(data);
				break;
			default:
				syslog(LOG_ERR, "Jet requests are to be a json objects or an array of json objets");
				break;
			}
		}

		void PeerAsync::handleMessage(const Json::Value &data)
		{
			const Json::Value& methodNode = data[jsonrpc::METHOD];
			Json::ValueType valueType = methodNode.type();
			switch (valueType) {
			case Json::nullValue:
				// result or error to a request
				AsyncRequest::handleResult(data);
				break;
			case Json::intValue:
				// this jet peer implementation uses unsigned numbers as fetch id when creating a fetch.
				// The method inside fetch notifications is of the same type
				{
					fetchId_t fetchId = methodNode.asInt();
					std::lock_guard < std::recursive_mutex > lock(m_mtx_fetchers);
					auto iter = m_fetchers.find(fetchId);
					if (iter!=m_fetchers.end()) {
						// it is a notification for a fetch
						const Json::Value& params = data[jsonrpc::PARAMS];
						try {
							iter->second.callback(params, 0);
						} catch(const std::runtime_error &e) {
							syslog(LOG_ERR, "Fetch callback '%s' threw exception '%s'!", iter->second.matcher.print().c_str(), e.what());
						} catch(...) {
							syslog(LOG_ERR, "Fetch callback '%s' threw exception!", iter->second.matcher.print().c_str());
						}
					}
				}
				break;
			case Json::stringValue:
				// this is any kind of request or notification
				{
					const std::string method(methodNode.asString());
					{
						std::lock_guard < std::recursive_mutex > lock(m_mtx_stateCallbacks);
						auto iter = m_stateCallbacks.find(method);
						if (iter != m_stateCallbacks.end()) {
							// it is a state!
							const Json::Value& value = data[jsonrpc::PARAMS][VALUE];

							if (!value.isNull()) {
								Json::Value response;
								stateCallback_t& callback = iter->second;
								if (!callback) {
									response[jsonrpc::ERR][jsonrpc::CODE] = jsonrpc::internalError;
									response[jsonrpc::ERR][jsonrpc::MESSAGE] = "state is read only!";
								} else {
									try {
										SetStateCbResult stateCallbackResult = callback(value, method);
										const Json::Value& notifyValue = stateCallbackResult.value;
										if (!notifyValue.isNull()) {
											// Notifies the changed value. This happens before eventually sending the response.
											// If there is no change, there is no notification.
											Json::Value sendata;
											sendata[jsonrpc::METHOD] = CHANGE;
											Json::Value& dataparams = sendata[jsonrpc::PARAMS];
											dataparams[PATH] = method;
											dataparams[VALUE] = notifyValue;
											sendMessage(sendata);
										}

										static const Json::Value SUCCESS_RESPONSE = Json::Value(Json::objectValue);
										if (stateCallbackResult.result.code) {
											response[jsonrpc::RESULT][WARNING][jsonrpc::CODE] = stateCallbackResult.result.code;
											if (!stateCallbackResult.result.message.empty()) {
												response[jsonrpc::RESULT][WARNING][jsonrpc::MESSAGE] = stateCallbackResult.result.message;
											}
										} else {
											response[jsonrpc::RESULT] = SUCCESS_RESPONSE;
										}
									} catch (const jsoncpprpcException& e) {
										response = e.json();
									} catch (const hbk::exception::jsonrpcException& e) {
										response[jsonrpc::ERR][jsonrpc::CODE] = e.code();
										response[jsonrpc::ERR][jsonrpc::MESSAGE] = e.message();
									} catch (const std::exception& e) {
										response[jsonrpc::ERR][jsonrpc::CODE] = jsonrpc::internalError;
										response[jsonrpc::ERR][jsonrpc::MESSAGE] = e.what();
									} catch (...) {
										response[jsonrpc::ERR][jsonrpc::CODE] = jsonrpc::internalError;
										response[jsonrpc::ERR][jsonrpc::MESSAGE] = "caught exception!";
									}
								}

								const Json::Value& idNode = data[jsonrpc::ID];
								if (idNode) {
									response[jsonrpc::ID] = idNode;
									try {
										sendMessage(response);
									} catch (const hbk::exception::jsonrpcException& e) {
										syslog(LOG_ERR, "jet peer: Unable to send %s", e.message().c_str());
									}
								}
							}
							return;
						}
					}
					{
						std::lock_guard < std::recursive_mutex > lock(m_mtx_methodCallbacks);
						const auto iter = m_methodCallbacks.find(method);
						if (iter != m_methodCallbacks.end()) {
							// it is a method!
							Json::Value response;
							try {
								const Json::Value& params = data[jsonrpc::PARAMS];
								response[jsonrpc::RESULT] = iter->second(params);
							} catch(const jsoncpprpcException& e) {
								response = e.json();
							} catch (const hbk::exception::jsonrpcException& e) {
								response[jsonrpc::ERR][jsonrpc::CODE] = e.code();
								response[jsonrpc::ERR][jsonrpc::MESSAGE] = e.message();
							} catch(const std::exception& e) {
								response[jsonrpc::ERR][jsonrpc::CODE] = jsonrpc::internalError;
								response[jsonrpc::ERR][jsonrpc::MESSAGE] = e.what();
							} catch(...) {
								response[jsonrpc::ERR][jsonrpc::CODE] = jsonrpc::internalError;
								response[jsonrpc::ERR][jsonrpc::MESSAGE] = "caught exception!";
							}
							const Json::Value& idNode = data[jsonrpc::ID];
							if (idNode) {
								response[jsonrpc::ID] = idNode;
								try {
									sendMessage(response);
								} catch (const hbk::exception::jsonrpcException& e) {
									syslog(LOG_ERR, "jet peer: Unable to send %s", e.message().c_str());
								}
							}
							return;
						}
					}
					syslog(LOG_ERR, "jet peer: unknown request or notification '%s'", method.c_str());
				}
				break;
			default:
				break;
			}
		}
	} // namespace jet
} // namespace hbk
