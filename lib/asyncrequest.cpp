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

#include <mutex>

#ifdef _WIN32
#define syslog fprintf
#define LOG_ERR stderr
#else
#include "syslog.h"
#endif

#include <json/writer.h>

#include "asyncrequest.h"
#include "jet/defines.h"
#include "hbk/exception/jsonrpc_exception.h"
#include "hbk/jsonrpc/jsonrpc_defines.h"


namespace hbk
{
	namespace jet
	{
		unsigned int AsyncRequest::m_sid = 0;
		AsyncRequest::openRequests_t AsyncRequest::m_openRequestCbs;
		std::mutex AsyncRequest::m_mtx_openRequestCbs;

		AsyncRequest::AsyncRequest(const char *pName, const Json::Value& params)
			: m_id(0)
		{
			m_requestDoc[jsonrpc::JSONRPC] = "2.0";
			m_requestDoc[jsonrpc::METHOD] = pName;
			m_requestDoc[jsonrpc::PARAMS] = params;
		}

		void AsyncRequest::execute(PeerAsync& peerAsync, const responseCallback_t& resultCb)
		{
			if (resultCb) {
				// we do not expect an answer when there is no result callback to be called
				{
					std::lock_guard < std::mutex > local(m_mtx_openRequestCbs);
					m_id = ++m_sid;
					m_openRequestCbs[m_id].responseCallback = resultCb;
				}
				m_requestDoc[jsonrpc::ID] = m_id;
			}

			try {
				peerAsync.sendMessage(m_requestDoc);
			} catch (const hbk::exception::jsonrpcException& e) {
				// "instant" error response call back is not to be called int this context but in eventloop context 
				// that handles all responmse callback functions.
				// To achive this, we use a notifier with a callback that is executed in the eventloop context.
				if (resultCb) {
					auto NotifierCb = [this, e]()
					{
						Json::Value response;
						response[jsonrpc::ID] = m_id;
						response[jsonrpc::ERR][jsonrpc::CODE] = e.code();
						response[jsonrpc::ERR][jsonrpc::MESSAGE] = e.message();
						handleResult(response);
					};
					peerAsync.getEventLoop().dispatch(NotifierCb);
				}
			}
		}

		void AsyncRequest::execute(PeerAsync& peerAsync)
		{
			try {
				peerAsync.sendMessage(m_requestDoc);
			}  catch (...) {
				// ignore!
			}
		}
		
		void AsyncRequest::handleResult(const Json::Value& data)
		{
			unsigned int id = data[jsonrpc::ID].asUInt();
			responseCallback_t responseCallback;
			{
				std::lock_guard < std::mutex > lock(m_mtx_openRequestCbs);
				const auto iter = m_openRequestCbs.find(id);
				if (iter == m_openRequestCbs.cend()) {
					syslog(LOG_ERR, "jet peer: No request with id='%u' is waiting for a response!", id);
					return;
				}
				responseCallback = iter->second.responseCallback;
				m_openRequestCbs.erase(iter);
			}
			try {
				responseCallback(data);
			} catch(...) {
				// catch and ignoreeverything!
			}
		}

		size_t AsyncRequest::clear()
		{
			size_t count;
			std::lock_guard < std::mutex > lock(m_mtx_openRequestCbs);

			count = m_openRequestCbs.size();
			for (auto &iter: m_openRequestCbs) {
				responseCallback_t resopnseCallback = iter.second.responseCallback;
				if (resopnseCallback) {
					Json::Value error;
					error[jsonrpc::ID] = iter.first;
					error[jsonrpc::ERR][jsonrpc::CODE] = -1;
					error[jsonrpc::ERR][jsonrpc::MESSAGE] = "jet request has been canceled without response!";
					try {
						resopnseCallback(error);
					} catch(...) {
						// catch and ignoreeverything!
					}
				}
			}
			m_openRequestCbs.clear();
			return count;
		}
	}
}
