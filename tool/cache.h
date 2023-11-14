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
#ifndef __HBK_JET_CACHE
#define __HBK_JET_CACHE

#include <functional>
#include <mutex>
#include <unordered_map>

#include "json/value.h"
#include "jet/peerasync.hpp"

namespace hbk {
	namespace jet {
		/// fetches and keeps everything matching to the given matcher. Changes may be notified by callback functions.
		/// class is thread-safe
		class cache {
		public:
			/// \param path Path of the state
			/// \param value Value of the state
			using Cb = std::function < void ( const std::string& path, const Json::Value& value) >;

			cache(hbk::jet::PeerAsync& peer, hbk::jet::matcher_t match);
			cache(const cache&) = delete;
			virtual ~cache();

			/// set specific callback to cb_t() if there is no callback to be called!
			void setCbs(Cb addCb, Cb changeCb, Cb removeCb);

			/// \return an empty object if no entry with this path does exist
			Json::Value getEntry(const std::string &path) const;

		private:
			/// jet path is the key
			using Cache = std::unordered_map < std::string, Json::Value >;

			void fetchCb( const Json::Value& params, int status);

			hbk::jet::PeerAsync& m_peer;
			hbk::jet::matcher_t m_match;
			Cache m_cache;
			mutable std::mutex m_cacheMtx;
			hbk::jet::fetchId_t m_fetchId;
			Cb m_addCb;
			Cb m_changeCb;
			Cb m_removeCb;
		};
	}
}
#endif
