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
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <functional>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>

#include <gtest/gtest.h>


#include <json/value.h>
#include <json/writer.h>

#include "hbk/jsonrpc/jsonrpc_defines.h"

#include "jet/peer.hpp"
#include "jet/defines.h"

#ifndef _WIN32
#define USE_UNIX_DOMAIN_SOCKETS
#endif

namespace hbk::jet
{

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




static SetStateCbResult cbStatePromise( const Json::Value& value, const std::string&, std::promise <Json::Value > & resultValue)
{
	SetStateCbResult result;
	result.value = value;
	resultValue.set_value(value);
	return result;
}

static SetStateCbResult cbSleep( const Json::Value& value, std::chrono::milliseconds sleepTime)
{
	SetStateCbResult result;
	result.value = value;
	std::this_thread::sleep_for(sleepTime);
	return result;
}

static SetStateCbResult cbStateIntMod10( const Json::Value& value, SetStateCbResult& result)
{
	Json::Value retVal;
	int number = value.asInt();
	int mod10 = number % 10;

	if(mod10 != 0) {
		retVal = mod10;
		result.result.code = WARN_ADAPTED;
	}

	result.value = retVal;
	return result;
}


/// worker creates some states and changes one common state that is being changed by other threads too
static void worker(const std::string& state)
{
	using States = std::vector < std::string >;
	States states;

	for(unsigned int i=0; i<3; ++i) {
		std::string path = "worker";
		path += "/state";
		path += std::to_string(i);
		hbk::jet::Peer::local().addStateAsync(path, i, hbk::jet::responseCallback_t(), hbk::jet::stateCallback_t() );
		states.push_back(path);
	}

	for(unsigned int i = 0; i<100; ++i) {
		hbk::jet::Peer::local().notifyState(state, i);
	}

	for (const std::string& iter: states) {
		hbk::jet::Peer::local().removeStateAsync(iter);
	}
}


TEST(synchronuous, testConnect)
{
	// tcp default port on local machine => success
	ASSERT_NO_THROW(hbk::jet::Peer peer("127.0.0.1", hbk::jet::JETD_TCP_PORT));
#ifdef USE_UNIX_DOMAIN_SOCKETS
	// unix domain socket (defaults to tcp default port on local machine under windows) => success
	ASSERT_NO_THROW(hbk::jet::Peer peer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0));
#endif

	// wrong port => fail!
	ASSERT_THROW(hbk::jet::Peer peer("127.0.0.1", 8), std::runtime_error);

	// wrong unix domain socket name => fail!
	ASSERT_THROW(hbk::jet::Peer peer("/var/run/notjet.socket", 0), std::runtime_error);
}

TEST(synchronuous, testDisconnect)
{
	/// create a peer, create a state and destroy the peer.
	/// create another peer and try to fetch from the state that should have died with the first peer.
	static const std::string path = "blub";

	for(unsigned int i=0; i<10; ++i) {
		{
#ifdef USE_UNIX_DOMAIN_SOCKETS
			hbk::jet::Peer peer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "peerTest");
#else
			hbk::jet::Peer peer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "peerTest");
#endif
			peer.addState(path, 35);
			peer.getAsyncPeer().sendMessage(Json::Value());
		}

		{
			unsigned int fetchCount = 0;
#ifdef USE_UNIX_DOMAIN_SOCKETS
			hbk::jet::Peer peer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "peerTest");
#else
			hbk::jet::Peer peer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "peerTest");
#endif
			hbk::jet::matcher_t match;
			match.equals = path;


			auto fetchcbCount = [&fetchCount](const Json::Value&, int status)
			{
				if (status<0) {
					return;
				}
				++fetchCount;
			};
			hbk::jet::fetchId_t fetch = peer.addFetch(match, fetchcbCount);
			peer.removeFetchAsync(fetch);
			ASSERT_EQ(fetchCount, 0u);
		}
	}
}

TEST(synchronuous, testManyPeers)
{
	static const unsigned int peerCount = 10;
	std::vector < std::unique_ptr < hbk::jet::Peer > > peers;

	for(unsigned int peerIndex = 0; peerIndex < peerCount; ++peerIndex) {
		std::stringstream path;
		path << "test/many_peers/no" << peerIndex;
#ifdef USE_UNIX_DOMAIN_SOCKETS
		auto pPeer = std::make_unique < hbk::jet::Peer> (hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "peerTest");
#else
		auto pPeer = std::make_unique < hbk::jet::Peer> ("127.0.0.1", hbk::jet::JETD_TCP_PORT, "peerTest");
#endif
		hbk::jet::SetStateCbResult result;

		auto cbState = [&result](const Json::Value& value, const std::string&) {
			result.value = value;
			return result;
		};
		pPeer->addState(path.str(), peerIndex, cbState);
		peers.emplace_back(std::move(pPeer));
	}
}

