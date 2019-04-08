#include "stdafx.h"

#include "externals.h"

#include "agentCom.h"


template <class T>  void SafeReleaseDelete(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
}



agentCom::agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt)
				 : _isStarted(FALSE)
				 , _running(FALSE)
				 , _done(FALSE)
				 , _src(src)
				 , _tgt(tgt)
				 , _isIec(FALSE)
				 , _iecAddr(0)
{
}


void agentCom::start(void)
{
	if (!_isStarted) {
		agent::start();
		_isStarted = true;
	}
}

bool agentCom::isRunning(void)
{
	return _running;
}

void agentCom::Release(void)
{
	// signaling
	if (_running) {
		(void) shutdown();
	}

	// wait until all threads are done
	while (!_done) {
		Sleep(10);
	}

	_isStarted = false;

	// release objects
	// ... none
}

bool agentCom::shutdown(void)
{
	bool old_running = _running;

	// signal to shutdown
	_running = FALSE;

	return old_running;
}


string agentCom::trim(string haystack)
{
	string needle;
	size_t count = haystack.length();

	for (size_t idx = 0; idx < count; idx++) {
		char c = haystack.at(idx);

		if (0x20 <= c) {
			char ary[2] = { c, 0 };
			needle.append(string(ary));
		}
	}
	return needle;
}


bool agentCom::isIec(void)
{
	return _isIec;
}

void agentCom::setIecAddr(int iecAddr)
{
	if (iecAddr >= 0 && iecAddr <= 255) {
		_iecAddr = iecAddr;
		_isIec = TRUE;
	}
	else {
		_iecAddr = 0;
		_isIec = FALSE;
	}
}

int agentCom::getIecAddr(void)
{
	return _iecAddr;
}

/* Ser-Com blocking message transfer */
string agentCom::doComRequestResponse(const string in)
{
	if (in.length() && (_hCom != INVALID_HANDLE_VALUE)) {
		char lpBuffer[C_BUF_SIZE];
		DWORD dNoOFBytestoWrite;         // No of bytes to write into the port
		DWORD dNoOfBytesWritten = 0;     // No of bytes written to the port

		dNoOFBytestoWrite = snprintf(lpBuffer, C_BUF_SIZE, "%s", in.c_str());

		int status = WriteFile(_hCom,	// Handle to the Serial port
			lpBuffer,					// Data to be written to the port
			dNoOFBytestoWrite,			// Number of bytes to write
			&dNoOfBytesWritten,			// Bytes written
			NULL);

		if (status) {
			const DWORD dNoOFBytestoRead = C_BUF_SIZE;
			DWORD dNoOfBytesRead = 0;

			status = FlushFileBuffers(_hCom);
			Sleep(25);
			status = ReadFile(_hCom,	// Handle to the Serial port
				lpBuffer,				// Buffer where data from the port is to be written to
				dNoOFBytestoRead,		// Number of bytes to be read
				&dNoOfBytesRead,		// Bytes read
				NULL);

			if (status) {
				return string(lpBuffer, dNoOfBytesRead);
			}
		}
	}

	/* Fail case */
	return string();
}

/* Check if device reacts like a Zolix-SC300 turntable */
string agentCom::getZolixIdn(void)
{
	doComRequestResponse(string(C_ZOLIX_VERSION_REQ_STR1));
	string resp = doComRequestResponse(string(C_ZOLIX_VERSION_REQ_STR2));
	size_t pos = resp.find(C_ZOLIX_VERSION_VAL_STR, 0);
	if (pos != string::npos) {
		return trim(resp);
	}
	else {
		return string();
	}
}

/* Prepare the IEC serial connection */
void agentCom::iecPrepare(int iecAddr)
{
	if (iecAddr != C_SET_IEC_ADDR_INVALID) {
		/* check if USB<-->IEC adapter is responding */
		const string iecSyncingStr = string("++ver\r\n");

		(void)        doComRequestResponse(iecSyncingStr);		// first request for sync purposes
		string resp = doComRequestResponse(iecSyncingStr);
		size_t pos = resp.find("version", 0);
		if (pos == string::npos) {
			return;
		}

		/* IEC adapter set to controller mode */
		doComRequestResponse(string("++mode 1\r\n"));

		/* IEC adapter target address */
		string strRequest = string("++addr ");
		strRequest.append(agentCom::int2String(iecAddr));
		strRequest.append("\r\n");
		doComRequestResponse(strRequest);
		resp = doComRequestResponse(string("++addr\r\n"));
		pos = resp.find(agentCom::int2String(iecAddr), 0);
		if (pos == string::npos) {
			return;
		}

		/* IEC adapter enable automatic target response handling */
		doComRequestResponse(string("++auto 1\r\n"));
	}
}

/* Do an *IDN? request and return the response */
string agentCom::getIdnResponse(void)
{
	string idn;

	for (int cnt = 5; cnt; cnt--) {
		idn = doComRequestResponse((const string)string(C_IDN_REQ_STR));
		if (idn.length() > 2) {
			break;
		}
		Sleep(100L);
	}
	return trim(idn);
}


