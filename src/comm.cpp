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

//COMMISSIONER
#include "commissioner_argcargv.hpp"
#include "device_hash.hpp"
#include "commissioner.hpp"
#include "logging.hpp"

//JSON
#include <jansson.h>

//PLUGIN_API
#include "plugin_api.hpp"
#include "ulfius_http_framework.hpp"

using namespace ot;
using namespace ot::BorderRouter;

class DataContainer
{
private:

	mutable std::mutex d_mutex;
	size_t size;
//	std::vector<uint8_t> data;
	uint8_t* data;
	bool d_flag = false;

public:
	DataContainer(int buffer_size)
	{
		std::cout << __func__ << std::endl;
		data = new uint8_t[buffer_size];
	}

	~DataContainer()
	{
		std::cout << __func__ << std::endl;
		delete data;
	}

	void Write(uint8_t* from, size_t len)
	{
		d_mutex.lock();
		size = len;
		memcpy(data, from, len);
		d_flag = true;
		d_mutex.unlock();
	}

	void Read(uint8_t* to)
	{
		d_mutex.lock();
		memcpy(to, data, size);
		d_flag = false;
		d_mutex.unlock();
	}

	bool DataAvailable()
	{
//		turbut nereikia
//		d_mutex.lock();
//		bool tmp = d_flag;
//		d_mutex.unlock();
//		return tmp;
		return d_flag;
	}
};

class CommissionerPlugin: public Plugin
{
public:

	CommissionerPlugin(int buffer_size): data_io(buffer_size)
    {
        std::cout << __func__ << " " << this << std::endl;
        mData = new uint8_t[100];
		Mngr = new std::thread(&CommissionerPlugin::ThreadManager, this);
		Mngr->detach();
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        delete mData;
    }

    DataContainer data_io;

protected:

    uint8_t *mData;

private:

    void InitCommissioner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m);
    void RunCommissioner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m);
    void AddJoiner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m);
	void ThreadManager();
	int JsonParse(uint8_t* buffer);

    Commissioner* commissioner = NULL;

    SteeringData steeringData;
    CommissionerArgs arguments;
	int network_num = 0;
	int joiner_num = 0;

	std::thread* InitComm = NULL;
	std::thread* RunComm = NULL;
	std::thread* AddJnr = NULL;
	std::thread* Mngr = NULL;
};

typedef void (CommissionerPlugin::*thread_fun_t)(std::future<void>, std::condition_variable*, std::mutex*);

class Thread
{
public:

	Thread(thread_fun_t fun, CommissionerPlugin* context, std::condition_variable* cond, std::mutex* cond_m): cv(cond), cv_m(cond_m)
	{
		promise = new std::promise<void>;
		future = promise->get_future();
		thread_fun = new std::thread(fun, context, std::move(future), cv, cv_m);
		std::cout << __func__ << std::endl;
	}
	Thread(thread_fun_t fun, CommissionerPlugin* context)
	{
		thread_fun = new std::thread(fun, context, std::move(future), nullptr, nullptr);
		std::cout << __func__ << std::endl;
	}
	~Thread()
	{
		if(promise)
			promise->set_value();
		if(thread_fun->joinable())
			thread_fun->join();
		delete promise;
		std::cout << __func__ << std::endl;
	}
	void Finish()
	{
		if(thread_fun->joinable())
			thread_fun->join();
	}
private:

	std::promise<void>* promise = nullptr;
	std::future<void> future;
	std::condition_variable* cv;
	std::mutex* cv_m;

	std::thread* thread_fun;
};

