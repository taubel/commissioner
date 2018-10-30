/*
 * comm.cpp
 *
 *  Created on: Oct 1, 2018
 *      Author: tautvydas
 */
//	TODO autorius, licenzija...

//STDLIB
#include <iostream>
#include <signal.h>
#include <thread>
#include <mutex>
#include <future>
#include <string>

//COMMISSIONER
//#include "commissioner_argcargv.hpp"
#include "device_hash.hpp"
#include "commissioner.hpp"
#include "logging.hpp"

//JSON
#include <jansson.h>

//PLUGIN_API
#include "plugin_api.hpp"
#include "ulfius_http_framework.hpp"

//UTILS
#include "utils/hex.hpp"

using namespace ot;
using namespace ot::BorderRouter;
using namespace ot::Utils;

struct JoinerInstance
{
    char mJoinerPSKdAscii[kPSKdLength + 1];
    char mJoinerEui64Ascii[kEui64Len * 2 + 1];
    uint8_t mJoinerEui64Bin[kEui64Len];
    char    mJoinerHashmacAscii[kEui64Len * 2 + 1];
    uint8_t mJoinerHashmacBin[kEui64Len];
};

class CommissionerArgs
{
private:
	std::string parameters_string;

    void CopyString(json_t* obj, char* to, int* error)
    {
    	const char* string;
    	if((string = json_string_value(obj)) == NULL)
    	{
    		*error = 1;
    		return;
    	}
    	strcpy(to, string);
    }

public:
    char* Read()
    {
		char* str = new char[parameters_string.length() + 1];
		return strcpy(str, parameters_string.c_str());
    }
    StatusCode ParametersChange(const char* string)
    {
    	//	TODO netik stringai
		json_error_t json_error;
		json_t* json_parsed = NULL;
		json_parsed = json_loads(string, 0, &json_error);

		if(!json_parsed)
		{
			return StatusCode::client_error;
		}
		json_t* ntw = json_object_get(json_parsed, "network_name");
		json_t* xpa = json_object_get(json_parsed, "xpanid");
		json_t* pas = json_object_get(json_parsed, "agent_pass");
		json_t* adr = json_object_get(json_parsed, "agent_addr");
		json_t* prt = json_object_get(json_parsed, "agent_port");

		if(!(ntw || xpa || pas || adr || prt))
		{
			json_decref(json_parsed);
			return StatusCode::client_error;
		}

		int error = 0;
		CopyString(ntw, mNetworkName, &error);
		CopyString(xpa, mXpanidAscii, &error);
		CopyString(pas, mPassPhrase, &error);
		CopyString(adr, mAgentAddress_ascii, &error);
		CopyString(prt, mAgentPort_ascii, &error);
		if(error)
		{
			json_decref(json_parsed);
			return StatusCode::client_error;
		}
		Hex2Bytes(mXpanidAscii, mXpanidBin, sizeof(mXpanidBin));

	//	default
		mSendCommKATxRate = 15;

		ComputePskc(mXpanidBin, mNetworkName, mPassPhrase, mPSKcBin);

		mParametersChanged = true;

		parameters_string.assign(string);
		json_decref(json_parsed);
		return StatusCode::success_ok;
    }

    char mAgentPort_ascii[kPortNameBufSize];
    char mAgentAddress_ascii[kIPAddrNameBufSize];
    int  mSendCommKATxRate;
    char    mXpanidAscii[kXpanidLength * 2 + 1];
    uint8_t mXpanidBin[kXpanidLength];
    char    mNetworkName[kNetworkNameLenMax + 1];
    char    mPassPhrase[kBorderRouterPassPhraseLen + 1];
    uint8_t mPSKcBin[OT_PSKC_LENGTH];

    bool mParametersChanged;
};

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class ReturnStatus
{
private:
	std::unique_ptr<std::promise<StatusCode>> promise;
	std::unique_ptr<std::future<StatusCode>> future;
	StatusCode old_status = StatusCode::unknown;
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
		promise = make_unique<std::promise<StatusCode>>();
		future = make_unique<std::future<StatusCode>>(promise->get_future());
	}

