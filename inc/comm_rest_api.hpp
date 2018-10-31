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

#ifndef INC_COMM_REST_API_HPP_
#define INC_COMM_REST_API_HPP_

#include "response.hpp"
#include "request.hpp"
#include "joiner_list_api.hpp"
#include "args_api.hpp"
#include <future>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
class ReturnStatus
{
private:
	std::unique_ptr<std::promise<T>> promise;
	std::unique_ptr<std::future<T>> future;
	T old_status;
	std::mutex mutex;

	bool IsReady()
	{
		return (future->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
	}
	void Reset()
	{
		promise.reset(nullptr);
		future.reset(nullptr);
		promise = make_unique<std::promise<T>>();
		future = make_unique<std::future<T>>(promise->get_future());
	}

public:
	ReturnStatus()
	{
		Reset();
	}
	T Get()
	{
		if(IsReady())
		{
			mutex.lock();
			T ret = future->get();
			Reset();
			mutex.unlock();
			return ret;
		}
		else
		{
			return old_status;
		}
	}
	void Set(T val)
	{
		mutex.lock();
		Reset();
		promise->set_value(val);
		old_status = val;
		mutex.unlock();
	}
};

StatusCode JoinersWrite(Request* req, Response* res, void* context);

StatusCode JoinersEdit(Request* req, Response* res, void* context);

StatusCode JoinersRead(Request* req, Response* res, void* context);

StatusCode JoinersClear(Request* req, Response* res, void* context);

StatusCode JoinersRemove(Request* req, Response* res, void* context);

StatusCode NetworkWrite(Request* req, Response* res, void* context);

StatusCode NetworkRead(Request* req, Response* res, void* context);

StatusCode NetworkDelete(Request* req, Response* res, void* context);

#endif /* INC_COMM_REST_API_HPP_ */
