/*
 * args_api.hpp
 *
 *  Created on: Oct 30, 2018
 *      Author: tautvydas
 */

#ifndef INC_ARGS_API_HPP_
#define INC_ARGS_API_HPP_

#include <jansson.h>
#include "response.hpp"
#include "commissioner_constants.hpp"
#include "utils/hex.hpp"
#include "web/pskc-generator/pskc.hpp"

namespace ot {
namespace BorderRouter {

class CommissionerArgs
{
private:
	json_t* json_obj;

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
    CommissionerArgs()
	{
    	json_obj = json_object();
	}
    ~CommissionerArgs()
	{
    	json_object_clear(json_obj);
    	json_decref(json_obj);
	}
    void Clear()
    {
    	json_object_clear(json_obj);
    }
    char* Read()
    {
		return json_dumps(json_obj, JSON_COMPACT);
    }
    StatusCode ParametersChange(const char* string)
    {
    	//	TODO netik stringai
		json_error_t json_error;
		json_t* json_parsed;
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
		Utils::Hex2Bytes(mXpanidAscii, mXpanidBin, sizeof(mXpanidBin));

	//	default
		mSendCommKATxRate = 15;

		ComputePskc(mXpanidBin, mNetworkName, mPassPhrase, mPSKcBin);

		mParametersChanged = true;

		json_object_update(json_obj, json_parsed);
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

} // namespace BorderRouter
} // namespace ot

#endif /* INC_ARGS_API_HPP_ */
