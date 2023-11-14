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
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <functional>
#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <json/value.h>
#include <json/writer.h>

#include "jet/defines.h"
#include "jet/peerasync.hpp"
#include "hbk/sys/eventloop.h"
#include "hbk/jsonrpc/jsonrpc_defines.h"

#ifndef _WIN32
#define USE_UNIX_DOMAIN_SOCKETS
#endif

namespace hbk::jet {



static void fetchcbCount(int& fetchCount)
{
	fetchCount += 1;
}

static void fetchCbCountDownOnChange(int& counter, std::promise <void> & p, const Json::Value& notification)
{
	if (notification[hbk::jet::EVENT]!=hbk::jet::CHANGE) {
		return;
	}
	--counter;
	if (counter==0) {
		p.set_value();
	}
}


static SetStateCbResult cbState( const Json::Value& value, const std::string&, Json::Value* pResult)
{
	(*pResult) = value;
	SetStateCbResult result(value);
	return result;
}

static void cbAsyncBoolResult( const Json::Value& result, std::promise < bool >& success)
{
	if(result.isMember(hbk::jsonrpc::RESULT)) {
		success.set_value(true);
	} else if(result.isMember(hbk::jsonrpc::ERR)) {
		success.set_value(false);
	} else {
		success.set_exception(std::make_exception_ptr(std::runtime_error("invalid result object!")));
	}
}

static void cbAsyncJsonResult( const Json::Value& result, std::promise < Json::Value >& promise)
{
	promise.set_value(result);
}

class AsyncTest : public ::testing::Test {

protected:
	hbk::sys::EventLoop eventloop;

	hbk::jet::PeerAsync peer;
	// we promise to wait for the result before leaving!
	std::future <int > asy;

	hbk::jet::fetchId_t m_fetchId;

	AsyncTest()
#ifdef USE_UNIX_DOMAIN_SOCKETS
		: peer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "AsyncTest")
#else
		: peer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "AsyncTest")
#endif
	{
		asy = std::async(std::launch::async, &hbk::sys::EventLoop::execute, std::ref(eventloop));
	}

	virtual ~AsyncTest()
	{
		eventloop.stop();
		asy.wait();
	}

	virtual void SetUp()
	{
	}

	virtual void TearDown()
	{
	}
};


TEST_F(AsyncTest, testInfo)
{
	std::promise < Json::Value > promise;
	std::future < Json::Value > future = promise.get_future();

	// get information about the jet damon and retrieve some informaiotn
	peer.infoAsync(std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(promise)));

	std::future_status futureStatus = future.wait_for(std::chrono::milliseconds(1000));
	ASSERT_EQ(futureStatus, std::future_status::ready);

	Json::Value response = future.get();
	ASSERT_TRUE(response[hbk::jsonrpc::RESULT]["name"].isString());
	ASSERT_TRUE(response[hbk::jsonrpc::RESULT]["version"].isString());
}


TEST_F(AsyncTest, testConfig)
{
	std::promise < bool > successPromise;
	std::future < bool > successFuture = successPromise.get_future();

	peer.configAsync("testConfig", true, std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(successPromise)));

	std::future_status futureStatus = successFuture.wait_for(std::chrono::milliseconds(1000));
	ASSERT_EQ(futureStatus, std::future_status::ready);
	ASSERT_EQ(successFuture.get(), true);
}