void CommissionerPlugin::ThreadManager()
{
	uint8_t buffer[200];
	std::condition_variable cv;
	std::mutex cv_m;
	std::unique_lock<std::mutex> lk(cv_m);
	std::cv_status ret;

	Thread* Init = nullptr;
	Thread* Run = nullptr;
	Thread* Add = nullptr;

	while(1)
	{
		if(data_io.DataAvailable())
		{
			data_io.Read(buffer);
			if(JsonParse(buffer))
			{
//				error
			}
		}
		if(network_num)
		{
			delete Init;
			delete Run;
			delete commissioner;
			Init = new Thread(&CommissionerPlugin::InitCommissioner, this, &cv, &cv_m);
			Run = new Thread(&CommissionerPlugin::RunCommissioner, this, &cv, &cv_m);
		}
		if(joiner_num)
		{
			delete Add;
			if(network_num)
				Add = new Thread(&CommissionerPlugin::AddJoiner, this, &cv, &cv_m);
			else
				Add = new Thread(&CommissionerPlugin::AddJoiner, this);
		}
		if(network_num)
		{
			ret = cv.wait_for(lk, std::chrono::seconds(2));
			if(ret == std::cv_status::timeout)
			{
				cv_m.unlock(); //	issiaiskinti kas cia vyksta
				delete Init;
				delete Run;
				delete Add;
				delete commissioner;
			}
			else
			{
				cv_m.unlock(); //	issiaiskinti kas cia vyksta
				Init->Finish();
				if(joiner_num)
					Add->Finish();
			}
		}
		else if(joiner_num)
			Add->Finish();

		network_num = 0;
		joiner_num = 0;
	}
}

int CommissionerPlugin::JsonParse(uint8_t* buffer)
{
//	ne visi turi buti string'ai
//	const char* string1 = {"{\"--network-name\" : \"test_name\",\"--xpanid\" : \"DEAD1111DEAD2222\",\"--agent-passphrase\" : \"test\",\"--agent-addr\" : \"127.0.0.1\",\"--agent-port\" : \"49191\",\"--joiner-passphrase\" : \"JNRTST\",\"--joiner-eui64\" : \"18b4300000000004\"}"};
//	const char* string2 = {"{\"--joiner-passphrase\" : \"JNRTST\",\"--joiner-eui64\" : \"18b4300000000005\"}"};

	json_error_t json_error;
	json_t* json_parsed = NULL;
	json_parsed = json_loads((const char*)buffer, 0, &json_error);

	if(!json_parsed)
	{
		printf("Json load error: %s\r\n", json_error.text);
		printf("Line: %d\r\n", json_error.line);
		printf("Column: %d\r\n", json_error.column);
		printf("Position: %d\r\n", json_error.position);
		printf("Source: %s\r\n", json_error.source);
		json_decref(json_parsed);
		return 1;
	}

	const char* network_required[5] = {"--network-name", "--xpanid", "--agent-passphrase", "--agent-addr" ,"--agent-port"};
	const char* joiner_required[2] = {"--joiner-passphrase", "--joiner-eui64"};

	for(int j = 0; j < 5; j++)
	{
		if(json_object_get(json_parsed, network_required[j]))
			network_num++;
	}
	for(int k = 0; k < 2; k++)
	{
		if(json_object_get(json_parsed, joiner_required[k]))
			joiner_num++;
	}

//	turi buti daugiau variantu, pvz agento adresas nesikeicia
	if((network_num && network_num < 5) || (joiner_num && joiner_num < 2))
	{
		network_num = 0;
		joiner_num = 0;
		printf("Error: missing arguments\r\n");
		json_decref(json_parsed);
		return 1;
	}

	int iterator = 1;
	const char* key;
	json_t* value;
	const char* arguments_string[40];
	arguments_string[0] = "Json_parsed";

	json_object_foreach(json_parsed, key, value)
	{
		arguments_string[iterator] = key;
		iterator++;
		arguments_string[iterator] = json_string_value(value); //	po kolkas tik stringai (paskui gal switch case?)
		iterator++;
	}

	int argc = json_object_size(json_parsed) * 2 + 1;
	const char *(*argv)[40] = &arguments_string;
	if(ParseArgs(argc, (char**)argv, &arguments)) //	visi keys turi tureti value
		return 1;
	json_decref(json_parsed);
	return 0;
}