class SyncPeerTest : public ::testing::Test {

protected:

	/// This jet peer represents the external client that sends requests to the service running in the device
	hbk::jet::Peer servingJetPeer;

	SyncPeerTest()
#ifdef USE_UNIX_DOMAIN_SOCKETS
		: servingJetPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "SyncPeerTest")
#else
		: servingJetPeer(("127.0.0.1", hbk::jet::JETD_TCP_PORT, "SyncPeerTest")
#endif
	{
	}

	virtual ~SyncPeerTest() = default;

	virtual void SetUp() override
	{
	}

	virtual void TearDown() override
	{
	}
};


TEST_F(SyncPeerTest, testConfig)
{
	std::string state = "astate";
	servingJetPeer.config("a peer name", true);
	
	servingJetPeer.getAsyncPeer().configAsync("another peer name", false);
	
	std::promise < void > successPromise;
	std::future < void > successFuture = successPromise.get_future();

	auto responseCb = [&](const Json::Value&) {
		successPromise.set_value();
	};
	servingJetPeer.getAsyncPeer().configAsync("testConfig", true, responseCb);

	std::future_status futureStatus = successFuture.wait_for(std::chrono::milliseconds(1000));
	ASSERT_EQ(futureStatus, std::future_status::ready);
}


TEST_F(SyncPeerTest, testNonExistentState)
{
	std::string state = "astate";
	servingJetPeer.addState(state, 42);
	ASSERT_THROW(servingJetPeer.setStateValue("doesntexist", "bla"), hbk::exception::jsonrpcException);
	servingJetPeer.removeStateAsync(state);
}

TEST_F(SyncPeerTest, testInfo)
{
	// get information about the jet damon and retrieve some informaiotn
	Json::Value result = servingJetPeer.info();
	ASSERT_TRUE(result[hbk::jsonrpc::RESULT]["name"].isString());
	ASSERT_TRUE(result[hbk::jsonrpc::RESULT]["version"].isString());
}



TEST_F(SyncPeerTest, testMultiThreading)
{
	Json::Value result;
	std::string jetPath = "test/oneForMany";
	auto stateCb = [](const Json::Value& value, const std::string&) -> SetStateCbResult {
		return SetStateCbResult(value);
	};
	servingJetPeer.addState(jetPath, 0, stateCb);
	static const unsigned int threadCount = 10;

	std::future < void > threadResults[threadCount];
	for(unsigned int threadIndex=0; threadIndex<threadCount; ++threadIndex) {
		threadResults[threadIndex] = std::async(std::launch::async, &worker, jetPath);
	}

	for(unsigned int threadIndex=0; threadIndex<threadCount; ++threadIndex) {
		threadResults[threadIndex].wait();
	}
	servingJetPeer.removeStateAsync(jetPath);
}


TEST_F(SyncPeerTest, testFetchState)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	
	Json::Value result;
	std::string printedMatcher;
	unsigned int fetchZahlCount;

	static const std::string statePath = "test/node/zahl";
	static const std::string otherStatePath = "test/othernode/zahl";
	
	/// the state accepts each request as the new value
	auto stateCb = [](const Json::Value& value, const std::string&) -> SetStateCbResult
	{
		return SetStateCbResult(value);
	};
	servingJetPeer.addState(statePath, 6, stateCb);
	servingJetPeer.addState(otherStatePath, 6, stateCb);

	// fetch all with path beginning with "test/" and ending with "/zahl"
	hbk::jet::matcher_t matchStartsEnds;
	matchStartsEnds.startsWith = "test";
	matchStartsEnds.endsWith = "zahl";
	printedMatcher = matchStartsEnds.print();
	ASSERT_EQ(printedMatcher, std::string(STARTSWITH) + "=test, " + ENDSWITH + "=zahl");
	
	hbk::jet::fetchId_t fetchZahl;
	
	auto fetchcbCount = [&fetchZahlCount](const Json::Value&, int status)
	{
		if (status<0) {
			return;
		}
		++fetchZahlCount;
	};
	fetchZahlCount = 0;
	fetchZahl = callingPeer.addFetch(matchStartsEnds, fetchcbCount);
	// before returning from addFetch, all matching states should have been notified
	ASSERT_EQ(fetchZahlCount, 2);
	callingPeer.setStateValue(statePath, 7);
	// before returning from setStateValue, the change should have been notified
	ASSERT_EQ(fetchZahlCount, 3);
	callingPeer.removeFetchAsync(fetchZahl);

	fetchZahlCount = 0;
	hbk::jet::matcher_t matchCaseInsensitiveContainsAllOf;
	matchCaseInsensitiveContainsAllOf.caseInsensitive = true;
	matchCaseInsensitiveContainsAllOf.containsAllOf = { "TEST", "noDe", "zahL"};
	printedMatcher = matchCaseInsensitiveContainsAllOf.print();
	ASSERT_EQ(printedMatcher, std::string(CASEINSENSITIVE) + ", " + CONTAINSALLOF + "=[TEST, noDe, zahL]");
	fetchZahl = callingPeer.addFetch(matchStartsEnds, fetchcbCount);
	ASSERT_EQ(fetchZahlCount, 2);
	callingPeer.removeFetchAsync(fetchZahl);
}


