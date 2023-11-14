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

#include <future>

#include "syncrequest.h"

namespace hbk
{
	namespace jet
	{
		SyncRequest::SyncRequest(const char* pName, const Json::Value& params)
			: AsyncRequest(pName, params)
		{
		}

		SyncRequest::~SyncRequest()
		{
			// destruction in destructor of base class might be to late.
			// before calling the base desctructor the object is already missing the parts from the specialized class and might be used by another thread!
			std::lock_guard < std::mutex > local(m_mtx_openRequestCbs);
			m_openRequestCbs.erase(m_id);
		}

		Json::Value SyncRequest::executeSync(PeerAsync& peerAsync)
		{
			auto f = m_result.get_future();

			auto lambda = [this](const Json::Value& result) {
				m_result.set_value(result);
			};
			execute(peerAsync, lambda);
			return f.get();
		}
	}
}