TEST_F(AsyncTest, test_async_fetch)
{
	static const unsigned int bunchCount = 1000;

	{
		// create a whole bunch of states that are to be fetched. As soon the fetch is being registered, we expect to be informed about all states.

		hbk::jet::fetchId_t fetchBunch;
		std::vector < std::string > states;

		for (unsigned int stateIndex=0; stateIndex < bunchCount; ++stateIndex) {
			std::promise < bool > asyncResultPromise;
			std::future <bool > asyncResultFuture = asyncResultPromise.get_future();
			Json::Value result;
			std::string path;
			path = "test/bunch/member" + std::to_string(stateIndex);
			std::future_status futureStatus;
			peer.addStateAsync(path, Json::Value(),
							   std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)),
							   std::bind(&cbState, std::placeholders::_1, std::placeholders::_2, &result));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(), true);
			states.push_back(path);
		}

		unsigned int membersLeft = bunchCount;
		{
			std::promise < bool > asyncResultPromise;
			std::future < bool > asyncResultFuture = asyncResultPromise.get_future();
			std::future_status futureStatus;

			hbk::jet::matcher_t match;
			match.startsWith = "test/bunch";

			auto fetchcbDecrement = [&membersLeft](const Json::Value&, int)
			{
				membersLeft -= 1;
			};


			fetchBunch = peer.addFetchAsync(match,
											fetchcbDecrement,
											std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(), true);
			ASSERT_EQ(membersLeft, 0u);
		}

		peer.removeFetchAsync(fetchBunch);
	}

#ifdef USE_UNIX_DOMAIN_SOCKETS
		hbk::jet::PeerAsync fetchingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "fetchingPeer");
#else
		hbk::jet::Peer fetchingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "fetchingPeer");
#endif



	static const std::string state_zahl = "test/node/zahl";
	{
		std::future_status futureStatus;
		int fetchCount;
		{
			// nothing should match
			fetchCount = 0;
			std::promise < bool > asyncResultPromise;
			std::future <bool > asyncResultFuture = asyncResultPromise.get_future();

			hbk::jet::matcher_t match;
			match.equals = state_zahl;
			hbk::jet::fetchId_t fetchObj = fetchingPeer.addFetchAsync(match,
																				 std::bind(&fetchcbCount, std::ref(fetchCount)),
																				 std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(),true);
			ASSERT_EQ(fetchCount, 0);

			fetchingPeer.removeFetchAsync(fetchObj);
		}

		{
			// one should match
			fetchCount = 0;
			peer.addStateAsync(state_zahl, 6,
							  hbk::jet::responseCallback_t(),
							  hbk::jet::stateCallback_t());

			std::promise < bool > asyncResultPromise;
			std::future <bool > asyncResultFuture = asyncResultPromise.get_future();

			hbk::jet::matcher_t match;
			match.equals = state_zahl;
			std::string printedMatcher = match.print();
			ASSERT_EQ(printedMatcher, std::string(EQUALS) + "=" + state_zahl);
			hbk::jet::fetchId_t fetchObj = fetchingPeer.addFetchAsync(match,
																	 std::bind(&fetchcbCount, std::ref(fetchCount)),
																	 std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(), true);
			ASSERT_EQ(fetchCount, 1);

			fetchingPeer.removeFetchAsync(fetchObj);
		}

		{
			// all but one should match
			fetchCount = 0;
			peer.addStateAsync(state_zahl, 6,
							  hbk::jet::responseCallback_t(),
							  hbk::jet::stateCallback_t());

			std::promise < bool > asyncResultPromise;
			std::future < bool > asyncResultFuture = asyncResultPromise.get_future();

			hbk::jet::matcher_t match;
			match.startsWith = "test";
			match.equalsNot = state_zahl;
			std::string printedMatcher = match.print();
			std::string expected = std::string(STARTSWITH) + "=test, " + std::string(EQUALSNOT) + "=" + state_zahl;
			ASSERT_EQ(printedMatcher, expected);
			hbk::jet::fetchId_t fetchObj = fetchingPeer.addFetchAsync(match,
																	 std::bind(&fetchcbCount, std::ref(fetchCount)),
																	 std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(), true);
			ASSERT_EQ(fetchCount, bunchCount);

			fetchingPeer.removeFetchAsync(fetchObj);
		}

		{
			// all but one should match
			fetchCount = 0;
			peer.addStateAsync(state_zahl, 6,
							  hbk::jet::responseCallback_t(),
							  hbk::jet::stateCallback_t());

			std::promise < bool > asyncResultPromise;
			std::future <bool > asyncResultFuture = asyncResultPromise.get_future();

			hbk::jet::matcher_t match;
			match.containsAllOf = { "test", "bunch"};
			std::string printedMatcher = match.print();
			ASSERT_EQ(printedMatcher, std::string(CONTAINSALLOF) + "=[test, bunch]");
			hbk::jet::fetchId_t fetchObj = fetchingPeer.addFetchAsync(match,
																	 std::bind(&fetchcbCount, std::ref(fetchCount)),
																	 std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)));
			futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(asyncResultFuture.get(), true);
			ASSERT_EQ(fetchCount, bunchCount);

			fetchingPeer.removeFetchAsync(fetchObj);
		}


		{
			// one should match some changes will happen
			std::promise < void > p;
			auto f = p.get_future();
			fetchCount = 10;
			int notifyCount = fetchCount;

			peer.addStateAsync(state_zahl, 6, 0.2, hbk::jet::responseCallback_t(), hbk::jet::stateCallback_t());

			hbk::jet::matcher_t match;
			match.equals = state_zahl;
			hbk::jet::fetchId_t fetchZahl = fetchingPeer.addFetchAsync(match, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1), hbk::jet::responseCallback_t());
			std::string printedMatcher = match.print();
			ASSERT_EQ(printedMatcher, std::string(EQUALS) + "=" + state_zahl);
			for(int count = 0; count < notifyCount; ++count) {
				peer.notifyState(state_zahl, count);
			}

			std::future_status futureStatus = f.wait_for(std::chrono::milliseconds(100));
			ASSERT_EQ(futureStatus, std::future_status::ready);
			ASSERT_EQ(fetchCount, 0);

			fetchingPeer.removeFetchAsync(fetchZahl);
		}
	}
}

