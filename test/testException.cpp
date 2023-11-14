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

#include <string>


#include <json/value.h>
#include <json/reader.h>

#include "hbk/exception/jsonrpc_exception.h"
#include "hbk/jsonrpc/jsonrpc_defines.h"

#include <gtest/gtest.h>


#include "jet/defines.h"

static const std::string expectedMessage = "ooops!";


TEST(textException, testConstruct)
{
	Json::CharReaderBuilder rBuilder;
	int expectedCode = -1;
	std::string expectedData("{ \"key\" : 5, \"more\" : \"text\" }");
	std::string expectedWhat;
	std::string parseErrors;
	std::unique_ptr<Json::CharReader> pCharReader(rBuilder.newCharReader());

	{
		hbk::jet::jsoncpprpcException exception(expectedCode, expectedMessage);
		expectedWhat = "code: " + std::to_string(expectedCode) + ", message: " + expectedMessage;
		ASSERT_EQ(exception.code(), expectedCode);
		ASSERT_EQ(exception.message(), expectedMessage);
		ASSERT_EQ(exception.what(), expectedWhat);
		ASSERT_EQ(exception.data(), Json::Value());
	}

	{
		hbk::jet::jsoncpprpcException exception(expectedCode, expectedMessage);
		ASSERT_EQ(exception.code(), expectedCode);
		ASSERT_EQ(exception.message(), expectedMessage);
		ASSERT_EQ(exception.data(), Json::Value());
	}

	{
		Json::Value data;
		pCharReader->parse(expectedData.c_str(), expectedData.c_str()+expectedData.length(), &data, &parseErrors);
		expectedWhat =  "code: " + std::to_string(expectedCode) + ", message: " + expectedMessage + ", data: " + data.toStyledString();
		hbk::jet::jsoncpprpcException exception(expectedCode, expectedMessage, data);
		ASSERT_EQ(exception.code(), expectedCode);
		ASSERT_EQ(exception.message(), expectedMessage);
		ASSERT_EQ(exception.what(), expectedWhat);
		ASSERT_EQ(exception.data(), data);
	}

	{
		// construct from complete error object
		std::string errorString = "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": " + std::to_string(expectedCode) + ", \"message\": \"" + expectedMessage + "\"}}";
		Json::Value errorObject;
		pCharReader->parse(errorString.c_str(), errorString.c_str()+errorString.length(), &errorObject, &parseErrors);
		hbk::jet::jsoncpprpcException exception(errorObject);
		ASSERT_EQ(exception.code(), expectedCode);
		ASSERT_EQ(exception.message(), expectedMessage);
	}

	{
		// construct without message
		std::string errorString = "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": " + std::to_string(expectedCode) + "}";
		Json::Value errorObject;
		pCharReader->parse(errorString.c_str(), errorString.c_str()+errorString.length(), &errorObject, &parseErrors);
		hbk::jet::jsoncpprpcException exception(errorObject);
		ASSERT_EQ(exception.code(), expectedCode);
		ASSERT_EQ(strlen(exception.message()), 0);
	}

	{
		// construct without code (which is really silly!)
		std::string errorString = "{\"jsonrpc\": \"2.0\", \"error\": {\"message\": \"" + expectedMessage + "\"}}";
		Json::Value errorObject;
		pCharReader->parse(errorString.c_str(), errorString.c_str()+errorString.length(), &errorObject, &parseErrors);
		hbk::jet::jsoncpprpcException exception(errorObject);
		ASSERT_EQ(exception.code(), 0);
		ASSERT_EQ(exception.message(), expectedMessage);
	}


}


TEST(textException, testDataEntries)
{
	hbk::jet::jsoncpprpcException::DataEntry dataEntry;
	hbk::jet::jsoncpprpcException::DataEntries dataEntriesRequested;
	hbk::jet::jsoncpprpcException::DataEntries dataEntriesResult;

	dataEntry.code = -2;
	dataEntry.message = "minus two";
	dataEntriesRequested["parameter 1"] = dataEntry;

	dataEntry.code = -5;
	dataEntry.message = "minus five";
	dataEntriesRequested["parameter 2"] = dataEntry;

	hbk::jet::jsoncpprpcException exception(dataEntriesRequested);
	Json::Value errorObject = exception.json();
	Json::Value data = errorObject[hbk::jsonrpc::ERR][hbk::jsonrpc::DATA];

	for( Json::Value::const_iterator itr = data.begin() ; itr != data.end() ; itr++ ) {
		std::string entryName = itr.key().asString();
		ASSERT_EQ(data[entryName][hbk::jsonrpc::CODE], dataEntriesRequested[entryName].code);
		ASSERT_EQ(data[entryName][hbk::jsonrpc::MESSAGE], dataEntriesRequested[entryName].message);
	}

	dataEntriesResult = exception.dataEntries();
	ASSERT_EQ(dataEntriesResult, dataEntriesRequested);
}

TEST(textException, testThrow)
{
	int expectedCode = -1;
	std::string expectedWhat = "code: " + std::to_string(expectedCode) + ", message: " + expectedMessage;
	try {
		throw hbk::jet::jsoncpprpcException(expectedCode, expectedMessage);
	} catch (const std::runtime_error& e) {
		ASSERT_EQ(e.what(), expectedWhat);
	}
}