public:
	ReturnStatus()
	{
		Reset();
	}
	StatusCode Get()
	{
		if(IsReady())
		{
			StatusCode ret = future->get();
			Reset();
			return ret;
		}
		else
		{
			return old_status;
		}
	}
	void Set(StatusCode val)
	{
		Reset();
		promise->set_value(val);
		old_status = val;
	}
};

//	TODO return tipo template?
class JoinerList
{
private:
	json_t* array;

	std::vector<JoinerInstance> vector;

	SteeringData steering;

	bool updated = false;

	void ComputeSteering(bool recalc_all)
	{
		if(recalc_all)
		{
			steering.Clear();
			for(auto& i : vector)
			{
				ComputeSteeringData(&steering, false, i.mJoinerEui64Bin);
			}
		}
		else
		{
			ComputeSteeringData(&steering, false, vector.back().mJoinerEui64Bin);
		}
		updated = true;
	}
	int JsonUnpack(json_t* obj, int num, ...)
	{
		json_t** value;
		const char* key;

		va_list list;
		va_start(list, num);
		for(int i = 0; i < num; i++)
		{
			key = va_arg(list, const char*);
			value = va_arg(list, json_t**);
			if((*value = json_object_get(obj, key)) == nullptr)
			{
				return -1;
			}
		}
		va_end(list);
		return 0;
	}

public:
	JoinerList()
	{
		steering.Init(kSteeringDefaultLength);
		array = json_array();
	}
	~JoinerList()
	{
		json_decref(array);
	}
	StatusCode Write(const char* string)
	{
//		TODO patikrinimai
		steering.Clear();
		vector.clear();
		json_array_clear(array);

		json_t* parsed = json_loads(string, 0, nullptr);
		if(!parsed)
			return StatusCode::client_error;

		bool is_array = false;
		json_t* psk;
		json_t* eui;
		json_t* current = parsed;
		size_t size = 1;
		if(json_is_array(parsed))
		{
			size = json_array_size(parsed);
			is_array = true;
		}
		for(size_t index = 0; index < size; index++)
		{
			if(is_array)
			{
				current = json_array_get(parsed, index);
			}
			if(JsonUnpack(current, 2, "psk", &psk, "eui64", &eui))
			{
				if(is_array)
					json_array_clear(parsed);
				else
					json_object_clear(parsed);

				return StatusCode::client_error;
			}
			vector.push_back(JoinerInstance());
			JoinerInstance& current_v = vector.back();

			strcpy(current_v.mJoinerPSKdAscii, json_string_value(psk));
			strcpy(current_v.mJoinerEui64Ascii, json_string_value(eui));
			Hex2Bytes(current_v.mJoinerEui64Ascii, current_v.mJoinerEui64Bin, kEui64Len);
//			ComputeSteering(current_v.mJoinerEui64Bin);
			ComputeSteeringData(&steering, false, current_v.mJoinerEui64Bin);

			json_array_append_new(array, current);
		}
		updated = true;
		return StatusCode::success_ok;
	}
	StatusCode Edit(const char* body, const char* eui)
	{
//		TODO patikrinimai
		bool recalc_all = false;
		size_t index;
		json_t* value;

		json_array_foreach(array, index, value)
		{
			if(!strcmp(json_string_value(json_object_get(value, "eui64")), eui))
			{
				json_array_remove(array, index);
				vector.erase(vector.begin() + index);
				recalc_all = true;
				break;
			}
		}

		json_t* obj = json_object();
		json_object_set_new(obj, "psk", json_string(body));
		json_object_set_new(obj, "eui64", json_string(eui));
		json_array_append_new(array, obj);

		vector.push_back(JoinerInstance());
		JoinerInstance& current = vector.back();

		strcpy(current.mJoinerPSKdAscii, body);
		strcpy(current.mJoinerEui64Ascii, eui);
		Hex2Bytes(current.mJoinerEui64Ascii, current.mJoinerEui64Bin, kEui64Len);

		ComputeSteering(recalc_all);

		return StatusCode::success_ok;
	}
	void Clear()
	{
		steering.Clear();
		vector.clear();
		json_array_clear(array);
		updated = true;
	}
	char* Read()
	{
		return json_dumps(array, JSON_COMPACT);
	}
	StatusCode Remove(const char* eui)
	{
		size_t index;
		json_t* value;
		json_array_foreach(array, index, value)
		{
			if(!strcmp(json_string_value(json_object_get(value, "eui64")), eui))
			{
				json_array_remove(array, index);
				vector.erase(vector.begin() + index);
				ComputeSteering(true);
				return StatusCode::success_ok;
			}
		}
		return StatusCode::client_error_not_found;
	}
	SteeringData GetSteering()
	{
		return steering;
	}
	bool WasListUpdated()
	{
		return updated;
	}
	void ListUpdateFinished()
	{
		updated = false;
	}
//	TODO laikinai
	const char* GetLastPskd()
	{
		return vector.back().mJoinerPSKdAscii;
	}
};

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