// create a whole bunch of states that are to be fetched. As soon the fetch is being registered, we expect to be informed about all states.
TEST_F(AsyncTest, test_async_get)
{

	hbk::jet::matcher_t match;
	match.contains = "test/bunch";
	std::string printedMatcher = match.print();
	ASSERT_EQ(printedMatcher, std::string(CONTAINS) + "=test/bunch");


	// first we get before any matching states do exist. We expect an empty array.
	{
		std::promise < size_t > countPromise;
		std::future < size_t > countFuture = countPromise.get_future();
		auto cbAsyncTriggerPromise = [&countPromise]( const Json::Value& data)
		{
			ASSERT_TRUE(data.isMember(hbk::jsonrpc::RESULT));
			size_t count = data[hbk::jsonrpc::RESULT].size();
			countPromise.set_value(count);
		};
		peer.getAsync(match, cbAsyncTriggerPromise);
		std::future_status futureStatus = countFuture.wait_for(std::chrono::milliseconds(1000));
		ASSERT_EQ(futureStatus, std::future_status::ready);
		ASSERT_EQ(countFuture.get(), 0);
	}

	static const unsigned int stateCount = 1000;
	std::vector < std::string > states;


	// Now add states and try again...
	for (unsigned int stateIndex=0; stateIndex<stateCount; ++stateIndex) {
		Json::Value result;
		std::string path;
		path = "test/bunch/member" + std::to_string(stateIndex);
		{
			std::promise < bool > asyncResultPromise;
			std::future <bool > asyncResultFuture = asyncResultPromise.get_future();

			peer.addStateAsync(path, Json::Value(),
							   std::bind(&cbAsyncBoolResult, std::placeholders::_1, std::ref(asyncResultPromise)),
							   std::bind(&cbState, std::placeholders::_1, std::placeholders::_2, &result));

			std::future_status futureStatus = asyncResultFuture.wait_for(std::chrono::milliseconds(1000));
			ASSERT_EQ(futureStatus, std::future_status::ready);

			ASSERT_EQ(asyncResultFuture.get(), true);
		}
		states.push_back(path);
	}

	{
		std::promise < size_t > countPromise;
		std::future < size_t > countFuture = countPromise.get_future();
		auto cbAsyncTriggerPromise = [&countPromise]( const Json::Value& data)
		{
			ASSERT_TRUE(data.isMember(hbk::jsonrpc::RESULT));
			size_t count = data[hbk::jsonrpc::RESULT].size();
			countPromise.set_value(count);
		};
		peer.getAsync(match, cbAsyncTriggerPromise);
		std::future_status futureStatus = countFuture.wait_for(std::chrono::milliseconds(1000));
		ASSERT_EQ(futureStatus, std::future_status::ready);
		ASSERT_EQ(countFuture.get(), stateCount);
	}
}

