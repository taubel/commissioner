/*
 * comm_rest_api.cpp
 *
 *  Created on: Oct 30, 2018
 *      Author: tautvydas
 */

#include "comm_rest_api.hpp"
#include "comm.hpp"

using namespace ot;
using namespace BorderRouter;

StatusCode JoinersWrite(Request* req, Response* res, void* context)
{
//	TODO response
	(void)res;

	std::vector<uint8_t> body = req->getBody();
	if(!body.size())
	{
		return StatusCode::client_error;
	}
	JoinerList* vector = reinterpret_cast<JoinerList*>(context);
	std::string str(body.begin(), body.end());
	return vector->Write(str.c_str());
}

StatusCode JoinersEdit(Request* req, Response* res, void* context)
{
//	TODO response
	(void)res;

	std::vector<uint8_t> body = req->getBody();
	if(!body.size())
	{
		return StatusCode::client_error;
	}
	JoinerList* vector = reinterpret_cast<JoinerList*>(context);
	std::string str_b(body.begin(), body.end());
	std::string str_p = req->getPath();
	return vector->Edit(str_b.c_str(), str_p.c_str() + 9);
}

StatusCode JoinersRead(Request* req, Response* res, void* context)
{
	(void)req;

	JoinerList* list = reinterpret_cast<JoinerList*>(context);
	char* string = list->Read();
	std::vector<uint8_t> response_body(string, string + strlen(string));
	res->setBody(response_body);
	res->setCode(StatusCode::success_ok);
	res->setHeader("200", "OK");

	delete string;
	return StatusCode::success_ok;
}

StatusCode JoinersClear(Request* req, Response* res, void* context)
{
	(void)res;
	(void)req;
	JoinerList* list = reinterpret_cast<JoinerList*>(context);

	list->Clear();

	return StatusCode::success_ok;
}

StatusCode JoinersRemove(Request* req, Response* res, void* context)
{
	(void)res;
	JoinerList* list = reinterpret_cast<JoinerList*>(context);

	std::string str = req->getPath();
	list->Remove(str.c_str() + 9);

	return StatusCode::success_ok;
}

StatusCode NetworkWrite(Request* req, Response* res, void* context)
{
//	TODO response
	(void)res;

	std::vector<uint8_t> body = req->getBody();
	if(!body.size())
	{
		return StatusCode::client_error;
	}
	CommissionerArgs* args = reinterpret_cast<CommissionerArgs*>(context);
	std::string str(body.begin(), body.end());
	return args->ParametersChange(str.c_str());
}

StatusCode NetworkRead(Request* req, Response* res, void* context)
{
	(void)req;

	CommissionerPlugin* plugin = reinterpret_cast<CommissionerPlugin*>(context);
	std::string parameter_string = plugin->arguments.Read();
	std::string status_string = plugin->status.Get();
	std::string return_string = "Parameters: " + parameter_string + ", Commissioner status: " + status_string;
	std::vector<uint8_t> response_body(return_string.begin(), return_string.end());
	res->setBody(response_body);
	res->setCode(StatusCode::success_ok);
	res->setHeader("200", "OK");

	return StatusCode::success_ok;
}

StatusCode NetworkDelete(Request* req, Response* res, void* context)
{
	(void)req;
	(void)res;

	CommissionerPlugin* plugin = reinterpret_cast<CommissionerPlugin*>(context);
	plugin->ResetCommissioner();

	return StatusCode::success_ok;
}



