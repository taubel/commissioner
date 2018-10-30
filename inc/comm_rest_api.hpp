/*
 * comm_rest_api.hpp
 *
 *  Created on: Oct 30, 2018
 *      Author: tautvydas
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
//	TODO mutex?
//	std::mutex mutex;

	bool IsReady()
	{
		std::future_status status = future->wait_for(std::chrono::milliseconds(0));
		return (status == std::future_status::ready);
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
			T ret = future->get();
			Reset();
			return ret;
		}
		else
		{
			return old_status;
		}
	}
	void Set(T val)
	{
		Reset();
		promise->set_value(val);
		old_status = val;
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