class CommissionerPlugin: public Plugin
{
public:

	CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        Mngr = new Thread(&CommissionerPlugin::ThreadManager, this);
//    	TODO ar nereikia detach?
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        delete Mngr;
    }
    void RestartCommissioner()
    {
        std::cout << __func__ << std::endl;
    	delete Mngr;
    	Mngr = nullptr;
    	Mngr = new Thread(&CommissionerPlugin::ThreadManager, this);
    }

    JoinerList joiner_list;
    CommissionerArgs arguments;
    ReturnStatus status;

private:

    void InitCommissioner(std::future<void> futureObj, std::condition_variable* cv);
    void RunCommissioner(std::future<void> futureObj);
	void ThreadManager(std::future<void> futureObj);

    Commissioner* commissioner = nullptr;
    Thread* Mngr = nullptr;
};

void CommissionerPlugin::ThreadManager(std::future<void> futureObj)
{
	std::condition_variable cv;
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk(cv_m);
	std::cv_status ret;

//	TODO scoped_ptr, shared_ptr arba kitas 'smart' pointeris kad isvengt problemu su dvigubu delete
	Thread* Init = nullptr;
	Thread* Run = nullptr;
//	TODO pabaigti statusus, vietoje StatusCode reiketu grazinti kazkoki teksta kuris butu labiau descriptive

	while(1)
	{
		if(arguments.mParametersChanged)
		{
			delete Run;
			Run = nullptr;
			delete commissioner;
			commissioner = nullptr;

			commissioner = new Commissioner(arguments.mPSKcBin, arguments.mSendCommKATxRate);

			Init = new Thread(&CommissionerPlugin::InitCommissioner, this, &cv);
//			TODO spurious wakeup
//			TODO InitCommissioner gali iskviesti notify_all pries ThreadManager pasiekiant wait_for. Galima pagalba aprasyta std::condition_variable reference puslapy
			ret = cv.wait_for(lk, std::chrono::seconds(2));
			cv_m.unlock(); //	TODO:	issiaiskinti kas cia vyksta
			delete Init;
			Init = nullptr;
			if(ret == std::cv_status::timeout)
			{
				status.Set(StatusCode::server_error_internal_server_error);
				arguments.mParametersChanged = false;
				goto cont;
			}
			Run = new Thread(&CommissionerPlugin::RunCommissioner, this);
			status.Set(StatusCode::success_ok);
			arguments.mParametersChanged = false;
		}
cont:
		if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			delete Init;
			Init = nullptr;
			delete Run;
			Run = nullptr;
			delete commissioner;
			commissioner = nullptr;
			return;
		}
	}
}

