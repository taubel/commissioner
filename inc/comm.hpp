/*
 * MIT License
 *
 * Copyright (c) 2018 8devices
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef INC_COMM_HPP_
#define INC_COMM_HPP_

//STDLIB
#include <iostream>
#include <signal.h>
#include <thread>
#include <mutex>
#include <future>
#include <string>

//JSON
#include <jansson.h>

//PLUGIN_API
#include "plugin_api.hpp"
#include "ulfius_http_framework.hpp"

//UTILS
#include "utils/hex.hpp"

//COMMISSIONER
#include "device_hash.hpp"
#include "commissioner.hpp"
#include "logging.hpp"
#include "comm_rest_api.hpp"
#include "args_api.hpp"
#include "joiner_list_api.hpp"
#include "thread_api.hpp"

namespace ot {
namespace BorderRouter {

class CommissionerPlugin: public Plugin
{
public:

	CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        status.Set("Disabled");
        Mngr.reset(new Thread(&CommissionerPlugin::ManagerThread, this));
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        Mngr.reset(nullptr);
    }
    void ResetCommissioner()
    {
        std::cout << __func__ << std::endl;
        arguments.Clear();
        joiner_list.Clear();
        Mngr.reset(new Thread(&CommissionerPlugin::ManagerThread, this));
    }

    JoinerList joiner_list;
    CommissionerArgs arguments;
    ReturnStatus<std::string> status;

private:

    void CommissionerThread(std::future<void> futureObj, std::condition_variable* cv);
	void ManagerThread(std::future<void> futureObj);

    Commissioner* commissioner = nullptr;
	std::unique_ptr<Thread> Mngr;
};

} // namespace BorderRouter
} // namespace ot

#endif /* INC_COMM_HPP_ */
