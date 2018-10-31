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