TEST_F(SyncPeerTest, testGetState)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif

	Json::Value result;
	unsigned int fetchZahlCount = 0;

	hbk::jet::matcher_t matchStartsEnds;
	matchStartsEnds.startsWith = "test";
	matchStartsEnds.endsWith = "zahl";
	// Call get before any matching exists. We expect a result with an empty array.
	result = callingPeer.get(matchStartsEnds);
	Json::Value resultArrary = result[hbk::jsonrpc::RESULT];
	fetchZahlCount = resultArrary.size();
	ASSERT_EQ(fetchZahlCount, 0);

	static const std::string statePath = "test/node/zahl";
	servingJetPeer.addState(statePath, 6);
	static const std::string otherStatePath = "test/othernode/zahl";
	servingJetPeer.addState(otherStatePath, 6);

	// fetch all with path beginning with "test/" and ending with "/zahl"
	result = callingPeer.get(matchStartsEnds);
	resultArrary = result[hbk::jsonrpc::RESULT];
	fetchZahlCount = resultArrary.size();
	ASSERT_EQ(fetchZahlCount, 2);
}

/// Check whether fetch notifications are in creation oreder.
/// \note This does test the behaviour of the jet daemon being used!
TEST_F(SyncPeerTest, testFetchSequence)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	

	
	static const std::string pathPrefix = "order/";
	std::string path;
	Json::Value value;
		
	
	std::vector < std::string > pathsOrdered;
	std::vector < std::string > pathsAsFetched;
	
	path = pathPrefix + "a";
	value = path;
	servingJetPeer.addState(path, value);
	pathsOrdered.push_back(path);

	path = pathPrefix + "0";
	value = path;
	servingJetPeer.addState(path, value);
	pathsOrdered.push_back(path);
	
	path = pathPrefix + "z";
	value = path;
	servingJetPeer.addState(path, value);
	pathsOrdered.push_back(path);

	path = pathPrefix + "c";
	value = path;
	servingJetPeer.addState(path, value);
	pathsOrdered.push_back(path);

	path = pathPrefix + "1";
	value = path;
	servingJetPeer.addState(path, value);
	pathsOrdered.push_back(path);
	
	auto fetchcb = [&](const Json::Value& params, int status)
	{
		if (status<0) {
			return;
		}
		pathsAsFetched.push_back(params[PATH].asString());
	};
	
	hbk::jet::matcher_t match;
	match.startsWith = pathPrefix;
	auto fetchId = callingPeer.addFetch(match, fetchcb);
	
	unsigned int index = 0;
	for (auto const &iter: pathsOrdered)
	{
		ASSERT_EQ(pathsAsFetched[index], iter);
		++index;
	}

	callingPeer.removeFetchAsync(fetchId);
}

TEST_F(SyncPeerTest, testFetchStateAndChange)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif

	static const std::string statePath = "test/node/zahl";

	/// simply use the value as requested
	static auto cbStateSimple = [](const Json::Value& value, const std::string&) -> SetStateCbResult
	{
		SetStateCbResult result;
		result.value = value;
		return result;
	};
	
	try {
		servingJetPeer.addState(statePath, 6, cbStateSimple);
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}

	// fetch all with path beginning with "test/" and ending with "/zahl"
	hbk::jet::matcher_t match;
	match.equals = statePath;
	std::string printedMatcher = match.print();
	ASSERT_EQ(printedMatcher, std::string(EQUALS) + "=" + statePath);
	
	unsigned int notificationCounter = 0;
	{
		auto fetchcbWake =[&notificationCounter](const Json::Value&, int stat)
		{
			if (stat==-1) {
				return;
			}
			notificationCounter++;
		};

		// notification on initial fetch
		callingPeer.addFetch(match, fetchcbWake);
		ASSERT_EQ(notificationCounter, 1);
		
		// provoke 2nd notification by set the value
		callingPeer.setStateValue(statePath, 10);
		ASSERT_EQ(notificationCounter, 2);
	}
}