void CommissionerPlugin::InitCommissioner(std::future<void> futureObj, std::condition_variable* cv)
{
	std::cout << "Initializing commissioner" << std::endl;
    int ret;

    srand(time(0));
    commissioner->InitDtls(arguments.mAgentAddress_ascii, arguments.mAgentPort_ascii);
    do
    {
    	if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    	{
    		std::cout << __func__ << " timeout" << std::endl;
    		goto exit;
    	}
    	ret = commissioner->TryDtlsHandshake();
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (commissioner->IsValid())
    {
        commissioner->CommissionerPetition();
    }
    else
    {
    	status.Set(StatusCode::server_error_internal_server_error);
    }
exit:
	cv->notify_all();
	std::cout << "~Initializing commissioner" << std::endl;
	return;
}

void CommissionerPlugin::RunCommissioner(std::future<void> futureObj)
{
	std::cout << "Running commissioner thread" << std::endl;
    while (commissioner->IsValid())
    {
        int            maxFd   = -1;
        struct timeval timeout = {1, 0};
        int            rval;

        fd_set readFdSet;
        fd_set writeFdSet;
        fd_set errorFdSet;

        FD_ZERO(&readFdSet);
        FD_ZERO(&writeFdSet);
        FD_ZERO(&errorFdSet);
        commissioner->UpdateFdSet(readFdSet, writeFdSet, errorFdSet, maxFd, timeout);
        rval = select(maxFd + 1, &readFdSet, &writeFdSet, &errorFdSet, &timeout);
        if (rval < 0)
        {
            otbrLog(OTBR_LOG_ERR, "select() failed", strerror(errno));
            status.Set(StatusCode::server_error_internal_server_error);
            break;
        }
        commissioner->Process(readFdSet, writeFdSet, errorFdSet);

        if(joiner_list.WasListUpdated())
        {
        	commissioner->CommissionerSet(joiner_list.GetSteering());
        	const char* last_pskd = joiner_list.GetLastPskd();
        	if(last_pskd)
        		commissioner->SetJoiner(last_pskd);
        	joiner_list.ListUpdateFinished();
        }

    	if(futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
    		goto exit;
    }
exit:
	std::cout << __func__ << " return" << std::endl;
	return;
}

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

//	TODO gal iseina konteksta padaryt const?
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

	CommissionerArgs* args = reinterpret_cast<CommissionerArgs*>(context);
	char* string = args->Read();
	std::vector<uint8_t> response_body(string, string + strlen(string));
	res->setBody(response_body);
	res->setCode(StatusCode::success_ok);
	res->setHeader("200", "OK");

	delete string;
	return StatusCode::success_ok;
}

StatusCode StatusRead(Request* req, Response* res, void* context)
{
	ReturnStatus* status = reinterpret_cast<ReturnStatus*>(context);
	(void)req;
	(void)res;

//	TODO ar reikalingas laukimas? Po kolkas jei statusas nepasikeite, returnina sena reiksme
//	TODO statusas turetu buti ne 'StatusCode' tipo, reiksme turi buti irasoma i body
	return status->Get();
}

StatusCode Restart(Request* req, Response* res, void* context)
{
	(void)req;
	(void)res;

	CommissionerPlugin* plugin = reinterpret_cast<CommissionerPlugin*>(context);
	plugin->RestartCommissioner();

	return StatusCode::success_ok;
}

static Plugin * CommCreate(PluginManagerCore *core)
{
	CommissionerPlugin* plugin = new CommissionerPlugin;
	if (plugin == NULL) return NULL;

	HttpFramework* u_framework = core->getHttpFramework();
	u_framework->addHandler("POST", "/network", 10, NetworkWrite, reinterpret_cast<void*>(&plugin->arguments));
	u_framework->addHandler("GET", "/network", 10, NetworkRead, reinterpret_cast<void*>(&plugin->arguments));
	u_framework->addHandler("GET", "/status", 10, StatusRead, reinterpret_cast<void*>(&plugin->status));
	u_framework->addHandler("POST", "/restart", 10, Restart, reinterpret_cast<void*>(plugin));

	u_framework->addHandler("POST", "/joiners", 9, JoinersWrite, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("PUT", "/joiners/*", 10, JoinersEdit, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("DELETE", "/joiners", 9, JoinersClear, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("DELETE", "/joiners/*", 10, JoinersRemove, reinterpret_cast<void*>(&plugin->joiner_list));
	u_framework->addHandler("GET", "/joiners", 10, JoinersRead, reinterpret_cast<void*>(&plugin->joiner_list));

    std::cout << __func__ << std::endl;
    return plugin;
}

static void CommDestroy(Plugin* plugin)
{
	CommissionerPlugin* comm_plugin = reinterpret_cast<CommissionerPlugin*>(plugin);
	delete comm_plugin;
//	TODO sunaikinti handlerius
}

extern "C" const plugin_api_t PLUGIN_API =
{
    .version = { 3, 2, 1},
    .create = CommCreate,
	.destroy = CommDestroy,
};