TEST_F(AsyncTest, test_oversized_message)
{
	Json::Value bigRequest;

	for (unsigned index = 0; index<hbk::jet::MAX_MESSAGE_SIZE; ++index) {
		bigRequest[index] = index;
	}
	EXPECT_ANY_THROW(peer.sendMessage(bigRequest));
}

TEST_F(AsyncTest, test_oversized_value)
{
	Json::Value oversizedValue;

	for (unsigned index = 0; index<hbk::jet::MAX_MESSAGE_SIZE; ++index) {
		oversizedValue[index] = index;
	}
	
	std::promise < Json::Value > addStatePromise;
	std::future < Json::Value > addStateFuture = addStatePromise.get_future();
	
	auto cb = [&](const Json::Value& response)
	{
		addStatePromise.set_value(response);
	};
	
	std::string jetPath = "test/hello";
	peer.addStateAsync(jetPath, oversizedValue, cb, stateCallback_t());
	
	std::future_status futureStatus = addStateFuture.wait_for(std::chrono::milliseconds(1000));
	ASSERT_EQ(futureStatus, std::future_status::ready);
	Json::Value response = addStateFuture.get();
	ASSERT_TRUE(response[jsonrpc::ERR].isObject());
	ASSERT_EQ(response[jsonrpc::ERR][jsonrpc::CODE], -1);
	ASSERT_TRUE(response[jsonrpc::ERR][jsonrpc::MESSAGE].isString());
}

TEST_F(AsyncTest, test_methods)
{
	
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::PeerAsync callingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::PeerAsync callingPeer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	{
		// method without parameters
		std::promise < Json::Value > addStatePromise;
		std::future < Json::Value > addMethodFuture = addStatePromise.get_future();
		std::promise < Json::Value > execMethodPromise;
		std::future < Json::Value > execMethodFuture = execMethodPromise.get_future();

		std::string jetPath = "test/hello";
		static const std::string stringValue = "hello";

		auto cb_hello = []( const Json::Value& )
		{
				Json::Value retVal;
				retVal = stringValue;
				return retVal;
			};

		peer.addMethodAsync(jetPath, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(addStatePromise)), cb_hello);
		addMethodFuture.wait();

		// call method asynchronuous
		callingPeer.callMethodAsync(jetPath, Json::Value(), 0.2, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(execMethodPromise)));
		execMethodFuture.wait();
		Json::Value result = execMethodFuture.get();
		std::cout << result.toStyledString() << std::endl;
		std::string resultString = result[hbk::jsonrpc::RESULT].asString();
		ASSERT_EQ(resultString, stringValue);

		peer.removeMethodAsync(jetPath);
	}

	{
		// methods with parameters as array
		std::promise < Json::Value > addStatePromise;
		std::future < Json::Value > addMethodFuture = addStatePromise.get_future();

		std::string jetPath = "test/add";

		auto cbAddArray = []( const Json::Value& params) -> Json::Value
		{
			Json::Value retVal;
			if(params.size()!=2) {
				throw hbk::jet::wrongParameterNumberException();
			}
			retVal = params[0u].asInt() + params[1].asInt();
			return retVal;
		};
		/// this is the timeout for adding (registering) the method.
		static const double timeout_s = 1.0;
		peer.addMethodAsync(jetPath, timeout_s, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(addStatePromise)), cbAddArray);
		// call method
		Json::Value result;
		Json::Value args;

		for(unsigned int i = 0; i < 1000; ++i) {
			std::promise < Json::Value > execMethodPromise;
			std::future < Json::Value > execMethodFuture = execMethodPromise.get_future();

			args[0u] = 1;
			args[1] = i;
			callingPeer.callMethodAsync(jetPath, args, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(execMethodPromise)));
			execMethodFuture.wait();
			result = execMethodFuture.get();
			ASSERT_EQ(1+i, result[hbk::jsonrpc::RESULT].asUInt());
		}
		peer.removeMethodAsync(jetPath);
	}

	{
		// removing an empty path does nothing (Just to have coverage)
		peer.removeMethodAsync("");
	}
}