TEST_F(SyncPeerTest, testFetchMethod)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif

	Json::Value result;
	unsigned int fetchCount = 0;

	static const std::string methodPath = "test/nop";
	auto methodNopCb = [](const Json::Value&) {
		return Json::Value();
	};
	double timeOut = 3.0;
	servingJetPeer.addMethod(methodPath, timeOut, methodNopCb);

	hbk::jet::matcher_t matchStartsEnds;
	matchStartsEnds.equals = "test/nop";
	hbk::jet::fetchId_t fetchIdHello;
	try {
		
		auto fetchcbCount = [&fetchCount](const Json::Value&, int status)
		{
			if (status<0) {
				return;
			}
			++fetchCount;
		};
		fetchIdHello = callingPeer.addFetch(matchStartsEnds, fetchcbCount);
	} catch (const std::runtime_error&) {
		FAIL();
	}
	ASSERT_EQ(fetchCount, 1);

	result = callingPeer.callMethod(methodPath, Json::Value());

	callingPeer.removeFetchAsync(fetchIdHello);
}

TEST_F(SyncPeerTest, notify_int64)
{
	static const std::string stateInt64 = "test/node/int64";
	int64_t initialInt64Value = 5;
	int64_t requestedInt64Value = initialInt64Value+INT32_MAX;

	std::promise <int64_t> fetchedInt64Value;
	std::future <int64_t> fInt64 = fetchedInt64Value.get_future();
	hbk::jet::Peer fetchingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "fetchingPeer");
	double timeOut = 3.0;
	servingJetPeer.addState(stateInt64, timeOut,initialInt64Value);

	auto fetchCb = [&fetchedInt64Value](const Json::Value& notification, int) {
		std::string event = notification[hbk::jet::EVENT].asString();
		if (event!=hbk::jet::CHANGE) {
			return;
		}
		fetchedInt64Value.set_value(notification[hbk::jet::VALUE].asInt64());
	};
	hbk::jet::matcher_t matchEquals;
	matchEquals.equals = stateInt64;
	fetchingPeer.addFetch(matchEquals, fetchCb);

	servingJetPeer.notifyState(stateInt64, requestedInt64Value);
	std::future_status status = fInt64.wait_for(std::chrono::milliseconds(500));
	ASSERT_EQ(status, std::future_status::ready);

	ASSERT_EQ(fInt64.get(), requestedInt64Value);
}

TEST_F(SyncPeerTest, notify_uint64)
{
	static const std::string stateUInt64 = "test/node/uint64";
	uint64_t initialUInt64Value = 5;
	uint64_t requestedUInt64Value = initialUInt64Value+UINT32_MAX;

	std::promise <uint64_t> fetchedUInt64Value;
	std::future <uint64_t> fUInt64 = fetchedUInt64Value.get_future();
	hbk::jet::Peer fetchingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "fetchingPeer");
	servingJetPeer.addState(stateUInt64, initialUInt64Value);

	auto fetchCb = [&fetchedUInt64Value](const Json::Value& notification, int) {
		std::string event = notification[hbk::jet::EVENT].asString();
		if (event!=hbk::jet::CHANGE) {
			return;
		}
		fetchedUInt64Value.set_value(notification[hbk::jet::VALUE].asUInt64());
	};
	hbk::jet::matcher_t matchEquals;
	matchEquals.equals = stateUInt64;
	fetchingPeer.addFetch(matchEquals, fetchCb);

	servingJetPeer.notifyState(stateUInt64, requestedUInt64Value);
	std::future_status status = fUInt64.wait_for(std::chrono::milliseconds(500));
	ASSERT_EQ(status, std::future_status::ready);

	ASSERT_EQ(fUInt64.get(), requestedUInt64Value);
}

TEST_F(SyncPeerTest, testFetchMatcher)
{
	Json::Value result;
	static const unsigned int COUNTMAX = 10;

	static const std::string statePath = "test/node/zahl";
	static const std::string statePathCapitalLetters = "test/node/ZAHL";
	static const std::string otherStatePath = "test/othernode/zahl";
	

#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer fetchingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "fetchingPeer");
#else
	hbk::jet::Peer fetchingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "fetchingPeer");
