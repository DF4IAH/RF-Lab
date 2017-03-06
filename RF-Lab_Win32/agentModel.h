#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>


using namespace concurrency;
using namespace std;


enum C_MODELREQ_ENUM {
	C_MODELREQ_END = 0
};

enum C_MODELRSP_ENUM {
	C_MODELRSP_END = 0
};


typedef struct agentModelReq
{
	SHORT								 cmd;
} agentModelReq_t;

typedef struct agentModelRsp
{
	SHORT								 stat;
} agentModelRsp_t;


class agentModel : public agent
{
public:
	enum AGENT_MODELS {
		AGENT_MODEL_NONE = 0,
		AGENT_MODEL_PATTERN,
	};


private:
	ISource<agentModelReq_t>			*_src;
	ITarget<agentModelRsp_t>			*_tgt;
	AGENT_MODELS						 _am_variant;
	class agentModelVariant				*_curModel;


public:
	explicit agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_MODELS am_variant);
	~agentModel(void);

protected:
	void			run(void);

public:
	/* default class functions() to be overwritten */
	static bool		isRunning(void);
	static void		Release(void);
	static bool		shutdown(void);
	static void		wmCmd(int wmId, LPVOID arg = nullptr);

	/* agentModelPattern - Rotor */
	static int		requestPos(void);
	static void		setLastTickPos(int tickPos);
	static int		getLastTickPos(void);

	/* agentModelPattern - TX */
	static void		setTxOnState(bool checked);
	static bool		getTxOnState(void);
	static void		setTxFrequencyValue(double value);
	static double	getTxFrequencyValue(void);
	static void		setTxPwrValue(double value);
	static double	getTxPwrValue(void);

	/* agentModelPattern - RX */
	static void		setRxFrequencyValue(double value);
	static void		setRxSpanValue(double value);
	static void		setRxLevelMaxValue(double value);
	static double	getRxLevelMaxValue(void);
};
