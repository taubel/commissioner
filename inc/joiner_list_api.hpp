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

#ifndef INC_JOINER_LIST_API_HPP_
#define INC_JOINER_LIST_API_HPP_

#include <jansson.h>
#include "commissioner_constants.hpp"
#include "response.hpp"
#include "utils/hex.hpp"
#include "utils/steering_data.hpp"
#include "device_hash.hpp"

namespace ot {
namespace BorderRouter {

struct JoinerInstance
{
    char mJoinerPSKdAscii[kPSKdLength + 1];
    char mJoinerEui64Ascii[kEui64Len * 2 + 1];
    uint8_t mJoinerEui64Bin[kEui64Len];
    char    mJoinerHashmacAscii[kEui64Len * 2 + 1];
    uint8_t mJoinerHashmacBin[kEui64Len];
};

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
		steering.Clear();
		vector.clear();
		json_array_clear(array);
		json_decref(array);
	}
	StatusCode Write(const char* string)
	{
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

				json_decref(parsed);
				return StatusCode::client_error;
			}
			vector.push_back(JoinerInstance());
			JoinerInstance& current_v = vector.back();

			strcpy(current_v.mJoinerPSKdAscii, json_string_value(psk));
			strcpy(current_v.mJoinerEui64Ascii, json_string_value(eui));
			Utils::Hex2Bytes(current_v.mJoinerEui64Ascii, current_v.mJoinerEui64Bin, kEui64Len);
			ComputeSteeringData(&steering, false, current_v.mJoinerEui64Bin);

			json_array_append_new(array, current);
		}
		updated = true;
		return StatusCode::success_ok;
	}
	StatusCode Edit(const char* body, const char* eui)
	{
		if(strlen(eui) != kEui64Len * 2)
			return StatusCode::client_error;

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
		Utils::Hex2Bytes(current.mJoinerEui64Ascii, current.mJoinerEui64Bin, kEui64Len);

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
	const char* GetLastPskd()
	{
		if(!vector.size())
			return NULL;

		return vector.back().mJoinerPSKdAscii;
	}
	const JoinerInstance* GetJoinerInstance(unsigned int num)
	{
		if(num > vector.size() - 1)
		{
			return nullptr;
		}
		std::vector<JoinerInstance>::iterator it = vector.begin();
		return &(*(it + num));
	}
};

} // namespace BorderRouter
} // namespace ot

#endif /* INC_JOINER_LIST_API_HPP_ */
