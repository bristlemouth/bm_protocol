#include "debug_ina232.h"
#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "cli.h"
#include <stdlib.h>

static INA232* _ina232_instance = NULL;

static BaseType_t ina232Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString);

static const CLI_Command_Definition_t cmdINA232 = {
  // Command string
  "ina",
  // Help string
  "ina:\n"
  " * ina pow - get latest power measurement\n"
  " * ina conv - get total conversion time\n",
  // Command function
  ina232Command,
  // Number of parameters
  1
};


void debugINA232Init(INA232* ina232) {
    _ina232_instance = ina232;
    FreeRTOS_CLIRegisterCommand( &cmdINA232 );
}

static BaseType_t ina232Command( char *writeBuffer,
                                  size_t writeBufferLen,
                                  const char *commandString) {
	BaseType_t parameterStringLength;

	( void ) writeBuffer;
	( void ) writeBufferLen;

	do {
		if(!_ina232_instance){
			printf("ina232 not initialized\n");
			break;
		}
		const char *parameter = FreeRTOS_CLIGetParameter(
			commandString,
			1,
			&parameterStringLength);
		if(parameter == NULL){
			printf("ERR Invalid paramters\n");
			break;
		}
		if(strncmp("pow", parameter, parameterStringLength) == 0){
			if(!_ina232_instance->measurePower()){
				printf("Failed to measure power!\n");
				break;
			}
			float _voltage = 0;
			float _current = 0;
			_ina232_instance->getPower(_voltage, _current);
			printf("Bus Voltage: %f\n", _voltage);
			printf("Bus Current: %f\n", _current);
			printf("Bus Power: %f\n", _current*_voltage);
		}
		else if (strncmp("conv", parameter, parameterStringLength) == 0){
			// get the total conversion time and print it out
			uint32_t convTime = _ina232_instance->getTotalConversionTimeMs();
			printf("Total Conversion Time Ms: %" PRIu32 "\n", convTime);
		}
		else{
			printf("ERR Invalid paramters\n");
			break;
		}
	} while(0);
	return pdFALSE;
}