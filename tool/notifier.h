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
#ifndef __HBK_JET_NOTIFIER
#define __HBK_JET_NOTIFIER

#include <functional>

#include "json/value.h"
#include "jet/defines.h"
#include "jet/peerasync.hpp"

namespace hbk {
namespace jet {
namespace tool {
/// notifies all changes of states matching to the given matcher
/// For every possible event type there is a separate callback function
/// - add: a new state or method apperared
/// - update: value of a state changed
/// - remove: a state or method disappeared
class notifier
{
public:
	using cb_t = std::function<void(const std::string& path, const Json::Value& value)>;

	notifier(hbk::jet::PeerAsync& peer);
	virtual ~notifier();

	/// set specific callback to cb_t() if there is no callback to be called!
	void start(cb_t addCb, cb_t changeCb, cb_t removeCb, const hbk::jet::matcher_t& match);
	void stop();

private:
	void fetchCb(const Json::Value& params, int status);

	hbk::jet::PeerAsync& m_peer;
	hbk::jet::fetchId_t m_fetchId;
	cb_t m_addCb;
	cb_t m_changeCb;
	cb_t m_removeCb;
};
}
}
}
#endif