#endif


	try {
		servingJetPeer.addState(statePath, 6);
		servingJetPeer.addState(statePathCapitalLetters, 6);
		servingJetPeer.addState(otherStatePath, 6);
	} catch (const std::runtime_error&) {
		FAIL();
	}
	
	{
		std::promise <void> p;
		int fetchCount = COUNTMAX;
		hbk::jet::matcher_t matchEquals;
		matchEquals.equals = statePath;
		hbk::jet::fetchId_t fetchEquals = fetchingPeer.addFetch(matchEquals, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1));
		std::future <void> f = p.get_future();

		for(unsigned int count=0; count<COUNTMAX; ++count) {
			servingJetPeer.notifyState(statePath, count);
			servingJetPeer.notifyState(otherStatePath, count);
		}
		f.wait_for(std::chrono::milliseconds(50));
		ASSERT_TRUE(fetchCount==0);
		fetchingPeer.removeFetchAsync(fetchEquals);
	}

	{
		std::promise <void> p;
		std::string pathUnique = "test/node/einzig";
		servingJetPeer.addState(pathUnique, 6);
		int fetchCount = COUNTMAX;
		hbk::jet::matcher_t matchStartsWith;
		matchStartsWith.startsWith = pathUnique;
		hbk::jet::fetchId_t fetchEquals = fetchingPeer.addFetch(matchStartsWith, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1));
		std::future <void> f = p.get_future();

		for(unsigned int count=0; count<COUNTMAX; ++count) {
			servingJetPeer.notifyState(pathUnique, count);
		}
		f.wait_for(std::chrono::milliseconds(50));
		ASSERT_TRUE(fetchCount==0);
		fetchingPeer.removeFetchAsync(fetchEquals);
		servingJetPeer.removeStateAsync(pathUnique);
	}

	{
		// fetch all with path beginning with "test"
		std::promise <void> p;
		int fetchCount = COUNTMAX*2;
		hbk::jet::matcher_t matchStartsWith;
		matchStartsWith.startsWith = "test";
		hbk::jet::fetchId_t fetchStartsWith = fetchingPeer.addFetch(matchStartsWith, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1));
		std::future <void> f = p.get_future();

		for(unsigned int count=0; count<COUNTMAX; ++count) {
			servingJetPeer.notifyState(statePath, count);
			servingJetPeer.notifyState(otherStatePath, count);
		}
		f.wait_for(std::chrono::milliseconds(50));
		ASSERT_TRUE(fetchCount==0);
		fetchingPeer.removeFetchAsync(fetchStartsWith);
	}


	{
		// fetch all with path beginning with "test" and ending with "zahl"
		std::promise <void> p;
		int fetchCount = COUNTMAX*2;
		hbk::jet::matcher_t matchStartsEnds;
		matchStartsEnds.startsWith = "test";
		matchStartsEnds.endsWith = "zahl";
		hbk::jet::fetchId_t fetchStartsWith = fetchingPeer.addFetch(matchStartsEnds, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1));
		std::future <void> f = p.get_future();

		for(unsigned int count=0; count<COUNTMAX; ++count) {
			servingJetPeer.notifyState(statePath, count);
			servingJetPeer.notifyState(otherStatePath, count);
		}
		f.wait_for(std::chrono::milliseconds(50));
		ASSERT_TRUE(fetchCount==0);
		fetchingPeer.removeFetchAsync(fetchStartsWith);
	}

	{
		// fetch caseinsensitive
		std::promise <void> p;
		int fetchCount = COUNTMAX*2;
		hbk::jet::matcher_t matcher;
		
		matcher.caseInsensitive = true;
		matcher.endsWith = "zahl";
		hbk::jet::fetchId_t fetch = fetchingPeer.addFetch(matcher, std::bind(&fetchCbCountDownOnChange, std::ref(fetchCount), std::ref(p), std::placeholders::_1));
		std::future <void> f = p.get_future();

		for(unsigned int count=0; count<COUNTMAX; ++count) {
			servingJetPeer.notifyState(statePath, count);
			servingJetPeer.notifyState(statePathCapitalLetters, count);
		}
		f.wait_for(std::chrono::milliseconds(50));
		ASSERT_TRUE(fetchCount==0);
		fetchingPeer.removeFetchAsync(fetch);
	}
	
	servingJetPeer.removeStateAsync(statePath);
	servingJetPeer.removeStateAsync(statePathCapitalLetters);
	servingJetPeer.removeStateAsync(otherStatePath);
}





TEST_F(SyncPeerTest, testMethods)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer callingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "callingPeer");
#else
	hbk::jet::Peer callingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "callingPeer");
