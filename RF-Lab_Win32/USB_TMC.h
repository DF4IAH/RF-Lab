#pragma once

/* @see https://sigrok.org/gitweb/?p=libsigrok.git;a=blob;f=src/scpi/scpi_usbtmc_libusb.c */
/* @see https://github.com/python-ivi/python-usbtmc/blob/master/usbtmc/usbtmc.py */


/* Agents Library */
#include <agents.h>
#include "agentCom.h"

/* LibUSB */
#include "libusb.h"
#include "libusbi.h"

#include "instruments.h"
#include "USB_TMC_types.h"


/* Application */

class USB_TMC;
typedef struct threadDataUsbTmc_s {
	int									threadNo;
	USB_TMC*							o;
} threadDataUsbTmc_t;


class USB_TMC : public agent
{
public:
	USB_TMC(unbounded_buffer<agentUsbReq>* pAgtUsbTmcReq, unbounded_buffer<agentUsbRsp>* pAgtUsbTmcRsp);
	virtual ~USB_TMC();

#if 0
	static void procThreadUsbTmc(void* pContext);
#endif

	void start(void);			// Start agent and release the brake in run()
	bool shutdown(void);		// Signal to shutdown the run() thread
	void Release(void);			// Stop agent and shutdown
	void run(void);				// The agent's own run() thread
	bool isDone(void);


private:

	/* Methods */

//	void threadsStart(void);
//	void threadsStop(void);

	int init_libusb(bool show);
	void print_devs_libusb(libusb_device **devs);
	void print_device_cap_libusb(struct libusb_bos_dev_capability_descriptor *dev_cap);
	wchar_t* uuid_to_string_libusb(const uint8_t* uuid);
	void shutdown_libusb(void);
	bool check_usbtmc_blacklist_libusb(struct usbtmc_blacklist *blacklist, uint16_t vid, uint16_t pid);

	int findInstruments(void);
	instrument_t* addInstrument(INSTRUMENT_ENUM_t type, int devs_idx, instrument_t *optionalInst);
	void releaseInstrument_usb_iface(instrument_t inst[], int cnt);
	bool openUsb(instrument_t *inst);
	bool closeUsb(instrument_t *inst);
	bool openTmc(instrument_t *inst);
	bool tmcInitiate(instrument_t *inst);
	bool tmcGoRemote(instrument_t *inst);
	void tmcGoLocal(instrument_t *inst);
	INSTRUMENT_ENUM_t checkIDTable(uint16_t idVendor, uint16_t idProduct);
	INSTRUMENT_ENUM_t usbTmcCheck(int devs_idx, struct libusb_device_descriptor *desc, instrument_t *outInst);
	C_USB_TMC_INSTRUMENT_TYPE_t usbTmcInstType(int devs_idx, instrument_t *outInst);
	bool usbTmcReadLine(instrument_t *inst, char* outLine, int len);
	void usbTmcReadFlush(instrument_t *inst);
	bool usbTmcCmdRST(instrument_t *inst);
	bool usbTmcCmdCLS(instrument_t *inst);
	bool usbTmcCmdSRE(instrument_t *inst, uint16_t bitmask);
	bool usbTmcCmdESE(instrument_t *inst, uint16_t bitmask);
	int  usbTmcGetSTB(instrument_t *inst);
	bool usbTmcGetIDN(instrument_t *inst);

	void usbtmc_bulk_out_header_write(uint8_t header[], uint8_t MsgID, uint8_t bTag, uint32_t TransferSize, uint8_t bmTransferAttributes, char TermChar);
	int usbtmc_bulk_in_header_read(uint8_t header[], uint8_t MsgID, uint8_t bTag, uint32_t *TransferSize, uint8_t *bmTransferAttributes);
	int scpi_usbtmc_bulkout(instrument_t *inst, uint8_t msg_id, const void *data, int32_t size, uint8_t transfer_attributes);
	int scpi_usbtmc_bulkin_start(instrument_t *inst, uint8_t msg_id, uint8_t *data, int32_t size, uint8_t *transfer_attributes);
	int scpi_usbtmc_bulkin_continue(instrument_t *inst, uint8_t *data, int size);
	int scpi_usbtmc_libusb_send(void *priv, const char *command);
	int scpi_usbtmc_libusb_read_begin(void *priv);
	int scpi_usbtmc_libusb_read_data(void *priv, char *buf, int maxlen);
	int scpi_usbtmc_libusb_read_complete(void *priv);


	/* Attributes */

