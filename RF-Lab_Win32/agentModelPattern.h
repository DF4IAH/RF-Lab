#pragma once

#include "agentModel.h"
#include "agentModelVariant.h"

#include "agentCom.h"

using namespace concurrency;
using namespace std;


#define AGENT_PATTERN_RECEIVE_TIMEOUT		 2500

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter RF on/off */
#define AGENT_PATTERN_TX_ON_STATE_DEFAULT	 TRUE

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter frequency 24 GHz */
#define AGENT_PATTERN_TX_FREQ_VALUE_DEFAULT  24e9

/* RX: Rohde & Schwarz SMR40 (signal generator) - transmitter power -20 dBm */
#define AGENT_PATTERN_TX_PWR_VALUE_DEFAULT   -20


/* RX: Rohde & Schwarz FSEK20 (spectrum analyzer) - frequency span 100 kHz */
#define AGENT_PATTERN_RX_SPAN_VALUE_DEFAULT  100e3


enum C_MODELPATTERN_RUNSTATES_ENUM {
	C_MODELPATTERN_RUNSTATES_NOOP = 0,
	C_MODELPATTERN_RUNSTATES_OPENCOM,
	C_MODELPATTERN_RUNSTATES_OPENCOM_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT,
	C_MODELPATTERN_RUNSTATES_INIT_WAIT,
	C_MODELPATTERN_RUNSTATES_INIT_ERROR,
	C_MODELPATTERN_RUNSTATES_RUNNING,
	C_MODELPATTERN_RUNSTATES_GOTO_X,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM,
	C_MODELPATTERN_RUNSTATES_CLOSE_COM_WAIT,
};

enum C_MODELPATTERN_PROCESSES_ENUM {
	C_MODELPATTERN_PROCESS_NOOP = 0,
	C_MODELPATTERN_PROCESS_RECORD_PATTERN_180DEG,
};


class agentModelPattern : public agentModelVariant
{
private:
	unbounded_buffer<agentComReq_t>		*pAgtComReq[C_COMINST__COUNT];
	unbounded_buffer<agentComRsp_t>		*pAgtComRsp[C_COMINST__COUNT];
	agentCom							*pAgtCom[C_COMINST__COUNT];
	LPVOID								 _arg;

	int									 simuMode;

	int									 initState;

	int									 lastTickPos;

	bool								 txOn;
	double								 txFrequency;
	double								 txPower;

	double								 rxFrequency;
	double								 rxSpan;
	double								 rxLevelMax;


public:
	explicit	agentModelPattern(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_ALL_SIMUMODE_t mode);
	void		run(void);

private:
	void		sendPos(int tickPos);

public:
	/* overwriting agentModel member functions() */
	bool		isRunning(void);
	void		Release(void);
	bool		shutdown(void);
	void		wmCmd(int wmId, LPVOID arg = nullptr);

	/* agentModelPattern - GENERAL */
	void		setSimuMode(int simuMode);
	int			getSimuMode(void);
	void		runProcess(int processID);

	/* agentModelPattern - Rotor */
	int			requestPos(void);
	void		setLastTickPos(int pos);
	int			getLastTickPos(void);

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
};