#endif
	try {
		// method without parameters
		std::string jetPath = "test/hello";
		
		auto cb_hello = []( const Json::Value& )
		{
			Json::Value retVal;
			retVal = "hello";
			return retVal;
		};

		servingJetPeer.addMethod(jetPath, cb_hello);

		// call method synchronuous
		Json::Value result = callingPeer.callMethod(jetPath, Json::Value());
		std::string resultString = result.asString();
		ASSERT_EQ(resultString, "hello");

		servingJetPeer.removeMethodAsync(jetPath);
	} catch (const std::runtime_error&) {
		FAIL();
	}

	{
		// methods with parameters as array
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
		servingJetPeer.addMethod(jetPath, cbAddArray);
		// call method
		Json::Value result;
		Json::Value args;

		for(unsigned int i=0; i<1000; ++i) {
			args[0u] = 1;
			args[1] = i;
			result = callingPeer.callMethod(jetPath, args);
			ASSERT_EQ(1+i, result.asUInt());
		}
		servingJetPeer.removeMethodAsync(jetPath);
	}

	{
		// methods with parameters as key value pairs
		std::string jetPath = "test/div";
		
		static const std::string DIVIDENT = "divident";
		static const std::string DIVISOR = "divisor";

		auto cbDiv = [](const Json::Value& params) -> Json::Value
		{
			Json::Value retVal;
			if( params[DIVIDENT].isNull()) {
				throw hbk::jet::missingParameterException(DIVIDENT);
			}
		
			if(params[DIVISOR].isNull()) {
				throw hbk::jet::missingParameterException(DIVISOR);
			}

			double divident = params[DIVIDENT].asDouble();
			double divisor = params[DIVISOR].asDouble();
			
			if (divisor==0.0)
			{
					throw hbk::exception::jsonrpcException(-1, "divisor may not be 0!");
			}
			double quotient = divident/divisor;
			return quotient;
		};
		servingJetPeer.addMethod(jetPath, cbDiv);
		Json::Value result;
		Json::Value args;

		double divident = 100000;
		double quotient;
		args[DIVIDENT] = divident;
		for(unsigned int i=1; i<=10000; ++i) {
			args[DIVISOR] = static_cast < double > (i);
			result = callingPeer.callMethod(jetPath, args);
			quotient = divident/static_cast < double > (i);
			ASSERT_NEAR(quotient, result.asDouble(), 0.0001);
		}
		
		// division by zero throws error!
		args[DIVISOR] = 0.0;
		servingJetPeer.removeMethodAsync(jetPath);
		ASSERT_THROW(callingPeer.callMethod(jetPath, args), hbk::exception::jsonrpcException);
	}

	{
		// call unknown method
		ASSERT_THROW(callingPeer.callMethod("test/unknown", Json::Value()), hbk::exception::jsonrpcException);
	}

	{
		// create method that throws an exeption!
		std::string jetPath = "test/exc";
		
		auto cb_exception = []( const Json::Value&)->Json::Value
		{
			throw hbk::exception::jsonrpcException(-42, "error description");
		};
		
		servingJetPeer.addMethod(jetPath, cb_exception);
		// call method

		Json::Value result;
		Json::Value args;
		ASSERT_THROW(callingPeer.callMethod(jetPath, args), hbk::exception::jsonrpcException);

		servingJetPeer.removeMethodAsync(jetPath);
	}
	
	{
		// check method timeout by provoking a timeout
		std::string jetPath = "test/timeout";
		static const unsigned int sleepTime_ms = 10;
		// wait less time then the sleep time
		double waitTime_s = (static_cast < double >(sleepTime_ms) / 10) / 1000;

		
		/// just waits and mirrors the value
		auto cbsleep = [](const Json::Value& value) -> Json::Value
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime_ms));
			return value;
		};

		servingJetPeer.addMethod(jetPath, cbsleep);
		ASSERT_THROW(callingPeer.callMethod(jetPath, Json::Value(), waitTime_s), hbk::exception::jsonrpcException);

		servingJetPeer.removeMethodAsync(jetPath);
	}
}

TEST_F(SyncPeerTest, testStates)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer settingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "settingPeer");
#else
	hbk::jet::Peer settingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "settingPeer");
