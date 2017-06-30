#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>


using namespace concurrency;
using namespace std;


typedef enum C_MODELREQ_ENUM {
	C_MODELREQ_END = 0
} C_MODELREQ_ENUM_t;

typedef enum C_MODELRSP_ENUM {
	C_MODELRSP_END = 0
} C_MODELRSP_ENUM_t;

typedef enum AGENT_ALL_SIMUMODE {
	AGENT_ALL_SIMUMODE_NONE = 0x00,
	AGENT_ALL_SIMUMODE_NO_TX = 0x01,
	AGENT_ALL_SIMUMODE_NO_RX = 0x02,
	AGENT_ALL_SIMUMODE_RUN_BARGRAPH = 0x10,
} AGENT_ALL_SIMUMODE_t;


typedef struct agentModelReq
{
	SHORT								 cmd;
	ULONG32								 data;
} agentModelReq_t;

typedef struct agentModelRsp
{
	SHORT								 stat;
	ULONG32								 data;
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
	AGENT_ALL_SIMUMODE					 _simuMode;
	class agentModelVariant				*_curModel;


public:
	explicit agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, AGENT_MODELS am_variant, AGENT_ALL_SIMUMODE_t mode);
	~agentModel(void);

protected:
	void			run(void);

public:
	/* Default class functions() to be overwritten */
	static bool		isRunning(void);
	static void		Release(void);
	static bool		shutdown(void);
	static void		wmCmd(int wmId, LPVOID arg = nullptr);

	/* Tools */
	static bool		parseStr2Bool(bool* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Long(long* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Double(double* ret, const char* ary, const char* fmt, char delimRight = 0);


	/* agentModelPattern - GENERAL */
	static void		setSimuMode(int simuMode);
	static int		getSimuMode(void);
	static void		runProcess(int processID, int arg);

	/* agentModelPattern - Rotor */
	static long		requestPos(void);
	static void		setLastTickPos(long tickPos);
	static long		getLastTickPos(void);

	/* agentModelPattern - TX */
	static void		setTxOnState(bool checked);
	static bool		getTxOnState(void);
	static bool		getTxOnDefault(void);
	static void		setTxFrequencyValue(double value);
	static double	getTxFrequencyValue(void);
	static double	getTxFrequencyDefault(void);
	static void		setTxPwrValue(double value);
	static double	getTxPwrValue(void);
	static double	getTxPwrDefault(void);

	/* agentModelPattern - RX */
	static void		setRxFrequencyValue(double value);
	static double	getRxFrequencyValue(void);
	static void		setRxSpanValue(double value);
	static double	getRxSpanValue(void);
	static double	getRxSpanDefault(void);
	static void		setRxLevelMaxValue(double value);
	static double	getRxLevelMaxValue(void);
};
