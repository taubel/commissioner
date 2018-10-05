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

class CommissionerPlugin: public Plugin
{
public:
    CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        mData = new uint8_t[100];
//        if(param_change_recv(1))
//        {
//        	printf("param_change_recv 1 returned error\r\n");
//        }
//        if(param_change_recv(2))
//        {
//        	printf("param_change_recv 2 returned error\r\n");
//        }
    }
    ~CommissionerPlugin()
    {
        std::cout << __func__ << " " << this << std::endl;
        delete mData;
    }
    void InitCommissioner();
    void RunCommissioner();
    void AddJoiner();

    Commissioner* commissioner = NULL;
    CommissionerArgs arguments;
	std::thread* RunComm = NULL;
	std::thread* AddJnr= NULL;

protected:
    uint8_t *mData;

private:
    SteeringData steeringData;
};

void CommissionerPlugin::AddJoiner()
{
	steeringData = ComputeSteeringData(arguments.mSteeringLength, arguments.mAllowAllJoiners, arguments.mJoinerEui64Bin);

	int count = 0;
	while(!(commissioner && commissioner->IsCommissionerAccepted()))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		count++;
		if(count > 500)
		{
			printf("%s timeout error\r\n", __func__);
			return;
		}
	}
	commissioner->SetJoiner(arguments.mJoinerPSKdAscii, steeringData);
//	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
}

void CommissionerPlugin::InitCommissioner()
{
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
    	ret = commissioner->TryDtlsHandshake();
    }
    while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    if (commissioner->IsValid())
    {
        commissioner->CommissionerPetition();
    }
}

void CommissionerPlugin::RunCommissioner()
{
	if(!commissioner)
		return;

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
    }
}

int ParamChangeCb(const struct _u_request* u_request, struct _u_response* u_response, void *context)
{
////	ne visi turi buti string'ai
//	const char* string1 = {"{\"--network-name\" : \"test_name\",\"--xpanid\" : \"DEAD1111DEAD2222\",\"--agent-passphrase\" : \"test\",\"--agent-addr\" : \"127.0.0.1\",\"--agent-port\" : \"49191\",\"--joiner-passphrase\" : \"JNRTST\",\"--joiner-eui64\" : \"18b4300000000004\"}"};
//	const char* string2 = {"{\"--joiner-passphrase\" : \"JNRTST\",\"--joiner-eui64\" : \"18b4300000000005\"}"};

    const IncomingUlfiusRequest request (u_request);
    OutgoingUlfiusResponse response (u_response);
    CommissionerPlugin* commplug = (CommissionerPlugin*)context;

    std::vector<uint8_t> body = request.getBody();
    size_t body_len = body.size();
	body[body_len] = 0;

	json_error_t json_error;
	json_t* json_parsed = NULL;
	json_parsed = json_loads(reinterpret_cast<const char*>(&body[0]), 0, &json_error);

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

	int network_num = 0;
	int joiner_num = 0;
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

	if((network_num && network_num < 3) || (joiner_num && joiner_num < 2))
	{
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
	if(ParseArgs(argc, (char**)argv, &commplug->arguments)) //	visi keys turi tureti value
		return 1;
	json_decref(json_parsed);

	if(network_num)
	{
		if(commplug->commissioner)
		{
//			set state invalid
			commplug->commissioner->CommissionerDeinit();
//			should wait for 'RunComm' thread to return
			delete commplug->commissioner;
		}
		std::cout << "Initializing commissioner" << std::endl;
		commplug->InitCommissioner();
		commplug->RunComm = new std::thread(&CommissionerPlugin::RunCommissioner, commplug);
		commplug->RunComm->detach();
	}
	if(joiner_num)
	{
		std::cout << "Adding joiner" << std::endl;
//		if(comm->AddJnr)
//			delete comm->AddJnr;
//		comm->AddJnr = new std::thread(&CommissionerPlugin::AddJoiner, comm);
//		comm->AddJnr->detach();
	}
	return 0;
}

static Plugin * CommCreate(PluginCore &core, Config &config)
{
	// 1: initializuoti savo plugin
	CommissionerPlugin* plugin = new CommissionerPlugin();
	// 2: prachekinti klaidas
	if (plugin == NULL) return NULL;

	// 3: prisiattachinti prie core
	HttpFramework& u_framework = core.getHttp();
	u_framework.addHandler("POST", "/arg", 10, ParamChangeCb, (void*)plugin);

    std::cout << __func__ << std::endl;
    return plugin;
}

extern "C" const plugin_api_t LOREM_IPSUM =
{
    .papi_version = { 3, 2, 1},
    .papi_create = CommCreate,
};