TEST_F(AsyncTest, test_states)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::PeerAsync callingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::PeerAsync callingPeer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	{
		std::promise < Json::Value > addStatePromise;
		std::future < Json::Value > addStateFuture = addStatePromise.get_future();
		std::promise < Json::Value > setStatePromise;
		std::future < Json::Value > setStateFuture = setStatePromise.get_future();

		std::string jetPath = "test/hello";
		static const uint32_t oddNumber = 41;

		auto AddStateResponseCb = [&]( const hbk::jet::JsonRpcResponseObject& response)
		{
			addStatePromise.set_value(response);
		};
		
		auto stateCbForceEqual = [&](const Json::Value& value, const std::string&) -> SetStateCbResult
		{

			uint32_t requestedValue = value.asUInt();
			if (requestedValue & 1) {
				// odd value!
				return SetStateCbResult(requestedValue + 1, WARN_ADAPTED);
			}
			return SetStateCbResult(value);
		};
		peer.addStateAsync(jetPath, Json::Value(), AddStateResponseCb, stateCbForceEqual);
		std::future_status futureStatus = addStateFuture.wait_for(std::chrono::milliseconds(1000));
		ASSERT_EQ(futureStatus, std::future_status::ready);

		// set state method asynchronuous
		auto SetStateResponseCb = [&]( const hbk::jet::JsonRpcResponseObject& response)
		{
			setStatePromise.set_value(response);
			// This one does not behave! It throws an exception which may not hurt the jet peer!
			throw std::runtime_error("Error!");
		};
		callingPeer.setStateValueAsync(jetPath, oddNumber, 0.2, SetStateResponseCb);
		futureStatus = setStateFuture.wait_for(std::chrono::milliseconds(1000));
		ASSERT_EQ(futureStatus, std::future_status::ready);

		// check the received response
		Json::Value response = setStateFuture.get();
		ASSERT_EQ(response[hbk::jsonrpc::RESULT][WARNING][hbk::jsonrpc::CODE], WARN_ADAPTED);

		peer.removeStateAsync(jetPath);

		/// empty path is ignored
		peer.removeStateAsync("");
	}

}

TEST_F(AsyncTest, test_method_timeout)
{

#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::PeerAsync callingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::PeerAsync callingPeer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	// method without parameters
	std::promise < Json::Value > addMethodPromise;
	std::future < Json::Value > addMethodFuture = addMethodPromise.get_future();
	std::promise < Json::Value > execMethodPromise;
	std::future < Json::Value > execMethodFuture = execMethodPromise.get_future();

	std::string jetPath = "test/hello";

	auto cb_helloAfterOneSecond = []( const Json::Value& )
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		Json::Value retVal;
		retVal = "hello";
		return retVal;
	};

	peer.addMethodAsync(jetPath, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(addMethodPromise)), cb_helloAfterOneSecond);
	addMethodFuture.wait();

	// call method asynchronuous. Don't wait the time required by the handler method
	callingPeer.callMethodAsync(jetPath, Json::Value(), 0.01, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(execMethodPromise)));
	execMethodFuture.wait();
	Json::Value result = execMethodFuture.get();
	ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::CODE].asInt(), hbk::jsonrpc::internalError);
	peer.removeMethodAsync(jetPath);
}