	HANDLE								 hThreadAgtUsbTmc;
	threadDataUsbTmc_t					 sThreadDataUsbTmc;

	const struct libusb_version			*version;
	libusb_device					   **devs;

	/* Server connection */
	unbounded_buffer<agentUsbReq>		*pAgtUsbTmcReq;
	unbounded_buffer<agentUsbRsp>		*pAgtUsbTmcRsp;

	ArrayOfInstruments_t*				 pAI;

	bool								 _isStarted;
	bool								 _running;
	short								 _runState;
	bool								 _isOpen;
	bool								 _done;
};


#if 0
SR_PRIV GSList *sr_scpi_scan(struct drv_context *drvc, GSList *options,
	struct sr_dev_inst *(*probe_device)(struct sr_scpi_dev_inst *scpi));
SR_PRIV struct sr_scpi_dev_inst *scpi_dev_inst_new(struct drv_context *drvc,
	const char *resource, const char *serialcomm);
SR_PRIV int sr_scpi_open(struct sr_scpi_dev_inst *scpi);
SR_PRIV int sr_scpi_source_add(struct sr_session *session,
	struct sr_scpi_dev_inst *scpi, int events, int timeout,
	sr_receive_data_callback cb, void *cb_data);
SR_PRIV int sr_scpi_source_remove(struct sr_session *session,
	struct sr_scpi_dev_inst *scpi);
SR_PRIV int sr_scpi_send(struct sr_scpi_dev_inst *scpi,
	const char *format, ...);
SR_PRIV int sr_scpi_send_variadic(struct sr_scpi_dev_inst *scpi,
	const char *format, va_list args);
SR_PRIV int sr_scpi_read_begin(struct sr_scpi_dev_inst *scpi);
SR_PRIV int sr_scpi_read_data(struct sr_scpi_dev_inst *scpi, char *buf, int maxlen);
SR_PRIV int sr_scpi_write_data(struct sr_scpi_dev_inst *scpi, char *buf, int len);
SR_PRIV int sr_scpi_read_complete(struct sr_scpi_dev_inst *scpi);
SR_PRIV int sr_scpi_close(struct sr_scpi_dev_inst *scpi);
SR_PRIV void sr_scpi_free(struct sr_scpi_dev_inst *scpi);

SR_PRIV int sr_scpi_read_response(struct sr_scpi_dev_inst *scpi,
	GString *response, gint64 abs_timeout_us);
SR_PRIV int sr_scpi_get_string(struct sr_scpi_dev_inst *scpi,
	const char *command, char **scpi_response);
SR_PRIV int sr_scpi_get_bool(struct sr_scpi_dev_inst *scpi,
	const char *command, gboolean *scpi_response);
SR_PRIV int sr_scpi_get_int(struct sr_scpi_dev_inst *scpi,
	const char *command, int *scpi_response);
SR_PRIV int sr_scpi_get_float(struct sr_scpi_dev_inst *scpi,
	const char *command, float *scpi_response);
SR_PRIV int sr_scpi_get_double(struct sr_scpi_dev_inst *scpi,
	const char *command, double *scpi_response);
SR_PRIV int sr_scpi_get_opc(struct sr_scpi_dev_inst *scpi);
SR_PRIV int sr_scpi_get_floatv(struct sr_scpi_dev_inst *scpi,
	const char *command, GArray **scpi_response);
SR_PRIV int sr_scpi_get_uint8v(struct sr_scpi_dev_inst *scpi,
	const char *command, GArray **scpi_response);
SR_PRIV int sr_scpi_get_data(struct sr_scpi_dev_inst *scpi,
	const char *command, GString **scpi_response);
SR_PRIV int sr_scpi_get_block(struct sr_scpi_dev_inst *scpi,
	const char *command, GByteArray **scpi_response);
SR_PRIV int sr_scpi_get_hw_id(struct sr_scpi_dev_inst *scpi,
	struct sr_scpi_hw_info **scpi_response);
SR_PRIV void sr_scpi_hw_info_free(struct sr_scpi_hw_info *hw_info);

SR_PRIV const char *sr_vendor_alias(const char *raw_vendor);
SR_PRIV const char *scpi_cmd_get(const struct scpi_command *cmdtable, int command);
SR_PRIV int scpi_cmd(const struct sr_dev_inst *sdi,
	const struct scpi_command *cmdtable, int command, ...);
SR_PRIV int scpi_cmd_resp(const struct sr_dev_inst *sdi,
	const struct scpi_command *cmdtable,
	GVariant **gvar, const GVariantType *gvtype, int command, ...);

#endif