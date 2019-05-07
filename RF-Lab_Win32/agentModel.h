#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

#include <map>

#include "agentModel_InstList.h"


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


typedef struct confAttributes
{

	string								attrName;
	string								attrSection;
	string								attrType;

	float								attrTurnLeftMaxDeg = 0.0f;
	float								attrTurnRightMaxDeg = 0.0f;
	uint32_t							attrTicks360Deg = 0UL;
	float								attrSpeedStart = 0.0f;
	float								attrSpeedAccl = 0.0f;
	float								attrSpeedTop = 0.0f;

	float								attrFreqMinHz = 0.0f;
	float								attrFreqMaxHz = 0.0f;
	float								attrFreqInitHz = 0.0f;

	float								attrTXlevelMinDbm = 0.0f;
	float								attrTXlevelMaxDbm = 0.0f;
	float								attrTXlevelInitDbm = 0.0f;

	float								attrSpanMinHz = 0.0f;
	float								attrSpanMaxHz = 0.0f;
	float								attrSpanInitHz = 0.0f;

	float								attrRXLoLevelMinDbm = 0.0f;
	float								attrRXLoLevelMaxDbm = 0.0f;
	float								attrRXLoLevelInitDbm = 0.0f;

	float								attrRXHiLevelMinDbm = 0.0f;
	float								attrRXHiLevelMaxDbm = 0.0f;
	float								attrRXHiLevelInitDbm = 0.0f;

	string								attrComDevice;
	uint16_t							attrComBaud = 0U;
	uint8_t								attrComBits = 0U;
	string								attrComPar;
	uint8_t								attrComStop = 0U;

	uint8_t								attrGpibAddr = 0U;

	string								attrServerType;
	uint16_t							attrServerPort = 0U;

	uint16_t							attrUsbVendorID = 0U;
	uint16_t							attrUsbProductID = 0U;

} confAttributes_t;



int	compare(void *pvlocale, const void *str1, const void *str2);



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
	class WinSrv						*_winSrv;
	HWND								 _hWnd;
	class agentModelVariant				*_curModel;

	char								 _fs_instrument_settings_filename[256];


public:
	explicit agentModel(ISource<agentModelReq_t> *src, ITarget<agentModelRsp_t> *tgt, class WinSrv *winSrv, HWND hWnd, AGENT_MODELS am_variant, AGENT_ALL_SIMUMODE_t mode);
	~agentModel(void);

protected:
	void			run(void);

private:

	void			fsLoadInstruments(const char* filename);
	void			confAttrClear(confAttributes_t* cA);
	void			pushInstrumentDataset(map<string, confAttributes_t>* mapConfig, string name, const confAttributes_t* cA);
	void			scanInstruments(void);
	void			preloadInstruments(void);  // TODO: remove me!

	bool			instCheckUsb(am_InstList_t::iterator it);
	bool			instCheckCom(am_InstList_t::iterator it);
	void			instActivateUsb(am_InstList_t::iterator it);
	void			instActivateCom(am_InstList_t::iterator it);


public:
	/* Default class functions() to be overwritten */
	static bool		isRunning(void);
	static void		Release(void);
	static bool		shutdown(void);
	static void		wmCmd(int wmId, LPVOID arg = nullptr);

	class agentModelVariant* getCurModCtx(void);
	class WinSrv*	getWinSrv(void);


	/* Tools */
	static bool		parseStr2Bool(bool* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Long(long* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Double(double* ret, const char* ary, const char* fmt, char delimRight = 0);
	static unsigned int getLineLength(const char* p, unsigned int len);


	/* agentModelPattern - GENERAL */
	static void		setSimuMode(int simuMode);
	static int		getSimuMode(void);
	static void		sendModelStatus(LPVOID status1, LPVOID status2);
	static void     initDevices(void);
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
