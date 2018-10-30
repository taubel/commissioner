/*
 * thread_api.hpp
 *
 *  Created on: Oct 30, 2018
 *      Author: tautvydas
 */

#ifndef INC_THREAD_API_HPP_
#define INC_THREAD_API_HPP_

namespace ot {
namespace BorderRouter {

class CommissionerPlugin;
typedef void (CommissionerPlugin::*init_thread_fun_t)(std::future<void>, std::condition_variable*);
typedef void (CommissionerPlugin::*thread_fun_t)(std::future<void>);

class Thread
{
public:

	Thread(init_thread_fun_t fun, CommissionerPlugin* context, std::condition_variable* cond)
	{
		promise = new std::promise<void>;
		future = promise->get_future();
		thread_fun = new std::thread(fun, context, std::move(future), cond);
		std::cout << __func__ << std::endl;
	}
	Thread(thread_fun_t fun, CommissionerPlugin* context)
	{
		promise = new std::promise<void>;
		future = promise->get_future();
		thread_fun = new std::thread(fun, context, std::move(future));
		std::cout << __func__ << std::endl;
	}
	~Thread()
	{
		promise->set_value();
		if(thread_fun->joinable())
			thread_fun->join();
		delete promise;
		delete thread_fun;
		std::cout << __func__ << std::endl;
	}

private:

	std::promise<void>* promise = nullptr;
	std::future<void> future;

	std::thread* thread_fun;
};

} // namespace BorderRouter
} // namespace ot

#endif /* INC_THREAD_API_HPP_ */
