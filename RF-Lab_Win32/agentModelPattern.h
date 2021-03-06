#pragma once

#include "instruments.h"
#include "agentModel.h"
#include "agentModelVariant.h"

#include "agentCom.h"
#include "USB_TMC.h"


using namespace concurrency;
using namespace std;



#define AGENT_PATTERN_RECEIVE_TIMEOUT			  2500


//#ifdef OLD_CODE

/* ROTOR: Count of ticks to turn 1� right */
#define AGENT_PATTERN_ROT_TICKS_PER_DEGREE		   800

/* ROTOR: Time in ms delay for turning 1� */
#define AGENT_PATTERN_ROT_MS_PER_DEGREE				60


/* ROTOR: Reference meassurement on current position */
#define AGENT_PATTERN_000_POS_DEGREE_START		   0.0
#define AGENT_PATTERN_000_POS_DEGREE_END		   0.0

/* ROTOR: Pattern 10� is measured between these to their limits by 5� steps */
#define AGENT_PATTERN_010_POS_DEGREE_START		  -5.0
#define AGENT_PATTERN_010_POS_DEGREE_END		   5.0

/* ROTOR: Pattern 180� is measured between these to their limits by 5� steps */
#define AGENT_PATTERN_180_POS_DEGREE_START		 -90.0
#define AGENT_PATTERN_180_POS_DEGREE_END		  90.0

/* ROTOR: Pattern 360� is measured between these to their limits by 5� steps */
#define AGENT_PATTERN_360_POS_DEGREE_START		-180.0
#define AGENT_PATTERN_360_POS_DEGREE_END		 180.0


/* ROTOR: Step increments in degrees */
#define AGENT_PATTERN_STEP_001					   1.0
#define AGENT_PATTERN_STEP_005					   5.0


#ifdef OLD_CODE

/* Serial communication parameters, as long as they are not stored non-volatile */
/* Zolix: USB-->COM port 3: 19200 baud, 8N1 */
//#define C_ROT_COM_IS_IEC							 false
//#define C_ROT_COM_PORT								 3
//#define C_ROT_COM_BAUD								 CBR_19200
//#define C_ROT_COM_BITS								 8
//#define C_ROT_COM_PARITY							 NOPARITY
//#define C_ROT_COM_STOPBITS							 ONESTOPBIT

#define C_TX_COM_IS_IEC								 true
#define C_TX_COM_PORT								 1
#define C_TX_COM_BAUD								 CBR_9600
#define C_TX_COM_BITS								 8
#define C_TX_COM_PARITY								 NOPARITY
#define C_TX_COM_STOPBITS							 ONESTOPBIT

#if 1
/* R&S SMR40: USB-->COM-->IEC625: address == 28, COM port 4: 19200 baud, 8N1 */
#define C_TX_COM_IEC_ADDR							 28
#define C_TX_COM_IEC_PORT							 4
#define C_TX_COM_IEC_BAUD							 CBR_19200
#define C_TX_COM_IEC_BITS							 8
#define C_TX_COM_IEC_PARITY							 NOPARITY
#define C_TX_COM_IEC_STOPBITS						 ONESTOPBIT

#elif 1
/* R&S SMB100A: USB-->COM-->IEC625: address == 29, COM port 4: 19200 baud, 8N1 */
#define C_TX_COM_IEC_ADDR							 29
#define C_TX_COM_IEC_PORT							 4
#define C_TX_COM_IEC_BAUD							 CBR_19200
#define C_TX_COM_IEC_BITS							 8
#define C_TX_COM_IEC_PARITY							 NOPARITY
#define C_TX_COM_IEC_STOPBITS						 ONESTOPBIT
#else
/* Agilent EXG N5173B: USB-->COM-->IEC625: address == 19, COM port 4: 19200 baud, 8N1 */
#define C_TX_COM_IEC_ADDR							 19
#define C_TX_COM_IEC_PORT							 4
#define C_TX_COM_IEC_BAUD							 CBR_19200
#define C_TX_COM_IEC_BITS							 8
#define C_TX_COM_IEC_PARITY							 NOPARITY
#define C_TX_COM_IEC_STOPBITS						 ONESTOPBIT
#endif


