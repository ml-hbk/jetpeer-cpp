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

#include "json/value.h"

#include "hbk/jsonrpc/jsonrpc_defines.h"

#include "jet/defines.h"

namespace hbk::jet {


/// Can be used for unit testing the execution of state and method callback methods.
/// Instead of using cjet, state and methd callback methods are called directly!
/// \note Fetch operations are not supported!
class PeerAsyncMock {
public:
        void addStateAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback, stateCallback_t callback)
        {
                MockedState mockedState;
                mockedState.stateCallback = callback;
                mockedState.value = value;
                m_states[path] = mockedState;
                if (resultCallback) {
                        Json::Value resultObject;
                        resultObject[hbk::jsonrpc::ID] = ++id;
                        resultObject[hbk::jsonrpc::RESULT] = Json::objectValue;
                        resultCallback(resultObject);
                }
        }

        void addMethodAsync(const std::string& path, responseCallback_t resultCallback, methodCallback_t callback)
        {
                m_methods[path] = callback;
                if (resultCallback) {
                        Json::Value resultObject;
                        resultObject[hbk::jsonrpc::ID] = ++id;
                        resultObject[hbk::jsonrpc::RESULT] = Json::objectValue;
                        resultCallback(resultObject);
                }
        }

        void addMethodAsync(const std::string& path, double, responseCallback_t resultCallback, methodCallback_t callback)
        {
                addMethodAsync(path, resultCallback, callback);
        }

        void removeStateAsync(const std::string& path)
        {
                m_states.erase(path);
        }

        void removeMethodAsync(const std::string& path)
        {
                m_methods.erase(path);
        }

        void setStateValueAsync(const std::string& path, const Json::Value& value, responseCallback_t resultCallback)
        {
                Json::Value params;
                SetStateCbResult cbResult = m_states[path].stateCallback(value, path);

                if (resultCallback) {
                        m_states[path].value = value;

                        Json::Value response;
                        if (cbResult.result.code) {
                                response[hbk::jsonrpc::RESULT][WARNING][hbk::jsonrpc::CODE] = cbResult.result.code;
                                if (!cbResult.result.message.empty()) {
                                        response[hbk::jsonrpc::RESULT][WARNING][hbk::jsonrpc::MESSAGE] = cbResult.result.message;
                                }
                        } else {
                                response[hbk::jsonrpc::RESULT] = Json::objectValue;
                        }
                        resultCallback(response);
                }
        }


        void setStateValueAsync(const std::string& path, const Json::Value& value, double, responseCallback_t resultCallback)
        {
                setStateValueAsync(path, value, resultCallback);
        }

        void callMethodAsync(const std::string& path, const Json::Value& args, double, responseCallback_t resultCallback)
        {
                methodCallback_t& cb = m_methods[path];
                Json::Value result;
                result[hbk::jsonrpc::RESULT] = cb(args);

                if (resultCallback) {
                        resultCallback(result);
                }
        }

        void callMethodAsync(const std::string& path, const Json::Value& args, responseCallback_t resultCallback)
        {
                callMethodAsync(path, args, 2.0, resultCallback);
        }

private:

        struct MockedState {
                stateCallback_t stateCallback;
                Json::Value value;
        };

        using MockedMethods = std::map < std::string, methodCallback_t >;
        using MockedStates = std::map < std::string, MockedState >;
        MockedStates m_states;
        MockedMethods m_methods;
        static unsigned int id;
};

unsigned int PeerAsyncMock::id = 0;
}