TEST_F(AsyncTest, test_state_timeout)
{

#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::PeerAsync callingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::PeerAsync callingPeer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	std::promise < Json::Value > addStatePromise;
	std::future < Json::Value > addStateFuture = addStatePromise.get_future();
	std::promise < Json::Value > setStatePromise;
	std::future < Json::Value > setStateFuture = setStatePromise.get_future();

	std::string jetPath = "test/hello";

	auto cbResponseAfterOneSecond = []( const Json::Value& request, const std::string& path) -> Json::Value
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		return request;
	};

	peer.addStateAsync(jetPath, Json::Value(), std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(addStatePromise)), cbResponseAfterOneSecond);
	addStateFuture.wait();

	// set state asynchronuous. Don't wait the time required by the handler method
	callingPeer.setStateValueAsync(jetPath, Json::Value(), 0.01, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(setStatePromise)));
	setStateFuture.wait();
	Json::Value result = setStateFuture.get();
	ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::CODE].asInt(), hbk::jsonrpc::internalError);
	peer.removeMethodAsync(jetPath);
}

/// call method asynchronously. Destroy the calling peer before result is available.
/// As result, The callback of the unfinished request is to be called with an error!
TEST_F(AsyncTest, test_stop_before_result)
{
	// method without parameters
	std::promise < Json::Value > addStatePromise;
	std::future < Json::Value > addMethodFuture = addStatePromise.get_future();

	std::string jetPath = "test/hello";

	bool executed = false;
	bool executedThrowing = false;

	auto cb_helloAfterOneSecond = []( const Json::Value& ) -> Json::Value
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		Json::Value retVal;
		retVal = "hello";
		return retVal;
	};

	auto cbAsyncUnblock = [&](const Json::Value& result)
	{
		ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::CODE].asInt(), -1);
		ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::MESSAGE].asString(), "jet request has been canceled without response!");
		executed = true;
	};

	auto cbAsyncThrowing = [&]( const Json::Value& result)
	{
		ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::CODE].asInt(), -1);
		ASSERT_EQ(result[hbk::jsonrpc::ERR][hbk::jsonrpc::MESSAGE].asString(), "jet request has been canceled without response!");
		executedThrowing = true;
		throw std::runtime_error("Error!");
	};


	peer.addMethodAsync(jetPath, std::bind(&cbAsyncJsonResult, std::placeholders::_1, std::ref(addStatePromise)), cb_helloAfterOneSecond);
	addMethodFuture.wait();

	{
#ifdef USE_UNIX_DOMAIN_SOCKETS
		hbk::jet::PeerAsync callingPeer(eventloop, hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
		hbk::jet::PeerAsync callingPeer(eventloop, "127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif

		// call the method that takes time to finish
		callingPeer.callMethodAsync(jetPath, Json::Value(), 1.0, cbAsyncUnblock);

		// another call to the method that takes time to finish, this one uses a callback that throws!
		callingPeer.callMethodAsync(jetPath, Json::Value(), 1.0, cbAsyncThrowing);
	}
	ASSERT_TRUE(executed);
	ASSERT_TRUE(executedThrowing);
	peer.removeMethodAsync(jetPath);
}

/// a fetch callback throws on shutwon of the peer. This has to be caught by the peer!
TEST_F(AsyncTest, test_stop_excaption)
{
	hbk::jet::matcher_t matcher;
	auto fetchCbException =[]( const Json::Value&, int ) {
		throw std::runtime_error("bad!");
	};
	peer.addFetchAsync(matcher, fetchCbException);
}
}
