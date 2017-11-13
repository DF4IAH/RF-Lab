#pragma once

#include "agentModel.h"
#include "agentModelVariant.h"

#include "agentCom.h"
#include "USB_TMC.h"

using namespace concurrency;
using namespace std;



#define AGENT_PATTERN_RECEIVE_TIMEOUT			  2500

/* ROTOR: Count of ticks to turn 1° right */
#define AGENT_PATTERN_ROT_TICKS_PER_DEGREE		   800

/* ROTOR: Time in ms delay for turning 1° */
#define AGENT_PATTERN_ROT_MS_PER_DEGREE				70

/* ROTOR: Pattern 180° is measured between these to limits */
#define AGENT_PATTERN_180_POS_DEGREE_START		 -90.0
#define AGENT_PATTERN_180_POS_DEGREE_END		  90.0
#define AGENT_PATTERN_180_POS_DEGREE_STEP		   5.0

/* ROTOR: Pattern 360° is measured between these to limits */
#define AGENT_PATTERN_360_POS_DEGREE_START		-180.0
#define AGENT_PATTERN_360_POS_DEGREE_END		 180.0
#define AGENT_PATTERN_360_POS_DEGREE_STEP		   5.0

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter RF on/off */
#define AGENT_PATTERN_TX_ON_STATE_DEFAULT		TRUE

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter frequency 24 GHz */
#define AGENT_PATTERN_TX_FREQ_VALUE_DEFAULT		  24e9

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter power -20 dBm */
#define AGENT_PATTERN_TX_PWR_VALUE_DEFAULT		   -20


/* RX: Rohde & Schwarz FSEK20 (spectrum analyzer) - frequency span 100 kHz */
#define AGENT_PATTERN_RX_SPAN_VALUE_DEFAULT		 100e3

#define C_ROT_COM_PORT								 3
#define C_ROT_COM_BAUD								 CBR_19200
#define C_ROT_COM_BITS								 8
#define C_ROT_COM_PARITY							 NOPARITY
#define C_ROT_COM_STOPBITS							 ONESTOPBIT

#define C_TX_COM_PORT								 1
#define C_TX_COM_BAUD								 CBR_9600
#define C_TX_COM_BITS								 8
#define C_TX_COM_PARITY								 NOPARITY
#define C_TX_COM_STOPBITS							 ONESTOPBIT

#define C_TX_COM_IEC_ADDR							 28
#define C_TX_COM_IEC_PORT							 4
#define C_TX_COM_IEC_BAUD							 CBR_19200
#define C_TX_COM_IEC_BITS							 8
#define C_TX_COM_IEC_PARITY							 NOPARITY
#define C_TX_COM_IEC_STOPBITS						 ONESTOPBIT

#define C_RX_COM_PORT								 1
#define C_RX_COM_BAUD								 CBR_9600
#define C_RX_COM_BITS								 8
#define C_RX_COM_PARITY								 NOPARITY
#define C_RX_COM_STOPBITS							 ONESTOPBIT

#define C_RX_COM_IEC_ADDR							 20
#define C_RX_COM_IEC_PORT							 4
#define C_RX_COM_IEC_BAUD							 CBR_19200
#define C_RX_COM_IEC_BITS							 8
#define C_RX_COM_IEC_PARITY							 NOPARITY
#define C_RX_COM_IEC_STOPBITS						 ONESTOPBIT


/* Instruments */
#include "instruments.h"


enum C_MODELPATTERN_RUNSTATES_ENUM {
	C_MODELPATTERN_RUNSTATES_NOOP = 0,
	C_MODELPATTERN_RUNSTATES_BEGIN,
	C_MODELPATTERN_RUNSTATES_USB_REGISTRATION,
	C_MODELPATTERN_RUNSTATES_OPENCOM,
	C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT,
	C_MODELPATTERN_PROCESSES_SELECTION_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT,
	C_MODELPATTERN_RUNSTATES_INIT_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT_ERROR,
	C_MODELPATTERN_RUNSTATES_REINIT,
	C_MODELPATTERN_RUNSTATES_RUNNING,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT,
};

enum C_MODELPATTERN_PROCESSES_ENUM {
	C_MODELPATTERN_PROCESS_END = 0,
	C_MODELPATTERN_PROCESS_STOP,
	C_MODELPATTERN_PROCESS_NOOP,
	C_MODELPATTERN_PROCESS_GOTO_X,
	C_MODELPATTERN_PROCESS_RECORD_PATTERN_180DEG,
	C_MODELPATTERN_PROCESS_RECORD_PATTERN_360DEG,
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

	class agentModel					*pAgtMod;
	threadDataAgentModelPattern_t		 sThreadDataAgentModelPattern;

	/* All Instruments detected */
	ArrayOfInstruments_t				 ai;

	agentCom							*pAgtCom[C_COMINST__COUNT];
	unbounded_buffer<agentComReq_t>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp_t>		*pAgtComRsp[C_COMINST__COUNT];

	USB_TMC								*pAgtUsbTmc;
	unbounded_buffer<agentUsbReq_t>		*pAgtUsbTmcReq;
	unbounded_buffer<agentUsbRsp_t>		*pAgtUsbTmcRsp;
	HANDLE								 hThreadAgtUsbTmc;

	int									 processing_ID;
	int									 processing_arg1;
	int									 simuMode;

	int									 initState;

	long								 lastTickPos;

	bool								 txOn;
	double								 txFrequency;
	double								 txPower;

	double								 rxFrequency;
	double								 rxSpan;
	double								 rxLevelMax;


public:
	explicit	agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class agentModel *am, AGENT_ALL_SIMUMODE_t mode);
	ArrayOfInstruments_t* getAIPtr(void);
	void		run(void);

private:
	void		agentsInit(void);
	void		agentsShutdown(void);
	void		threadsStart(void);
	void		threadsStop(void);
	void		sendPos(long tickPos);

public:
	/* overwriting agentModel member functions() */
	bool		isRunning(void);
	bool		shutdown(void);
	void		Release(void);
	void		wmCmd(int wmId, LPVOID arg = nullptr);
	static void	procThreadProcessID(void* pContext);

	/* agentModelPattern - GENERAL */
	void		setSimuMode(int simuMode);
	int			getSimuMode(void);
	void		runProcess(int processID, int arg);
	void		initDevices(void);
	void        setStatusPosition(double pos);

	/* agentModelPattern - Rotor */
	long		requestPos(void);
	void		setLastTickPos(long pos);
	long		getLastTickPos(void);

	/* agentModelPattern - TX */
	void		setTxOnState(bool checked);
	bool		getTxOnState(void);
	bool		getTxOnDefault(void);
	void		setTxFrequencyValue(double value);
	double		getTxFrequencyValue(void);
	double		getTxFrequencyDefault(void);
	void		setTxPwrValue(double value);
	double		getTxPwrValue(void);
	double		getTxPwrDefault(void);

	/* agentModelPattern - RX */
	void		setRxFrequencyValue(double value);
	double		getRxFrequencyValue(void);
	void		setRxSpanValue(double value);
	double		getRxSpanValue(void);
	double		getRxSpanDefault(void);
	void		setRxLevelMaxValue(double value);
	double		getRxLevelMaxValue(void);
	bool		getRxMarkerPeak(double* retX, double* retY);

	/* Tools */
	int			runningProcessPattern(double degStartPos, double degEndPos, double degResolution);
 static double	calcTicks2Deg(long ticks);
 static long	calcDeg2Ticks(double deg);
 static DWORD	calcDeg2Ms(double deg);
 static DWORD	calcTicks2Ms(long ticks);
};
