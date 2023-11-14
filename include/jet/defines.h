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

#ifndef _HBK__JET__DEFINES_H
#define _HBK__JET__DEFINES_H
#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "json/value.h"
#include "hbk/exception/jsonrpc_exception.h"
namespace hbk {
	namespace jet {
		const unsigned int JETD_TCP_PORT = 11122;
		const unsigned int JETWS_TCP_PORT = 11123;
		const char JET_UNIX_DOMAIN_SOCKET_NAME[] = "/var/run/jet.socket";

		static const char NAME[] = "name";
		static const char DBG[] = "debug";

		static const char PATH[] = "path";
		static const char ARGS[] = "args";
		static const char VALUE[] = "value";
		static const char TIMEOUT[] = "timeout";
		static const char FETCHONLY[] = "fetchOnly";
		static const char VALUEASRESULT[] = "valueAsResult";

		static const char EVENT[] = "event";

		// matchers
		static const char CONTAINS[] = "contains";
		static const char STARTSWITH[] = "startsWith";
		static const char ENDSWITH[] = "endsWith";
		static const char EQUALS[] = "equals";
		static const char EQUALSNOT[] = "equalsNot";
		static const char CONTAINSALLOF[] = "containsAllOf";
		static const char CASEINSENSITIVE[] = "caseInsensitive";

		// request types
		static const char ADD[] = "add";
		static const char REMOVE[] = "remove";
		static const char FETCH[] = "fetch";
		static const char UNFETCH[] = "unfetch";
		static const char CALL[] = "call";
		static const char SET[] = "set";
		static const char GET[] = "get";

		static const char CONFIG[] = "config";
		static const char INFO[] = "info";
		static const char AUTHENTICATE[] = "authenticate";

		/// change notification form jet peer owning a state to the jet daemon
		static const char CHANGE[] = "change";
		static const char WARNING[] = "warning";
		
		enum WarningCode {
			/// Will never appear in json. instead, there will be no warning object at all.
			WARN_NONE = 0,
			/// A request suceeded but got adapted
			WARN_ADAPTED = 1,
		};


		// request parameters
		static const char USER[] = "user";
		static const char PASSWORD[] = "password";

		static const char ACCESS[] = "access";
		static const char FETCH_GROUPS[] = "fetchGroups";
		static const char SET_GROUPS[] = "setGroups";
		static const char CALL_GROUPS[] = "callGroups";
		/// Maximum length of a single jet messages or batched jet messages supported by this peer implementation
		static const size_t MAX_MESSAGE_SIZE = 262144;

		/// In case of success:
		/// ```
		/// {
		///		"result" : {}
		/// }
		/// ```
		/// 
		/// In case of error:
		/// ```
		/// {
		///		"error" : {
		///			"code" : <number>,
		///			"message": <string>,
		///			"data" : {}
		///		}
		/// }
		/// ```
		/// `data` is optional
		/// 
		/// 
		/// Please [https://www.jsonrpc.org/specification](https://www.jsonrpc.org/specification) see for details
		using JsonRpcResponseObject = Json::Value;
		
		using fetchId_t = int;

		/// @param notification contains PATH and EVENT as key-value-pairs and VALUE as an object
		/// ```
		/// {
		///		"path" : <path of state notifying state>,
		///   "event" : "add"|"change"|"remove",
		///   "value" : <the new value being notified>
		/// }
		/// ```
		/// @param status < 0 if something really bad like loss of connection happened!
		using fetchCallback_t = std::function < void ( const Json::Value& notification, int status ) >;

		/// callback method processing the request for a registered jet method
		/// \return the result of the function. It will be delivered to the requesting jet peer in form of an jsonrpc response object.
		/// \throws hbk::Exception::jsonException on error. It will be delivered to the requesting jet peer in form of an jsonrpc error object.
		using methodCallback_t = std::function < Json::Value (const Json::Value&) >;
		
		class SetStateResult {
		public:
			SetStateResult()
				: code(WARN_NONE)
			{
			}
			SetStateResult(WarningCode theCode)
				: code(theCode)
			{
			}
			SetStateResult(WarningCode theCode, const std::string& theMessage)
				: code(theCode)
				, message(theMessage)
			{
			}
			enum WarningCode code; /// the mandatory code.
			std::string message; /// optional decribing text
		};

		/// Returned by state callback methods
		class SetStateCbResult {
		public:
			SetStateCbResult()
			{
			}
			SetStateCbResult(const Json::Value& theValue)
				: value(theValue)
			{
			}
			SetStateCbResult(const Json::Value& theValue, WarningCode theCode)
				: value(theValue)
				, result(theCode)
			{
			}
			SetStateCbResult(const Json::Value& theValue, WarningCode theCode, const std::string& theMessage)
				: value(theValue)
				, result(theCode, theMessage)
			{
			}
			Json::Value value;
			SetStateResult result;
		};
		
		/// \param value The requested value for the state to be set
		/// \param path The path of the state that is to be set
		/// \return Json::nullValue if the state was not changed by the
		/// state callback method or the Json::Value representing the
		/// new state value.
		/// \note Please note that returning Json::nullValue signals the peer library to NOT emit a CHANGE notification.
		/// \throws hbk::Exception::jsonException on error
		using stateCallback_t = std::function < SetStateCbResult (const Json::Value& value, const std::string& path) >;

