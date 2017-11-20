#include "stdafx.h"
#include <string>

#include "instruments.h"


INSTRUMENT_ENUM_t findSerInstrumentByIdn(const string rspIdnStr, INSTRUMENT_ENUM_t instVariant)
{
	INSTRUMENT_ENUM_t inst = (INSTRUMENT_ENUM_t)INSTRUMENT_DEVICE_GENERIC;

	if (strstr(rspIdnStr.c_str(), "SMR40")) {
		inst = INSTRUMENT_TRANSMITTERS_SER__RS_SMR40;
	}
	else if (strstr(rspIdnStr.c_str(), "N5173B")) {
		inst = INSTRUMENT_TRANSMITTERS_SER__AGILENT_N5173B;
	}

	return (INSTRUMENT_ENUM_t)((int)inst | ((int)instVariant & INSTRUMENT_VARIANT_MASK));
}