#endif

	{
		// simple state
		std::promise < Json::Value > result;
		auto f = result.get_future();
		std::string path = "test/states/simple";
		servingJetPeer.addState(path, 42, std::bind(&cbStatePromise, std::placeholders::_1, std::placeholders::_2, std::ref(result)));
		servingJetPeer.notifyState(path, 43);
		servingJetPeer.notifyState(path, 54);

		int requestedValue = 45;
		settingPeer.setStateValue(path, requestedValue);

		f.wait_for(std::chrono::milliseconds(100));
		ASSERT_EQ(f.get(), requestedValue);
		servingJetPeer.removeStateAsync(path);
	}

	{
		// simple state with blanks in path
		std::promise < Json::Value > result;
		auto f = result.get_future();
		std::string path = "test/state with blanks in path/simple state";
		servingJetPeer.addState(path, 42, std::bind(&cbStatePromise, std::placeholders::_1, std::placeholders::_2, std::ref(result)));
		servingJetPeer.notifyState(path, 43);
		servingJetPeer.notifyState(path, 54);

		int requestedValue = 45;
		settingPeer.setStateValue(path, requestedValue);

		f.wait_for(std::chrono::milliseconds(100));
		ASSERT_EQ(f.get(), requestedValue);
		servingJetPeer.removeStateAsync(path);
	}

	{
		// simple state with other strange characters in path
		std::promise < Json::Value > result;
		auto f = result.get_future();
		std::string path = "test/dies könnte 'Mühe' machen/simple state";
		servingJetPeer.addState(path, 42, std::bind(&cbStatePromise, std::placeholders::_1, std::placeholders::_2, std::ref(result)));
		servingJetPeer.notifyState(path, 43);
		servingJetPeer.notifyState(path, 54);

		int requestedValue = 45;
		settingPeer.setStateValue(path, requestedValue);

		f.wait_for(std::chrono::milliseconds(100));
		ASSERT_EQ(f.get(), requestedValue);
		servingJetPeer.removeStateAsync(path);
	}

	{
		// bool
		std::string path("test/states/boolean");
		std::promise < Json::Value > result;
		auto f = result.get_future();

		hbk::jet::matcher_t match;
		match.equals = path;
		
		servingJetPeer.addState(path, Json::Value(), std::bind(&cbStatePromise, std::placeholders::_1, std::placeholders::_2, std::ref(result)));
		{
			// there is one match with an empty value
			Json::Value jValue = servingJetPeer.get(match);
			ASSERT_EQ(jValue[hbk::jsonrpc::RESULT].size(), 1);
			ASSERT_TRUE(jValue[hbk::jsonrpc::RESULT][0][hbk::jet::VALUE].isNull());
		}

		{
			servingJetPeer.notifyState(path, true);
			Json::Value jValue = servingJetPeer.get(match);
			ASSERT_EQ(jValue[hbk::jsonrpc::RESULT][0][hbk::jet::VALUE].asBool(), true);
		}

		{
			servingJetPeer.notifyState(path, false);
			Json::Value jValue = servingJetPeer.get(match);
			ASSERT_EQ(jValue[hbk::jsonrpc::RESULT][0][hbk::jet::VALUE].asBool(), false);
		}

		{
			settingPeer.setStateValue(path, true);
			f.wait_for(std::chrono::milliseconds(100));
			ASSERT_EQ(f.get(), true);
		}
		servingJetPeer.removeStateAsync(path);
	}


	{
		// read only state throws exception on write!
		std::string path = "test/states/ro";
		servingJetPeer.addState(path, Json::Value());
		ASSERT_THROW(settingPeer.setStateValue(path, 35), hbk::exception::jsonrpcException);
		servingJetPeer.removeStateAsync(path);
	}

	{
		// check state request timeout
		static const unsigned int sleepTime_ms = 200;
		// wait less time then the sleep time
		double waitTime_s = (static_cast < double >(sleepTime_ms) / 10) / 1000;
		std::promise < Json::Value > result;
		auto f = result.get_future();
		std::string path = "test/states/simple";

		servingJetPeer.addState(path, 42, std::bind(&cbSleep, std::placeholders::_1, std::chrono::milliseconds(sleepTime_ms)));
		int requestedValue = 45;
		ASSERT_THROW(settingPeer.setStateValue(path, requestedValue, waitTime_s), hbk::exception::jsonrpcException);
		servingJetPeer.removeStateAsync(path);
	}
}

TEST_F(SyncPeerTest, testComplexState)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer settingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "settingPeer");
#else
	hbk::jet::Peer settingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "settingPeer");