void CommissionerPlugin::AddJoiner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m)
{
	if(cv_m)
	{
		std::unique_lock<std::mutex> lk(*cv_m);
		cv->wait(lk);
		if(futureObj.wait_for(std::chrono::milliseconds(1)) != std::future_status::timeout)
			return;
	}

	std::cout << "Adding joiner" << std::endl;
	steeringData = ComputeSteeringData(arguments.mSteeringLength, arguments.mAllowAllJoiners, arguments.mJoinerEui64Bin);
//	jeigu petitioning nesuveikia
//	int count = 0;
//	while(!(commissioner && commissioner->IsCommissionerAccepted()))
//	{
//		std::this_thread::sleep_for(std::chrono::milliseconds(10));
//		count++;
//		if(count > 500)
//		{
//			printf("%s timeout error\r\n", __func__);
//			return;
//		}
//	}
	commissioner->SetJoiner(arguments.mJoinerPSKdAscii, steeringData);
}

void CommissionerPlugin::InitCommissioner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m)
{
	std::cout << "Initializing commissioner" << std::endl;

    uint8_t pskcBin[OT_PSKC_LENGTH];
    int kaRate;
    int ret;

	if (arguments.mHasPSKc)
	{
		memcpy(pskcBin, arguments.mPSKcBin, sizeof(pskcBin));
	}
	else
	{
		ComputePskc(arguments.mXpanidBin, arguments.mNetworkName, arguments.mPassPhrase, pskcBin);
	}
    if (!arguments.mNeedSendCommKA)
    {
        kaRate = 0;
    }
	commissioner = new Commissioner(pskcBin, kaRate);
    srand(time(0));
    commissioner->InitDtls(arguments.mAgentAddress_ascii, arguments.mAgentPort_ascii);
    do
    {
    	if(futureObj.wait_for(std::chrono::milliseconds(1)) != std::future_status::timeout)
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
	std::this_thread::sleep_for(std::chrono::seconds(1)); //	workaround ('notify_all' gali buti iskviestas kai dar ne visi thread'ai pasieke 'wait' komanda)
	printf("Workaround Workaround Workaround Workaround Workaround Workaround \r\n");
	cv->notify_all();
	std::cout << "~Initializing commissioner" << std::endl;
	return;
}

void CommissionerPlugin::RunCommissioner(std::future<void> futureObj, std::condition_variable* cv, std::mutex* cv_m)
{
	std::unique_lock<std::mutex> lk(*cv_m);
	cv->wait(lk);
	if(futureObj.wait_for(std::chrono::milliseconds(1)) != std::future_status::timeout)
		goto exit;

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
            break;
        }
        commissioner->Process(readFdSet, writeFdSet, errorFdSet);
    	if(futureObj.wait_for(std::chrono::milliseconds(1)) != std::future_status::timeout)
    		goto exit;
    }
exit:
	std::cout << __func__ << " return" << std::endl;
	return;
}

StatusCode ParamChangeCb(const Request& req, Response& res, void* context)
{
	DataContainer* data_io = (DataContainer*)context;

	std::vector<uint8_t> body = req.getBody();
	if(body.size())
	{
		body[body.size()] = 0;
		data_io->Write(&body[0], body.size() + 1);
	}

	std::vector<uint8_t> res_body(10);

	res_body[2] = 55;

    res.setHeader("Status", "OK");
	res.setCode(StatusCode::success_ok);
	res.setBody(res_body);

	return StatusCode::success_ok;
}

static Plugin * CommCreate(PluginCore &core, Config &config)
{
	// 1: initializuoti savo plugin
	CommissionerPlugin* plugin = new CommissionerPlugin(200);
	// 2: prachekinti klaidas
	if (plugin == NULL) return NULL;

	// 3: prisiattachinti prie core
	HttpFramework& u_framework = core.getHttp();
	u_framework.addHandler("POST", "/arg", 10, ParamChangeCb, (void*)&plugin->data_io);

    std::cout << __func__ << std::endl;
    return plugin;
}

extern "C" const plugin_api_t LOREM_IPSUM =
{
    .papi_version = { 3, 2, 1},
    .papi_create = CommCreate,
};



