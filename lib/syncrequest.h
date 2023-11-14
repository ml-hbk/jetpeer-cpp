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


#ifndef __HBK_JET_SYNCREQUEST_H
#define __HBK_JET_SYNCREQUEST_H

#include <future>

#include <json/value.h>

#include "asyncrequest.h"

namespace hbk
{
	namespace jet
	{
		/// a request handled synchronuously. method executeSync sends the request and waits for the response to arrive
		class SyncRequest: public AsyncRequest
		{
		public:
			/// A request id is generated automatically
			/// \param name Name of the state or method
			/// \param params All parameters to be sned to the jet daemon
			SyncRequest(const char *pName, const Json::Value& params);
			SyncRequest(const SyncRequest&&) = delete;
			SyncRequest operator = (const SyncRequest) = delete;

			virtual ~SyncRequest();

			/// send the request and wait for result
			/// \return Result or error object
			Json::Value executeSync(PeerAsync& peerAsync);

		private:
			std::promise < Json::Value > m_result;
		};
	} // namespace jet
} // namespace hbk
#endif