		/// Used for asynchronous execution of requests.
		/// Asynchronuous requests without a response callback will be send without an id. Hence the jetd won't send a response.
		/// \param result a json rpc response object
		/// On success it looks like this:
		/// \code
		/// {
		///		"id" : <the request id>,
		///		"result" : {}
		/// }
		/// \endcode
		/// If the request was "almost" succesfull, a warning is issued. 
		/// This happens for example when values from the request got adapted:
		/// \code
		/// {
		///		"id" : <the request id>,
		///		"result" : {
		///			"warning" : {
		///				"code" : <mandatory, enum WarningCode i.e. WARN_ADAPTED = 1>,
		///				"message" : <optional, string>
		///			}
		///		}
		/// }
		/// \endcode
		/// In case of an error:
		/// \code
		/// {
		///		"id" : <the request id>,
		///		"error" : {
		///			"code" : <mandatory, error code as integer>,
		///			"message" : <optional, string>
		///		}
		/// }
		/// \endcode
		using responseCallback_t = std::function < void ( const JsonRpcResponseObject& result) >;

		using userGroups_t = std::list < std::string > ;

		/// For describing the match rules for fetchers.
		/// All rules are AND gated!
		struct matcher_t {
			matcher_t()
				: caseInsensitive(false)
			{
			}
			bool caseInsensitive;
			/// fetch does match if path contains
			std::string contains;
			/// fetch does match if path starts with
			std::string startsWith;
			/// fetch does match if path ends with
			std::string endsWith;
			/// fetch does match if path equals
			std::string equals;
			/// fetch does match if path does not equal
			std::string equalsNot;
			/// fetch does match if path contains all of those
			std::vector < std::string > containsAllOf;

			/// return string describing all fetch conditions
			std::string print()
			{
				std::string msg;

				if (caseInsensitive) {
					msg += CASEINSENSITIVE;
				}

				if (contains.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(CONTAINS)+"="+contains;
				}

				if (startsWith.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(STARTSWITH)+"="+startsWith;
				}

				if (endsWith.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(ENDSWITH)+"="+endsWith;
				}

				if (equals.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(EQUALS)+"="+equals;
				}

				if (equalsNot.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(EQUALSNOT)+"="+equalsNot;
				}

				if (containsAllOf.empty()==false) {
					if (msg.empty()==false) {
						msg += ", ";
					}
					msg += std::string(CONTAINSALLOF)+"=[";
					for (const auto& iter : containsAllOf) {
						msg += iter + ", ";
					}
					msg.pop_back();
					msg.pop_back();
					msg += "]";
				}

				return msg;
			}
		};

		struct fetcher_t {
			fetcher_t();
			/// @param cb Callback to be called on notification
			/// @param m Matching conditions
			fetcher_t(fetchCallback_t cb, matcher_t m);
			fetchCallback_t callback;
			matcher_t matcher;
		};


		/// gives the complete error object including the data object
		/// Data object may include detailed error information for example if setting elements of a complex state failed.
		class jsoncpprpcException : public hbk::exception::jsonrpcException
		{
		public:
			struct DataEntry {
				int code;
				std::string message;
				bool operator==(const DataEntry& other) const {
					return ((code == other.code) && (message == other.message));
				}
			};
			/// item name is the key
			using DataEntries = std::unordered_map < std::string, DataEntry >;

			jsoncpprpcException(const Json::Value& error) noexcept;
			jsoncpprpcException(int code, const std::string& message) noexcept;
			jsoncpprpcException(int code, const std::string& message, const Json::Value& data) noexcept;

			/// Creates an error object with nested details
			/// \code
			/// {
			///   "jsonrpc": "2.0",
			///   "error":
			///   {
			///     "code": -32600,
			///     "message": "Invalid Request",
			///     "data" : {
			///       {
			///         "par1" {
			///           "code" : <error codes as integer>,
			///           ["message" : "<optional error message as string>"]
			///         },
			///         "par2" {
			///           "code" : <error codes as integer>,
			///           ["message" : "<optional error message as string>"]
			///         }
			///       {
			///     }
			///   }
			/// }
			/// \endcode
			jsoncpprpcException(DataEntries &data) noexcept;
			~jsoncpprpcException() = default;

			Json::Value json() const noexcept;
			int code() const noexcept;
			const char* message() const noexcept;
			const Json::Value& data() const noexcept;
			DataEntries dataEntries() const noexcept;

			const char* what() const noexcept override;

		private:
			std::string m_what;
			Json::Value m_data_obj;
		};



		/// thrown if the parameter number is not suitable for the mehtod to call
		class wrongParameterNumberException : public hbk::exception::jsonrpcException
		{
		public:
			wrongParameterNumberException()
				: hbk::exception::jsonrpcException(-1, "wrong number of parameters for method")
			{
			}
			virtual ~wrongParameterNumberException() noexcept = default;
		};

		/// thrown if an expected parameter is missing in the request
		class missingParameterException : public hbk::exception::jsonrpcException
		{
		public:
			missingParameterException(const std::string& name)
				: hbk::exception::jsonrpcException(-1, std::string("missing parameter '")+name+"' for method")
			{
			}

			virtual ~missingParameterException() noexcept = default;
		};
	}
}
#endif
