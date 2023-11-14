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

#include <json/value.h>
#include <json/writer.h>

#include "hbk/jsonrpc/jsonrpc_defines.h"
#include "hbk/exception/jsonrpc_exception.h"

#include "jet/defines.h"

static Json::StreamWriterBuilder wBuilder;

namespace hbk {
	namespace jet {
		jsoncpprpcException::jsoncpprpcException(const Json::Value& error) noexcept
			: jsonrpcException(error[jsonrpc::ERR][jsonrpc::CODE].asInt(), error[jsonrpc::ERR][jsonrpc::MESSAGE].asString())
			, m_data_obj(error[jsonrpc::ERR][jsonrpc::DATA])
		{
			m_what = "code: " + std::to_string(m_code) + ", message: " + m_message;
			if (!m_data_obj.isNull()) {
				m_what += ", data: " + m_data_obj.toStyledString();
			}
		}

		jsoncpprpcException::jsoncpprpcException(int code, const std::string& message) noexcept
			: jsonrpcException(code, message)
		{
			m_what = "code: " + std::to_string(code) + ", message: " + message;
		}

		jsoncpprpcException::jsoncpprpcException(int code, const std::string& message, const Json::Value& data) noexcept
			: jsonrpcException(code, message)
		{
			m_data_obj = data;
			m_what = "code: " + std::to_string(m_code) + ", message: " + m_message;
			if (!m_data_obj.isNull()) {
				m_what += ", data: " + m_data_obj.toStyledString();
			}

		}

		jsoncpprpcException::jsoncpprpcException(DataEntries& data) noexcept
			: jsonrpcException(-1, "see data object for details")
		{
			for (const auto & iter: data) {
				const DataEntry& entry = iter.second;
				Json::Value item;
				item[jsonrpc::CODE] = entry.code;
				item[jsonrpc::MESSAGE] = entry.message;
				m_data_obj[iter.first] = item;
			}
		}

		Json::Value jsoncpprpcException::json() const noexcept
		{
			Json::Value errorObject;
			errorObject[jsonrpc::ERR][jsonrpc::CODE] = m_code;
			errorObject[jsonrpc::ERR][jsonrpc::MESSAGE] = m_message;
			errorObject[jsonrpc::ERR][jsonrpc::DATA] = m_data_obj;
			return errorObject;
		}

		int jsoncpprpcException::code() const noexcept
		{
			return m_code;
		}

		const char* jsoncpprpcException::message() const noexcept
		{
			return m_message.c_str();
		}

		const char* jsoncpprpcException::what() const noexcept
		{
			return m_what.c_str();
		}

		const Json::Value& jsoncpprpcException::data() const noexcept
		{
			return m_data_obj;
		}

		jsoncpprpcException::DataEntries jsoncpprpcException::dataEntries() const noexcept
		{
			DataEntries dataEntries;
			for (Json::ValueConstIterator iter = m_data_obj.begin(); iter!=m_data_obj.end(); ++iter) {
				const Json::Value entryNode = *iter;
				DataEntry dataEntry;
				dataEntry.code = entryNode[jsonrpc::CODE].asInt();
				dataEntry.message = entryNode[jsonrpc::MESSAGE].asString();
				dataEntries[iter.key().asString()] = dataEntry;
			}
			return dataEntries;
		}

	}
}
