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

#include <stdexcept>
#include <functional>
#include <thread>
#include <chrono>

#include "hbk/sys/eventloop.h"

#include "jet/defines.h"
#include "jet/peer.hpp"
#include "jet/peerasync.hpp"

#include "asyncrequest.h"
#include "syncrequest.h"

/// otherwise unix domain socket is used which is faster but is not supported under windows and requires cjet 1.3
/// under windows, jet peer will always use tcp
//#define USE_TCP

namespace hbk
{
	namespace jet
	{

		hbk::jet::Peer& Peer::local()
		{
#if defined(_WIN32) || defined(USE_TCP)
			// windows does not support unix domain sockets
			static Peer _instance("127.0.0.1", JETD_TCP_PORT);
#else
			static Peer _instance(JET_UNIX_DOMAIN_SOCKET_NAME, 0);
#endif
			return _instance;
		}

		Peer::Peer(const std::string &address, unsigned int port, const std::string& name, bool debug)
			: m_peerAsync(m_eventloop, address, port, name, debug)
		{
			m_workerThread = std::thread(std::bind(&sys::EventLoop::execute, std::ref(m_eventloop)));
		}

		Peer::~Peer()
		{
			m_eventloop.stop();
			try {
				m_workerThread.join();
			} catch (const std::system_error&) {
				// ignore
			}
		}

		bool Peer::resume()
		{
			return m_peerAsync.resume();
		}

		Json::Value Peer::authenticate(const std::string& user, const std::string& password)
		{
			Json::Value params;

			params[USER] = user;
			params[PASSWORD] = password;

			SyncRequest method(AUTHENTICATE, params);
			Json::Value result = method.executeSync(m_peerAsync);

			if(result.isMember(jsonrpc::ERR)) {
				throw jsoncpprpcException(result);
			}

			return result[jsonrpc::RESULT];
		}

		JsonRpcResponseObject Peer::info()
		{
			Json::Value result;
			SyncRequest method(INFO, Json::Value());

			result = method.executeSync(m_peerAsync);
			return result;
		}

		JsonRpcResponseObject Peer::config(const std::string& name, bool debug)
		{
			Json::Value params;
			Json::Value result;
			params[NAME] = name;
			params[DBG] = debug;
			SyncRequest method(CONFIG, params);

			result = method.executeSync(m_peerAsync);
			return result;
		}

		JsonRpcResponseObject Peer::callMethod(const std::string& path, const Json::Value& args)
		{
			Json::Value params;
			return callMethodPrivate(path, args, params);
		}

		JsonRpcResponseObject Peer::callMethod(const std::string& path, const Json::Value& args, double timeout_s)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;

			return callMethodPrivate(path, args, params);
		}

		JsonRpcResponseObject Peer::callMethodPrivate(const std::string& path, const Json::Value& args, Json::Value& params)
		{
			params[PATH] = path;

			if (!args.isNull()) {
				params[ARGS] = args;
			}

			SyncRequest method(CALL, params);
			Json::Value result = method.executeSync(m_peerAsync);

			if(result.isMember(jsonrpc::ERR)) {
				throw jsoncpprpcException(result);
			}

			return result[jsonrpc::RESULT];
		}

		void Peer::addMethod(const std::string& path, methodCallback_t callback)
		{
			Json::Value params;
			addMethodPrivate(path, params, callback);
		}

		void Peer::addMethod(const std::string& path, double timeout_s, methodCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;

			addMethodPrivate(path, params, callback);
		}

		void Peer::addMethod(const std::string& path, const userGroups_t& fetchGroups,
		                     const userGroups_t& callGroups, methodCallback_t callback,
		                     double timeout_s)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			if (!fetchGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string &it: fetchGroups) {
					groups.append(it);
				}