#define C_RX_COM_IS_IEC								 true
#define C_RX_COM_PORT								 1
#define C_RX_COM_BAUD								 CBR_9600
#define C_RX_COM_BITS								 8
#define C_RX_COM_PARITY								 NOPARITY
#define C_RX_COM_STOPBITS							 ONESTOPBIT

#if 1
/* R&S FSEK20: USB-->COM-->IEC625 - address == 20, COM port 4: 19200 baud, 8N1 */
#define C_RX_COM_IEC_ADDR							 20
#define C_RX_COM_IEC_PORT							 4
#define C_RX_COM_IEC_BAUD							 CBR_19200
#define C_RX_COM_IEC_BITS							 8
#define C_RX_COM_IEC_PARITY							 NOPARITY
#define C_RX_COM_IEC_STOPBITS						 ONESTOPBIT

#else
/* */
#define C_RX_COM_IEC_ADDR							 19
#define C_RX_COM_IEC_PORT							 4
#define C_RX_COM_IEC_BAUD							 CBR_19200
#define C_RX_COM_IEC_BITS							 8
#define C_RX_COM_IEC_PARITY							 NOPARITY
#define C_RX_COM_IEC_STOPBITS						 ONESTOPBIT
#endif

#endif  // OLD_CODE


/* Instruments */
#include "instruments.h"


enum C_MODELPATTERN_RUNSTATES_ENUM {

	C_MODELPATTERN_RUNSTATES_NOOP = 0,
	C_MODELPATTERN_RUNSTATES_BEGIN,
	C_MODELPATTERN_RUNSTATES_CHECK_CONNECTIONS,
	C_MODELPATTERN_RUNSTATES_WAIT_FOR_GUI,
	C_MODELPATTERN_RUNSTATES_COM_CONNECT,
	C_MODELPATTERN_RUNSTATES_USB_CONNECT,
	C_MODELPATTERN_RUNSTATES_INST_COM_INIT,
	C_MODELPATTERN_RUNSTATES_INST_USB_INIT,
	C_MODELPATTERN_RUNSTATES_RUNNING,
	C_MODELPATTERN_RUNSTATES_USB_CLOSE_SEND,
	C_MODELPATTERN_RUNSTATES_USB_CLOSE_WAIT,
	C_MODELPATTERN_RUNSTATES_COM_CLOSE_SEND,
	C_MODELPATTERN_RUNSTATES_COM_CLOSE_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT_ERROR,
	C_MODELPATTERN_RUNSTATES_REINIT,
	C_MODELPATTERN_RUNSTATES_SHUTDOWN,

};

enum C_MODELPATTERN_PROCESSES_ENUM {

	C_MODELPATTERN_PROCESS_NOOP = 0,
	C_MODELPATTERN_PROCESS_CONNECT_DEVICES,
	C_MODELPATTERN_PROCESS_END,
	C_MODELPATTERN_PROCESS_STOP,
	C_MODELPATTERN_PROCESS_GOTO_X,
	C_MODELPATTERN_PROCESS_RECORD_REFERENCE,
	C_MODELPATTERN_PROCESS_RECORD_PATTERN,

};



class agentModelPattern;
typedef struct threadDataAgentModelPattern_s {

	int									threadNo;
	agentModelPattern*					o;

} threadDataAgentModelPattern_t;



class agentModelPattern : public agentModelVariant, public agent
{

private:
	LPVOID								 _arg;

	class WinSrv						*_winSrv;
	class agentModel					*_pAgtMod;
	threadDataAgentModelPattern_t		 _sThreadDataAgentModelPattern;

	InstList_t::iterator				 _pConInstruments[C_CONNECTED__COUNT];

	agentCom							*_pAgtCom[C_COMINST__COUNT];
	unbounded_buffer<AgentComReq_t>		*_pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<AgentComRsp_t>		*_pAgtComRsp[C_COMINST__COUNT];