// @see: http://xanthium.in/Serial-Port-Programming-using-Win32-API
void agentCom::run(void)
{
	agentComRsp comRsp;

	_running = TRUE;
	while (_running) {
		// clear result buffers
		comRsp.stat = C_COMRSP_FAIL;
		comRsp.data = string();

		try {
			// receive the request
			agentComReq comReq = Concurrency::receive(_src);

			// command decoder
			switch (comReq.cmd) {
			case C_COMREQ_OPEN_ZOLIX: 
			case C_COMREQ_OPEN_IDN:
				{
					int data_port		= 0;
					int data_baud		= 0;
					int data_size		= 0;
					int data_stopbits	= 0;
					int data_parity		= 0;
					int data_iec_addr	= C_SET_IEC_ADDR_INVALID;

					sscanf_s(comReq.parm.c_str(), C_OPENPARAMS_STR,
						&data_port, 
						&data_baud,
						&data_size,
						&data_parity,
						&data_stopbits,
						&data_iec_addr
						);  // COM PORT

					wchar_t cbuf[C_BUF_SIZE];
					wsprintf(cbuf, L"\\\\.\\COM%d%c", data_port, 0);
					_hCom = CreateFile(cbuf,           // port name
						GENERIC_READ | GENERIC_WRITE,  // Read/Write
						0,                             // No Sharing
						NULL,                          // No Security
						OPEN_EXISTING,				   // Open existing port only
						0 /*FILE_FLAG_OVERLAPPED*/,	   // Non Overlapped I/O
						NULL);
					if (_hCom != INVALID_HANDLE_VALUE) {
						DCB dcbSerialParams;
						SecureZeroMemory(&dcbSerialParams, sizeof(DCB));  //  Initialize the DCB structure.
						dcbSerialParams.DCBlength = sizeof(DCB);

						int status = GetCommState(_hCom, &dcbSerialParams);

						dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
						dcbSerialParams.BaudRate = (DWORD)data_baud;	// Setting BaudRate
						dcbSerialParams.ByteSize = (BYTE)data_size;		// Setting ByteSize
						dcbSerialParams.StopBits = (BYTE)data_stopbits;	// Setting StopBits
						dcbSerialParams.Parity = (BYTE)data_parity;		// Setting Parity
						dcbSerialParams.fParity = FALSE;

						/* IEC 625 & serial comm - known to work */
						dcbSerialParams.fOutxCtsFlow = FALSE;
						dcbSerialParams.fOutxDsrFlow = FALSE;
						dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
						dcbSerialParams.fDsrSensitivity = FALSE;
						dcbSerialParams.fTXContinueOnXoff = FALSE;
						dcbSerialParams.fOutX = FALSE;
						dcbSerialParams.fInX = FALSE;
						dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;

						status = SetCommState(_hCom, &dcbSerialParams);
						if (status) {
							COMMTIMEOUTS timeouts = { 0 };
							timeouts.ReadIntervalTimeout			= 1;	// in milliseconds
							timeouts.ReadTotalTimeoutMultiplier		= 1;	// in milliseconds
							timeouts.ReadTotalTimeoutConstant		= 50;	// in milliseconds
							timeouts.WriteTotalTimeoutMultiplier	= 0;	// in milliseconds
							timeouts.WriteTotalTimeoutConstant		= 0;	// in milliseconds
							status = SetCommTimeouts(_hCom, &timeouts);
							if (status) {
								/* Set IEC serial adapter to the defined IEC target address */
								if (data_iec_addr != C_SET_IEC_ADDR_INVALID) {
									iecPrepare(data_iec_addr);
								}

								switch (comReq.cmd) {
								case C_COMREQ_OPEN_ZOLIX:
									comRsp.data = string(getZolixIdn());
									comRsp.stat = C_COMRSP_DATA;
									break;

								case C_COMREQ_OPEN_IDN:
									comRsp.data = string(getIdnResponse());
									comRsp.stat = C_COMRSP_DATA;
									break;
								}
							}
						}
					}
				} 
				break;

			case C_COMREQ_COM_SEND:
				{
					/* Send and receive message */
					string response = doComRequestResponse(comReq.parm);

					if (response.length()) {
						comRsp.data = response;
						comRsp.stat = C_COMRSP_DATA;
					}
				}
				break;

			case C_COMREQ_CLOSE:
				{
					CloseHandle(_hCom);
					_hCom = INVALID_HANDLE_VALUE;
					comRsp.stat = C_COMRSP_OK;
				}
				break;

			case C_COMREQ_END:
				default:
				{
					CloseHandle(_hCom);
					_hCom = INVALID_HANDLE_VALUE;
					comRsp.stat = C_COMRSP_END;
					_running = FALSE;
				}
			}

			// send the response
			Concurrency::send(_tgt, comRsp);
		}
		catch (const Concurrency::operation_timed_out& e) {
			(void)e;
		}
	}

	// Move the agent to the finished state.
	_done = true;
	agent::done();
}


string agentCom::int2String(int val)
{
	char buf[8];

	int len = sprintf_s(buf, "%d", val);
	buf[len] = 0;

	return string(buf);
}

string agentCom::double2String(double val)
{
	char buf[64];

	int len = sprintf_s(buf, "%lf", val);
	buf[len] = 0;

	return string(buf);
}
