/*
 * comm.hpp
 *
 *  Created on: Oct 30, 2018
 *      Author: tautvydas
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
//#include "commissioner_argcargv.hpp"
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
        Mngr.reset(new Thread(&CommissionerPlugin::ManagerThread, this));
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        Mngr.reset(nullptr);
    }
    void RestartCommissioner()
    {
        std::cout << __func__ << std::endl;
        Mngr.reset(new Thread(&CommissionerPlugin::ManagerThread, this));
    }

    JoinerList joiner_list;
    CommissionerArgs arguments;
    ReturnStatus status;

private:

    void CommissionerThread(std::future<void> futureObj, std::condition_variable* cv);
	void ManagerThread(std::future<void> futureObj);

    Commissioner* commissioner = nullptr;
	std::unique_ptr<Thread> Mngr;
};

} // namespace BorderRouter
} // namespace ot

#endif /* INC_COMM_HPP_ */
