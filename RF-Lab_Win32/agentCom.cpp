#include "stdafx.h"
#include "agentCom.h"


template <class T>  void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		//(*ppT)->Release();
		*ppT = nullptr;
	}
}



agentCom::agentCom(ISource<agentComReq>& src, ITarget<agentComRsp>& tgt)
				 : _running(FALSE)
				 , _done(FALSE)
				 , _src(src)
				 , _tgt(tgt)
{
}


inline bool agentCom::isRunning()
{
	return _running;
}


void agentCom::Release()
{
	// signaling
	if (_running) {
		(void) shutdown();
	}

	// wait until all threads are done
	while (!_done) {
		Sleep(1);
	}

	// release objects
	// ... none
}


bool agentCom::shutdown()
{
	bool old_running = _running;

	// signal to shutdown
	_running = FALSE;

	return old_running;
}

// @see: http://xanthium.in/Serial-Port-Programming-using-Win32-API
void agentCom::run()
{
	agentComRsp comRsp;

	_running = TRUE;
	while (_running) {
		// clear result buffers
		comRsp.stat = C_COMRSP_FAIL;
		comRsp.data = string();

		// receive the request
		agentComReq comReq = receive(_src);

		// command decoder
		switch (comReq.cmd) {
		case C_COMREQ_OPEN: 
		{
			int data_port = 0;
			int data_baud = 0;
			int data_size = 0;
			int data_bits = 0;
			int data_parity = 0;

			sscanf_s(comReq.parm.c_str(), ":P=%d :B=%d :I=%d :A=%d :S=%d",
				&data_port, 
				&data_baud,
				&data_size,
				&data_parity,
				&data_bits
				// , (unsigned) comReq.parm.length()
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
			if (_hCom == INVALID_HANDLE_VALUE)
				comRsp.stat = C_COMRSP_FAIL;

			else {
				DCB dcbSerialParams;
				SecureZeroMemory(&dcbSerialParams, sizeof(DCB));  //  Initialize the DCB structure.
				dcbSerialParams.DCBlength = sizeof(DCB);

				int status = GetCommState(_hCom, &dcbSerialParams);

				dcbSerialParams.DCBlength			= sizeof(dcbSerialParams);
				dcbSerialParams.BaudRate			= (DWORD)data_baud;  // Setting BaudRate
				dcbSerialParams.ByteSize			= (BYTE)data_size;   // Setting ByteSize
				dcbSerialParams.StopBits			= (BYTE)data_bits;   // Setting StopBits
				dcbSerialParams.Parity				= (BYTE)data_parity; // Setting Parity
				dcbSerialParams.fParity				= FALSE;
				dcbSerialParams.fOutxCtsFlow		= FALSE;
				dcbSerialParams.fOutxDsrFlow		= FALSE;
				dcbSerialParams.fDtrControl			= DTR_CONTROL_ENABLE;
				dcbSerialParams.fDsrSensitivity		= FALSE;
				dcbSerialParams.fTXContinueOnXoff	= FALSE;
				dcbSerialParams.fOutX				= TRUE;
				dcbSerialParams.fInX				= TRUE;
				dcbSerialParams.fRtsControl			= RTS_CONTROL_ENABLE;

				status = SetCommState(_hCom, &dcbSerialParams);
				if (!status) {
					comRsp.stat = C_COMRSP_FAIL;
				}
				else {
					COMMTIMEOUTS timeouts = { 0 };
					timeouts.ReadIntervalTimeout		 = 10;		// in milliseconds
					timeouts.ReadTotalTimeoutConstant	 = 10;		// in milliseconds
					timeouts.ReadTotalTimeoutMultiplier  = 1;		// in milliseconds
					timeouts.WriteTotalTimeoutConstant	 = 10;		// in milliseconds
					timeouts.WriteTotalTimeoutMultiplier = 1;		// in milliseconds
					status = SetCommTimeouts(_hCom, &timeouts);
					comRsp.stat = status ?  C_COMRSP_OK : C_COMRSP_FAIL;
				}
			}
		} 
		break;

		case C_COMREQ_COM_SEND:
		case C_COMREQ_COM_SEND_RECEIVE:
		{
			// send parm string via serial port and wait for result
			char lpBuffer[C_BUF_SIZE];
			DWORD dNoOFBytestoWrite;         // No of bytes to write into the port
			DWORD dNoOfBytesWritten = 0;     // No of bytes written to the port

			dNoOFBytestoWrite = snprintf(lpBuffer, C_BUF_SIZE, "%s", comReq.parm.c_str());

			int status = WriteFile(_hCom,	// Handle to the Serial port
				lpBuffer,					// Data to be written to the port
				dNoOFBytestoWrite,			// Number of bytes to write
				&dNoOfBytesWritten,			// Bytes written
				NULL);

			if (!status) {
				comRsp.stat = C_COMRSP_FAIL;

			} else {
				const DWORD dNoOFBytestoRead = C_BUF_SIZE;
				DWORD dNoOfBytesRead = 0;
				char lpBuffer[C_BUF_SIZE];

				status = FlushFileBuffers(_hCom);
				status = ReadFile(_hCom,	// Handle to the Serial port
					lpBuffer,				// Buffer where data from the port is to be written to
					dNoOFBytestoRead,		// Number of bytes to be read
					&dNoOfBytesRead,		// Bytes read
					NULL);

				if (C_COMREQ_COM_SEND == comReq.cmd) {
					// drop result of that command
					comRsp.stat = C_COMRSP_DROP;

				} else if (C_COMREQ_COM_SEND_RECEIVE == comReq.cmd) {
					if (status) {
						comRsp.data = string(lpBuffer, dNoOfBytesRead);
						comRsp.stat = C_COMRSP_DATA;
					}
					else {
						comRsp.stat = C_COMRSP_FAIL;
					}
				}
			}
			

		}
		break;

		case C_COMREQ_CLOSE:
		case C_COMREQ_END:
		default:
		{
			CloseHandle(_hCom);
			_hCom = INVALID_HANDLE_VALUE;
			comRsp.stat = C_COMRSP_END;
			_running = FALSE;
		}
		break;

		}

		if (comRsp.stat != C_COMRSP_DROP) {
			// send the response
			send(_tgt, comRsp);
		}
	}

	// Move the agent to the finished state.
	_done = TRUE;
	done();
}
