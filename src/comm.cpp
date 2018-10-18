/*
 * comm.cpp
 *
 *  Created on: Oct 1, 2018
 *      Author: tautvydas
 */

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
    void Read(char* to)
    {
    	strcpy(to, parameters_string.c_str());
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

		mParametersChanged = true;

		parameters_string.append(string);
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
	std::string joiners_string;

	std::vector<JoinerInstance> vector;

	SteeringData steering;

	bool updated = false;

	void ComputeSteering(uint8_t* euibin)
	{
		ComputeSteeringData(&steering, false, euibin);
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
	}
	StatusCode Append(const char* string)
	{
		json_t* parsed = json_loads(string, 0, nullptr);
		if(!parsed)
			return StatusCode::client_error;

		size_t index;
		json_t* value;
		json_t* psk;
		json_t* eui;
		json_array_foreach(parsed, index, value)
		{
			if(JsonUnpack(value, 2, "psk", &psk, "eui64", &eui))
			{
				json_decref(parsed);
				return StatusCode::client_error;
			}

			vector.push_back(JoinerInstance());
			JoinerInstance& current = vector.back();

			strcpy(current.mJoinerPSKdAscii, json_string_value(psk));
			strcpy(current.mJoinerEui64Ascii, json_string_value(eui));
			Hex2Bytes(current.mJoinerEui64Ascii, current.mJoinerEui64Bin, kEui64Len);
			ComputeSteering(current.mJoinerEui64Bin);
		}
		updated = true;
		joiners_string.append(string);
		json_decref(parsed);
		return StatusCode::success_ok;
	}
	void Read(char* to)
	{
		strcpy(to, joiners_string.c_str());
	}
	void Remove()
	{
//		TODO
		/*
		 * Argumentas: psk arba eui64
		 * 1. Iesko per vektoriu
		 * 2. Issisaugo indexa i remove_num vektoriu/list/...
		 * 3. Istrina instance
		 * 4. Notify
		 */
	}
	SteeringData GetSteering()
	{
		return steering;
	}
	bool WasListUpdated()
	{
		return updated;
	}
	void ListUpdateFinish()
	{
		updated = false;
	}
//	TODO laikinai
	const char* GetLastPskd()
	{
		return vector.back().mJoinerPSKdAscii;
	}
};

class CommissionerPlugin: public Plugin
{
public:

	CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        mngr_future = mngr_promise.get_future();
		Mngr = new std::thread(&CommissionerPlugin::ThreadManager, this, std::move(mngr_future));
		Mngr->detach();
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        mngr_promise.set_value();
    }

    JoinerList joiner_list;
    CommissionerArgs arguments;
    ReturnStatus status;

private:

    void InitCommissioner(std::future<void> futureObj, std::condition_variable* cv);
    void RunCommissioner(std::future<void> futureObj);
	void ThreadManager(std::future<void> futureObj);

    Commissioner* commissioner = nullptr;

	std::thread* Mngr = nullptr;
	std::promise<void> mngr_promise;
	std::future<void> mngr_future;
};

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

void CommissionerPlugin::ThreadManager(std::future<void> futureObj)
{
	std::condition_variable cv;
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk(cv_m);
	std::cv_status ret;

	Thread* Init = nullptr;
	Thread* Run = nullptr;

	while(1)
	{
		if(arguments.mParametersChanged)
		{
			delete Run;
			Run = nullptr;
			delete commissioner;
			commissioner = nullptr;

			Init = new Thread(&CommissionerPlugin::InitCommissioner, this, &cv);
//			TODO spurious wakeup
//			TODO InitCommissioner gali iskviesti notify_all pries ThreadManager pasiekiant wait_for. Galima pagalba aprasyta std::condition_variable reference puslapy
			ret = cv.wait_for(lk, std::chrono::seconds(2));
			cv_m.unlock(); //	TODO:	issiaiskinti kas cia vyksta
			delete Init;
			if(ret == std::cv_status::timeout)
			{
				status.Set(StatusCode::server_error_internal_server_error);
				goto cont;
			}
			Run = new Thread(&CommissionerPlugin::RunCommissioner, this);
			status.Set(StatusCode::success_ok);
			arguments.mParametersChanged = false;
		}
cont:
		if(futureObj.wait_for(std::chrono::milliseconds(0)) != std::future_status::timeout)
		{
			delete Init;
			delete Run;
			return;
		}
	}
}

