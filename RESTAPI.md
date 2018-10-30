# Rest API overview

## Network Write

* **Url**

	/network

* **Method**

	POST

* **Data**

	String containing parameters needed for commissioner, formated in JSON. Example:

		'{"network_name" : "test_name","xpanid" : "DEAD1111DEAD2222","agent_pass" : "test","agent_addr" : "127.0.0.1","agent_port" : "49191"}'

* **Description**

	Initializes and runs commissioner.

## Network Read

* **Url**

	/network

* **Method**

	GET

* **Response**

	JSON formated string. Example:

		'{"network_name" : "test_name","xpanid" : "DEAD1111DEAD2222","agent_pass" : "test","agent_addr" : "127.0.0.1","agent_port" : "49191"}'

* **Description**

	Returns current commissioner configuration

## Status Read

* **Url**

	/status

* **Method**

	GET

* **Response**

	Status string. Example:

		'Initializing commissioner', 'Commissioner timeout'...

* **Description**

	Returns string that describes the current state of the plugin

## Joiners Write

* **Url**

	/joiners

* **Method**

	POST

* **Data**

	String containing joiner devices psk and eui64 pairs, formated in JSON. Example:

		'[{"psk":"JNRTST1","eui64":"18b4300000000002"},{"psk":"JNRTST2","eui64":"18b4300000000003"},{"psk":"JNRTST3","eui64":"18b4300000000004"}]'

* **Description**

	Overwrites existing joiner device list

## Joiners Edit

* **Url**

	/joiners/[eui64]

* **Method**

	PUT

* **Data**

	String containing psk for the joiner device with eui64 specified in the URL. Example:

		'JNRTST1'

* **Description**

	Edits existing joiner or adds new to the list

## Joiners Read

* **Url**

	/joiners

* **Method**

	GET

* **Response**

	JSON formated string. Example:

		'[{"psk":"JNRTST1","eui64":"18b4300000000002"},{"psk":"JNRTST2","eui64":"18b4300000000003"},{"psk":"JNRTST3","eui64":"18b4300000000004"}]'

* **Description**

	Returns string containing current list of joiners

## Joiners Clear

* **Url**

	/joiners

* **Method**

	DELETE

* **Description**

	Clears joiners list

## Joiners Remove

* **Url**

	/joiners/[eui64]

* **Method**

	DELETE

* **Description**

	Removes specified joiner from list

## Commissioner Restart

* **Url**

	/restart

* **Method**

	POST

* **Description**

	Resets the commissioner. Deletes configuration and joiner list




