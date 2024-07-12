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

#include "boost/asio/io_context.hpp"
//#include "hbk/sys/eventloop.h"

#include "stream/LocalClientStream.hpp"
#include "stream/TcpClientStream.hpp"

#include "jet/peerasync.hpp"
#include "jet/defines.h"
#include "asyncrequest.h"



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


		//PeerAsync::PeerAsync(sys::EventLoop& eventloop, const std::string &address, unsigned int port, const std::string& name, bool debug)
		PeerAsync::PeerAsync(boost::asio::io_context& eventloop, const std::string &address, unsigned int port, const std::string& name, bool debug)
			: m_address(address)
			, m_port(port)
			, m_name(name)
			, m_debug(debug)
			, m_eventLoop(eventloop)
			, m_stopped(false)
			, m_lengthBufferLevel(0)
			, m_reader(rBuilder.newCharReader())
		{
			// Compose json without indentation. This saves lots of bandwidth and time!
			wBuilder.settings_["indentation"] = "";

			if (m_port!=0) {
				// connect via tcp
				std::string portString(std::to_string(m_port));
				m_stream = std::make_unique<daq::stream::TcpClientStream>(m_eventLoop, m_address, std::to_string(m_port));
			} else {
				m_stream = std::make_unique<daq::stream::LocalClientStream>(m_eventLoop, m_address);
			}
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
			boost::system::error_code retVal = m_stream->init(); // m_socket.connect(m_address, portString);
			if (retVal) {
				std::string msg;
				msg = "jet peerAsync could not connect to jetd " + m_stream->remoteHost() + ")!";
				throw std::runtime_error(msg);
			}

			m_stream->asyncRead(std::bind(&PeerAsync::onSizeReceive, this, std::placeholders::_1), sizeof(uint32_t));
			//m_socket.setDataCb(std::bind(&PeerAsync::receive, this));

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
			m_stream->close();

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

		void PeerAsync::onSizeReceive(const boost::system::error_code &ec)
		{
			if (ec) {
				syslog(LOG_ERR, "Error %s when receiving size", ec.message().c_str());
			}

			int32_t bigEndianSize;
			m_stream->copyDataAndConsume(&bigEndianSize, sizeof(bigEndianSize));
			m_payloadSize = ntohl(bigEndianSize);

			if (m_payloadSize>MAX_MESSAGE_SIZE) {
				syslog(LOG_ERR, "jet peer %s:%u: Received message size (%zu) exceeds maximum message size (%zu). Closing connection!", m_address.c_str(), m_port, m_payloadSize, MAX_MESSAGE_SIZE);
				stop();
				return;
			}

			m_stream->asyncRead(std::bind(&PeerAsync::onPayloadReceive, this, std::placeholders::_1), m_payloadSize);
		}

		void PeerAsync::onPayloadReceive(const boost::system::error_code &ec)
		{
			if (ec) {
				syslog(LOG_ERR, "Error %s when receiving payload", ec.message().c_str());
			}

			Json::Value dataJson;
			const char* pData = reinterpret_cast<const char*>(m_stream->data());

			/// \note Need to do this this in order to have termination '\0' added
			std::string bla(pData, m_payloadSize);
			if (m_reader->parse(bla.c_str(), bla.c_str()+m_payloadSize, &dataJson, &parseErrors)) {
			//if (m_reader->parse(pData, pData+m_payloadSize, &dataJson, &parseErrors)) {
				receiveCallback(dataJson);
			} else {
				syslog(LOG_ERR, "jet peer %s:%u: Error '%s' while parsing received telegram (%zu byte)", m_address.c_str(), m_port, parseErrors.c_str(), m_payloadSize);
			}

			m_stream->consume(m_payloadSize);
			m_stream->asyncRead(std::bind(&PeerAsync::onSizeReceive, this, std::placeholders::_1), sizeof(uint32_t));
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
			std::string msg = Json::writeString(wBuilder, value);
			size_t len = msg.length();
			if (len>MAX_MESSAGE_SIZE) {
				std::string errorMsg;
				errorMsg = "Message size " + std::to_string(len) + " exceeds maximum message size (" + std::to_string(MAX_MESSAGE_SIZE) + ") and will not be send!";
				syslog(LOG_ERR, "%s", errorMsg.c_str());
				throw hbk::exception::jsonrpcException(-1, errorMsg);
			}
			uint32_t lenBig = htonl(static_cast < uint32_t > (len));

			boost::system::error_code ec;
			daq::stream::ConstBufferVector buffers;
			buffers.push_back(boost::asio::const_buffer(&lenBig, sizeof(lenBig)));
			buffers.push_back(boost::asio::const_buffer(msg.c_str(), msg.length()));

			m_stream->write(buffers, ec);
			if (ec) {
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
				const auto iter = m_fetchers.find(fetchId);
				if (iter!=m_fetchers.cend()) {
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
						const auto iter = m_stateCallbacks.find(method);
						if (iter != m_stateCallbacks.cend()) {
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
						if (iter != m_methodCallbacks.cend()) {
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
