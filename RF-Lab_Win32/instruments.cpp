#include "stdafx.h"
#include <string>

#include "instruments.h"


INSTRUMENT_ENUM_t findSerInstrumentByIdn(const string rspIdnStr, int instVariant)
{
	INSTRUMENT_ENUM_t inst = (INSTRUMENT_ENUM_t)INSTRUMENT_DEVICE_GENERIC;

	if (strstr(rspIdnStr.c_str(), "SMR40")) {
		inst = INSTRUMENT_TRANSMITTERS_SER__RS_SMR40;

	} else if (strstr(rspIdnStr.c_str(), "SMB100A")) {
		inst = INSTRUMENT_TRANSMITTERS_SER__RS_SMB100A;

	} else if (strstr(rspIdnStr.c_str(), "SMC100A")) {
		inst = INSTRUMENT_TRANSMITTERS_USB__RS_SMC100A;

	} else if (strstr(rspIdnStr.c_str(), "SMC100A")) {
		inst = INSTRUMENT_TRANSMITTERS_USB__RS_SMC100A;

	} else if (strstr(rspIdnStr.c_str(), "FSEK 20")) {
		inst = INSTRUMENT_RECEIVERS_SER__RS_FSEK20;
	}

	if (instVariant) {
		return (INSTRUMENT_ENUM_t)((((int)inst) & INSTRUMENT_DEVICE_MASK) | (instVariant & (INSTRUMENT_VARIANT_MASK | INSTRUMENT_CONNECT_MASK)));

	} else {
		return inst == INSTRUMENT_DEVICE_GENERIC ? INSTRUMENT_NONE : inst;
	}
}