				params[ACCESS][FETCH_GROUPS] = groups;
			}

			if (!callGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string &it: callGroups) {
					groups.append(it);
				}

				params[ACCESS][CALL_GROUPS] = groups;
			}

			addMethodPrivate(path, params, callback);
		}

		void Peer::addMethodPrivate(const std::string& path, Json::Value& params, methodCallback_t callback)
		{
			params[PATH] = path;

			m_peerAsync.registerMethod(path, callback);
			SyncRequest request(ADD, params);

			Json::Value retVal = request.executeSync(m_peerAsync);

			if(retVal.isMember(jsonrpc::ERR)) {
				m_peerAsync.unregisterMethod(path);
#ifdef _WIN32
				throw jsoncpprpcException(-1, __FUNCTION__, retVal);
#else
				throw jsoncpprpcException(-1, __PRETTY_FUNCTION__, retVal);
#endif
			}
		}

		void Peer::addMethodAsync(const std::string& path, const userGroups_t& fetchGroups, const userGroups_t& callGroups,
			methodCallback_t callback, double timeout_s, responseCallback_t resultCallback)
		{
			m_peerAsync.addMethodAsync(path, fetchGroups, callGroups, callback, timeout_s, resultCallback);
		}

		void Peer::addState(const std::string& path, const Json::Value& value, stateCallback_t callback)
		{
			Json::Value params;
			addStatePrivate(path, value, params, callback);
		}

		void Peer::addState(const std::string& path, const Json::Value& value, double timeout_s, stateCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;

			addStatePrivate(path, value, params, callback);
		}

		void Peer::addState(const std::string& path, const userGroups_t& fetchGroups,
		                    const userGroups_t& setGroups, const Json::Value& value,
		                    double timeout_s, stateCallback_t callback)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			if (!fetchGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string &it: fetchGroups) {
					groups.append(it);
				}

				params[ACCESS][FETCH_GROUPS] = groups;
			}

			if (!setGroups.empty()) {
				Json::Value groups = Json::Value(Json::arrayValue);
				for (const std::string &it: setGroups) {
					groups.append(it);
				}

				params[ACCESS][SET_GROUPS] = groups;
			}

			addStatePrivate(path, value, params, callback);
		}

		void Peer::addStatePrivate(const std::string& path, const Json::Value& value, Json::Value& params, stateCallback_t callback)
		{
			params[PATH] = path;
			params[VALUE] = value;

			m_peerAsync.registerState(path, callback);
			SyncRequest method(ADD, params);
			Json::Value retVal = method.executeSync(m_peerAsync);

			if (retVal.isMember(jsonrpc::ERR)) {
				m_peerAsync.unregisterState(path);
				throw jsoncpprpcException(retVal);
			}
		}

		/// @param resultCallback called on completion providing the result
		/// @param callback function to be called when state is set via jet. leave empty for read only states
		/// \throws hbk::exception::jsonrpcException on error
		void Peer::addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback, stateCallback_t callback)
		{
			m_peerAsync.addStateAsync(path, value, resultCallback, callback);
		}

		void Peer::addStateAsync(const std::string& path, const Json::Value& value, double timeout_s, responseCallback_t resultCallback, stateCallback_t callback)
		{
			m_peerAsync.addStateAsync(path, value, timeout_s, resultCallback, callback);
		}

		void Peer::addStateAsync(const std::string& path, const Json::Value& value,
		                         const userGroups_t& fetchGroups, const userGroups_t& setGroups,
		                         double timeout_s, responseCallback_t resultCallback,
		                         stateCallback_t callback)
		{
			m_peerAsync.addStateAsync(path, fetchGroups, setGroups, value, timeout_s, resultCallback, callback);
		}

		void Peer::removeFetchAsync(fetchId_t fetchId, responseCallback_t resultCb)
		{
			m_peerAsync.removeFetchAsync(fetchId, resultCb);
		}

		/// the peer serves a new method on jet
		/// \throws hbk::exception::jsonrpcException on error
		void Peer::removeMethodAsync(const std::string& path, responseCallback_t resultCallback)
		{
			m_peerAsync.removeMethodAsync(path, resultCallback);
		}

		/// @param resultCb called when operation finished, nullptr if we are not interested in the response. This results in sending a notification instead of an request (no id).
		void Peer::removeStateAsync(const std::string& path, responseCallback_t resultCb)
		{
			m_peerAsync.removeStateAsync(path, resultCb);
		}

		Json::Value Peer::get(const matcher_t& match)
		{
			Json::Value params;
			PeerAsync::addPathInformation(params, match);

			SyncRequest request(GET, params);
			return request.executeSync(m_peerAsync);
		}

		fetchId_t Peer::addFetch(const matcher_t &match, fetchCallback_t callback)
		{
			fetchId_t fetchId = PeerAsync::createFetchId();

			Json::Value params;
			params[jsonrpc::ID] = fetchId;
			PeerAsync::addPathInformation(params, match);

			m_peerAsync.registerFetch(fetchId, fetcher_t(callback, match));
			SyncRequest method(FETCH, params);

			Json::Value result = method.executeSync(m_peerAsync);
			if(result.isMember(jsonrpc::ERR)) {
					m_peerAsync.unregisterFetch(fetchId);
				throw jsoncpprpcException(result);
			}

			return fetchId;
		}

		SetStateResult Peer::setStateValue(const std::string& path, const Json::Value& value)
		{
			Json::Value params;
			return setStateValuePrivate(path, value, params);
		}

		SetStateResult Peer::setStateValue(const std::string& path, const Json::Value& value, double timeout_s)
		{
			Json::Value params;
			params[TIMEOUT] = timeout_s;
			return setStateValuePrivate(path, value, params);
		}

		SetStateResult Peer::setStateValuePrivate(const std::string& path, const Json::Value& value, Json::Value& params)
		{
			SetStateResult warning;
			params[PATH] = path;
			params[VALUE] = value;

			SyncRequest method(SET, params);

			Json::Value result = method.executeSync(m_peerAsync);
			if(result.isMember(hbk::jsonrpc::ERR)) {
//                std::cout << result.toStyledString() << std::endl;
				throw jsoncpprpcException(result);
			} else {
				Json::Value &resultNode = result[hbk::jsonrpc::RESULT];
				if (resultNode[WARNING][hbk::jsonrpc::CODE].isInt()) {
					warning.code = static_cast<WarningCode>(resultNode[WARNING][hbk::jsonrpc::CODE].asInt());
				}
			}
			return warning;
		}
	}
}
