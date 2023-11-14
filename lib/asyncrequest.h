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

#ifndef __HBK_JET_ASYNCREQUEST_H
#define __HBK_JET_ASYNCREQUEST_H

#include <unordered_map>
#include <mutex>

#include <json/value.h>

#include "hbk/sys/notifier.h"

#include "jet/peerasync.hpp"
#include "jet/defines.h"

namespace hbk
{
	namespace jet
	{
		/// a request handled asynchronuously. method exceute sends the callback provided to the conrstructor is executed on completion.
		class AsyncRequest
		{
		public:


			/// A request id is not added automatically. without an id the won't be a response from the jet daemon
			/// \param name Name of the state or method
			/// \param params All parameters to be sned to the jet daemon
			AsyncRequest(const char* pName, const Json::Value& params);
			AsyncRequest(const AsyncRequest&) = delete;

			virtual ~AsyncRequest() = default;

			AsyncRequest& operator=(const AsyncRequest&) = delete;

			/// send the request does not wait for result
			/// the result callback is being kept until the result arrives.
			void execute(PeerAsync &peerAsync, const responseCallback_t& resultCb);
			/// Send the request. The is no result callback method. Hence no jsonrpc id is being send and no json rpc response will return
			void execute(PeerAsync& peerAsync);

			/// find the request this reply belongs to!
			static void handleResult(const Json::Value& params);

			/// Clear all open requests. Responses for those won't be recognized afterwards!
			/// All request callbacks will be called with an error object stating that the request was canceled without response!
			/// \return number of requests removed
			static size_t clear();

		protected:
			struct Request {
				responseCallback_t responseCallback;
				/// Since creating a notifier involves system calls, we do this only if needed.
				std::unique_ptr<hbk::sys::Notifier> errorNotifier;
			};
			/// id of the request is the key
			using openRequests_t = std::unordered_map < unsigned int, Request >;
			
			unsigned int m_id;
			Json::Value m_requestDoc;

			static unsigned int m_sid;

			static openRequests_t m_openRequestCbs;
			static std::mutex m_mtx_openRequestCbs;
		private:
		};
	}
}
#endif