void CommissionerPlugin::InitCommissioner(std::future<void> futureObj, std::condition_variable* cv)
{
	std::cout << "Initializing commissioner" << std::endl;

    uint8_t pskcBin[OT_PSKC_LENGTH];
    int kaRate = arguments.mSendCommKATxRate;
    int ret;

	ComputePskc(arguments.mXpanidBin, arguments.mNetworkName, arguments.mPassPhrase, pskcBin);
	commissioner = new Commissioner(pskcBin, kaRate);
    srand(time(0));
    commissioner->InitDtls(arguments.mAgentAddress_ascii, arguments.mAgentPort_ascii);
    do
    {
    	if(futureObj.wait_for(std::chrono::milliseconds(0)) != std::future_status::timeout)
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
        	commissioner->SetJoiner(joiner_list.GetLastPskd());
        	joiner_list.ListUpdateFinish();
        }

    	if(futureObj.wait_for(std::chrono::milliseconds(1)) != std::future_status::timeout)
    		goto exit;
    }
exit:
	std::cout << __func__ << " return" << std::endl;
	return;
}

StatusCode JoinersWrite(const Request& req, Response& res, void* context)
{
//	TODO response
	(void)res;
	std::vector<uint8_t> body = req.getBody();
	if(!body.size())
	{
		return StatusCode::client_error;
	}
	const char* string = reinterpret_cast<const char*>(body.data());
	JoinerList* vector = reinterpret_cast<JoinerList*>(context);

	return vector->Append(string);
}

StatusCode NetworkWrite(const Request& req, Response& res, void* context)
{
//	TODO response
	(void)res;

	std::vector<uint8_t> body = req.getBody();
	if(!body.size())
	{
		return StatusCode::client_error;
	}

	const char* string = reinterpret_cast<const char*>(body.data());
	CommissionerArgs* args = reinterpret_cast<CommissionerArgs*>(context);

	return args->ParametersChange(string);
}

StatusCode NetworkRead(const Request& req, Response& res, void* context)
{
	(void)req;

	CommissionerArgs* args = reinterpret_cast<CommissionerArgs*>(context);
	char* string = new char[128];
	args->Read(string);
	std::vector<uint8_t> response_body(string, string + strlen(string));
	res.setBody(response_body);
	res.setCode(StatusCode::success_ok);
	res.setHeader("200", "OK");

	delete string;
	return StatusCode::success_ok;
}

//	TODO gal iseina konteksta padaryt const?
StatusCode JoinersRead(const Request& req, Response& res, void* context)
{
	(void)req;

	JoinerList* list = reinterpret_cast<JoinerList*>(context);
	char* string = new char[1024];
	list->Read(string);
	std::vector<uint8_t> response_body(string, string + strlen(string));
	res.setBody(response_body);
	res.setCode(StatusCode::success_ok);
	res.setHeader("200", "OK");

	delete string;
	return StatusCode::success_ok;
}

StatusCode StatusRead(const Request& req, Response& res, void* context)
{
	ReturnStatus* status = reinterpret_cast<ReturnStatus*>(context);
	(void)req;
	(void)res;

//	TODO ar reikalingas laukimas? Po kolkas jei statusas nepasikeite, returnina sena reiksme
	return status->Get();
}

static Plugin * CommCreate(PluginCore &core, Config &config)
{
//	TODO po kolkas nenaudojamas
	(void)config;
	// 1: initializuoti savo plugin
	CommissionerPlugin* plugin = new CommissionerPlugin;
	// 2: prachekinti klaidas
	if (plugin == NULL) return NULL;

	// 3: prisiattachinti prie core
	HttpFramework& u_framework = core.getHttp();
	u_framework.addHandler("POST", "/network", 10, NetworkWrite, (void*)&plugin->arguments);
	u_framework.addHandler("GET", "/network", 10, NetworkRead, (void*)&plugin->arguments);
	u_framework.addHandler("GET", "/status", 10, StatusRead, reinterpret_cast<void*>(&plugin->status));
	u_framework.addHandler("PUT", "/joiners", 10, JoinersWrite, (void*)&plugin->joiner_list);
	u_framework.addHandler("GET", "/joiners", 10, JoinersRead, (void*)&plugin->joiner_list);

    std::cout << __func__ << std::endl;
    return plugin;
}

extern "C" const plugin_api_t LOREM_IPSUM =
{
    .papi_version = { 3, 2, 1},
    .papi_create = CommCreate,
};




