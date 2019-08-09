#pragma once

/* Agents Library */
#include <agents.h>
#include <string>
#include <iostream>
#include <sstream>

#include <map>

#include "instruments.h"


using namespace concurrency;
using namespace std;



// TODO: remove that historic config loader
//#define USE_PRELOAD_INSTRUMENTS



typedef enum C_MODELREQ_ENUM {
	C_MODELREQ_END						= 0
} C_MODELREQ_ENUM_t;

typedef enum C_MODELRSP_ENUM {
	C_MODELRSP_END						= 0
} C_MODELRSP_ENUM_t;

typedef enum AGENT_ALL_SIMUMODE {
	AGENT_ALL_SIMUMODE_NONE				= 0x00,
	AGENT_ALL_SIMUMODE_NO_TX			= 0x01,
	AGENT_ALL_SIMUMODE_NO_RX			= 0x02,
	AGENT_ALL_SIMUMODE_RUN_BARGRAPH		= 0x10,
} AGENT_ALL_SIMUMODE_t;


typedef struct agentModelReq
{
	SHORT								cmd;
	ULONG32								data1;
} agentModelReq_t;

typedef struct agentModelRsp
{
	SHORT								stat;
	ULONG32								data1;
} agentModelRsp_t;


typedef struct confAttributes
{
	string								attrName;
	string								attrSection;
	string								attrType;

	float								attrTurnLeftMaxDeg;
	float								attrTurnRightMaxDeg;
	uint32_t							attrTicks360Deg;
	float								attrSpeedStart;
	float								attrSpeedAccl;
	float								attrSpeedTop;


	float								attrFreqMinHz;
	float								attrFreqMaxHz;
	float								attrFreqInitHz;

	float								attrTXlevelMinDbm;
	float								attrTXlevelMaxDbm;
	float								attrTXlevelInitDbm;

	float								attrSpanMinHz;
	float								attrSpanMaxHz;
	float								attrSpanInitHz;

	float								attrRXLoLevelMinDbm;
	float								attrRXLoLevelMaxDbm;
	float								attrRXLoLevelInitDbm;

	float								attrRXHiLevelMinDbm;
	float								attrRXHiLevelMaxDbm;
	float								attrRXHiLevelInitDbm;

	float								attrVnaNbPointsMin;
	float								attrVnaNbPointsMax;
	float								attrVnaNbPointsInit;


	LinkType_BM_t						attrLinkType;

	/* INTERFACE */
	string								attrServerType;
	uint16_t							attrServerPort;

	/* ETH */
	string								attrEthHostname;
	string								attrEthMAC;
	uint16_t							attrEthPort;

	/* USB */
	uint16_t							attrUsbVendorID;
	uint16_t							attrUsbProductID;

	/* IEC */
	uint8_t								attrGpibAddr;

	/* COM */
	string								attrComDevice;
	uint16_t							attrComBaud;
	uint8_t								attrComBits;
	string								attrComPar;
	uint8_t								attrComStop;

} confAttributes_t;




enum MEASDATA_SETUP_ENUM {

	MEASDATA_SETUP__NONE = 0,

	MEASDATA_SETUP__REFMEAS_GEN_SPEC,
	MEASDATA_SETUP__ROT180_DEG5_GEN_SPEC,
	MEASDATA_SETUP__ROT360_DEG5_GEN_SPEC,

};

typedef struct {

	volatile MEASDATA_SETUP_ENUM			measVar;

	volatile double							txQrg;
	volatile double							txPwr;
	volatile double							rxSpan;

	volatile double							rxRefPwr;

	volatile double							posStep;
	volatile int							entriesCount;
	std::list<double>					   *posDeg;
	std::list<double>					   *rxPwrMag;
	std::list<double>					   *rxPwrPhase;

} MeasData;




int	compare(void *pvlocale, const void *str1, const void *str2);



class agentModel : public agent
{
public:
	enum AGENT_MODELS {
		AGENT_MODEL_NONE				= 0,
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

#ifdef USE_PRELOAD_INSTRUMENTS
	void			preloadInstruments(void);  // Old variant
#endif


public:
	/* Default class functions() to be overwritten */
	static bool		isRunning(void);
	static void		Release(void);
	static bool		shutdown(void);
	static void		wmCmd(int wmId, LPVOID arg = nullptr);

	class agentModelVariant* getCurModCtx(void);
	class WinSrv*	getWinSrv(void);
	HWND            getWnd(void);


	/* Tools */
	static bool		parseStr2Bool(bool* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Long(long* ret, const char* ary, const char* fmt, char delimRight = 0);
	static bool		parseStr2Double(double* ret, const char* ary, const char* fmt, char delimRight = 0);
	static unsigned int getLineLength(const char* p, unsigned int len);


	/* agentModelPattern - GENERAL */
	       void		setupInstrumentList(void);
	static void		setSimuMode(AGENT_ALL_SIMUMODE_t simuMode);
	static AGENT_ALL_SIMUMODE_t	getSimuMode(void);
	static void     getMeasData(MeasData** md);
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