#endif

	static const std::string path = "test/states/obj";
	Json::Value request;
	SetStateCbResult result;

	static const char ZAHL[] = "zahl";
	static const char TEXT[] = "text";

	/// Expects content in the form:
	/// \code
	/// {
	///   "zahl" : < a number>,
	///   "text" : < a string>
	/// }
	/// \endcode
	/// Creates detailed error information
	auto cbComplexState = []( const Json::Value& value, const std::string&) -> SetStateCbResult
	{
		SetStateCbResult result;
		jsoncpprpcException::DataEntry entry;
		jsoncpprpcException::DataEntries dataEntries;

		if (value.isMember(ZAHL)) {
			// should be a number
			if (value[ZAHL].isInt()==false) {
				entry.code = -1;
				entry.message = "has to be of type integer";
				dataEntries[ZAHL] = entry;
			}
		}

		if (value.isMember(TEXT)) {
			// should be a string
			if (value[TEXT].isString()==false) {
				entry.code = -1;
				entry.message = "has to be of type string";
				dataEntries[TEXT] = entry;
			}
		}

		if (dataEntries.empty()==false) {
			throw hbk::jet::jsoncpprpcException(dataEntries);
		}

		result.value = value;
		std::cout << __FUNCTION__ << " done!" << std::endl;

		return result;
	};


	// some valid requests
	request[ZAHL] = 42;
	request[TEXT] = "bla";
	servingJetPeer.addState(path, request, cbComplexState);
	
	request.clear();
	request[TEXT] = "bla";
	servingJetPeer.notifyState(path, request);
	
	request.clear();
	request[TEXT] = "blub";
	request[ZAHL] = 40;
	servingJetPeer.notifyState(path, request);
	
	request.clear();
	request["zahl"] = 5;
	settingPeer.setStateValue(path, request);
	
	// provoke some errors!
	request.clear();
	// we expect a number here!
	request["zahl"] = "not a number";
	ASSERT_THROW(settingPeer.setStateValue(path, request), hbk::exception::jsonrpcException);
	ASSERT_THROW(settingPeer.setStateValue(path, request), hbk::jet::jsoncpprpcException);

	request.clear();
	// we expect a text here
	request[TEXT] = 0;
	ASSERT_THROW(settingPeer.setStateValue(path, request), hbk::exception::jsonrpcException);
	ASSERT_THROW(settingPeer.setStateValue(path, request), hbk::jet::jsoncpprpcException);
	
	
	servingJetPeer.removeStateAsync(path);
}

TEST_F(SyncPeerTest, testAdaptState)
{
#ifdef USE_UNIX_DOMAIN_SOCKETS
	hbk::jet::Peer settingPeer(hbk::jet::JET_UNIX_DOMAIN_SOCKET_NAME, 0, "settingPeer");
#else
	hbk::jet::Peer settingPeer("127.0.0.1", hbk::jet::JETD_TCP_PORT, "settingPeer");
#endif

	SetStateResult result;
	SetStateCbResult cbResult;
	std::string jetPath = "test/states/mod10";
	// simple state that is adapted
	servingJetPeer.addState(jetPath, 2, std::bind(&cbStateIntMod10, std::placeholders::_1, std::ref(cbResult)));
	servingJetPeer.notifyState(jetPath, 2);
	servingJetPeer.notifyState(jetPath, 22);

	int requestedValue;

	requestedValue = 0;
	result = settingPeer.setStateValue(jetPath, requestedValue);
	ASSERT_EQ(cbResult.value.asInt(), requestedValue);
	ASSERT_EQ(cbResult.result.code, WARN_NONE);
	ASSERT_EQ(result.code, WARN_NONE);
	
	requestedValue = 15;
	result = settingPeer.setStateValue(jetPath, requestedValue);
	ASSERT_EQ(cbResult.value.asInt(), requestedValue%10);
	ASSERT_EQ(cbResult.result.code, WARN_ADAPTED);
	ASSERT_EQ(result.code, WARN_ADAPTED);

	requestedValue = 66;
	result = settingPeer.setStateValue(jetPath, requestedValue);
	ASSERT_EQ(cbResult.value.asInt(), requestedValue%10);
	ASSERT_EQ(cbResult.result.code, WARN_ADAPTED);
	ASSERT_EQ(result.code, WARN_ADAPTED);

	servingJetPeer.removeStateAsync(jetPath);
}

TEST_F(SyncPeerTest, testErrorHandling)
{
	// try to use the same path twice
	std::string jetPath = "test/double";
	servingJetPeer.addState(jetPath, "content");
	// try to use a path that is already occupied!
	ASSERT_THROW(servingJetPeer.addState(jetPath, "content"), hbk::exception::jsonrpcException);
	servingJetPeer.removeStateAsync(jetPath);

	// try setting a state that does not exist
	std::string pathNotExistingState;
	ASSERT_THROW(servingJetPeer.setStateValue(pathNotExistingState, true), hbk::exception::jsonrpcException);
}
}