	USB_TMC								*_pAgtUsbTmc;
	unbounded_buffer<AgentUsbReq_t>		*_pAgtUsbTmcReq;
	unbounded_buffer<AgentUsbRsp_t>		*_pAgtUsbTmcRsp;
	HANDLE								 _hThreadAgtUsbTmc;

	MeasData							 _measDataEntries;

	volatile int						 _processing_ID;
	volatile double						 _processing_arg1;
	volatile double						 _processing_arg2;
	volatile double						 _processing_arg3;
	volatile AGENT_ALL_SIMUMODE_t		 _simuMode;

	volatile int						 _initState;
	volatile bool						 _guiPressedConnect;

	volatile long						 _curPosTicksAbs;


public:
	explicit				agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class WinSrv *winSrv, class agentModel *am, AGENT_ALL_SIMUMODE_t mode);
						    ~agentModelPattern();
	void					run(void);

private:
	void					agentsInit(void);
	void					agentsShutdown(void);
	void					threadsStart(void);
	void					threadsStop(void);

	void					checkInstruments(void);
	bool					instTryEth(InstList_t::iterator it);
	bool					instTryUsb(InstList_t::iterator it);
	bool					instTryCom(InstList_t::iterator it);

#ifdef OLD
	instrument_t*			addSerInstrument(INSTRUMENT_ENUM_t type,
								agentCom* pAgtCom, uint8_t comPort, uint32_t comBaud, uint8_t comBits, uint8_t comParity, uint8_t comStopbits, 
								bool isIec, uint8_t iecAddr,
								string idn);
#endif

	void					sendPosTicksAbs(long tickPos);

	MeasData				measDataInit(MEASDATA_SETUP_ENUM measVar, std::list<double> initList);
	void					measDataAdd(MeasData* md, std::list<double> dataList);
	void					measDataFinalize(MeasData* md, MeasData* glob);


public:
	/* overwriting agentModel member functions() */
	bool					isRunning(void);
	bool					shutdown(void);
	void					Release(void);
	void					wmCmd(int wmId, LPVOID arg = nullptr);
	static void				procThreadProcessID(void* pContext);

	/* agentModelPattern - GENERAL */
	void					setSimuMode(AGENT_ALL_SIMUMODE_t _simuMode);
	AGENT_ALL_SIMUMODE_t	getSimuMode(void);
	void					getMeasData(MeasData** md);
	void					runProcess(int processID, double arg1, double arg2, double arg3);
	void					connectDevices(void);
	void					initDevices(void);
	void					perfomInitValues(InstList_t::iterator it);
	void					setStatusPosition(double posDeg);
	void					setStatusRxPower_dBm(double rxPwr);

	/* agentModelPattern - Rotor */
	long					receivePosTicksAbs(void);
	void					setCurPosTicksAbs(long newPosTicksAbs);
	long					getCurPosTicksAbs(void);

	/* agentModelPattern - TX */
	void					setTxOnState(bool checked);
	bool					getTxOnState(void);
	bool					getTxOnDefault(void);
	void					setTxFrequencyValue(double value);
	double					getTxFrequencyValue(void);
	double					getTxFrequencyDefault(void);
	void					setTxPwrValue(double value);
	double					getTxPwrValue(void);
	double					getTxPwrDefault(void);

	/* agentModelPattern - RX */
	void					setRxFrequencyValue(double value);
	double					getRxFrequencyValue(void);
	void					setRxSpanValue(double value);
	double					getRxSpanValue(void);
	double					getRxSpanDefault(void);
	void					setRxLevelMaxValue(double value);
	double					getRxLevelMaxValue(void);
	bool					getRxMarkerPeak(double* retX, double* retY);

	/* Tools */
	int						runningProcessPattern(MEASDATA_SETUP_ENUM measVariant, double degStartPos, double degEndPos, double degResolution);
	static double			calcTicks2Deg(long ticks);
	static long				calcDeg2Ticks(double deg);
	static DWORD			calcDeg2Ms(double deg);
	static DWORD			calcTicks2Ms(long ticks);
 
};
