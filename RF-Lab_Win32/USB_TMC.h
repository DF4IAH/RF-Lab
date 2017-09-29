#pragma once

/* @see https://sigrok.org/gitweb/?p=libsigrok.git;a=blob;f=src/scpi/scpi_usbtmc_libusb.c */
/* @see https://github.com/python-ivi/python-usbtmc/blob/master/usbtmc/usbtmc.py */


#include "libusb.h"
#include "libusbi.h"


#ifndef MIN
#define MIN(a,b) ((a) < (b) ?  (a) : (b))
#endif

#define MAX_TRANSFER_LENGTH				2048
#define TRANSFER_TIMEOUT				1000

/* Some USBTMC-specific enums, as defined in the USBTMC standard. */
//#define LIBUSB_CLASS_APPLICATION		0xFE  --> @see: libusb.h  enum libusb_class_code { ... };
#define SUBCLASS_USBTMC					0x03
#define USBTMC_USB488					0x01

typedef enum USB_CR_ENUM {
	/* USBTMC control requests */
	INITIATE_ABORT_BULK_OUT				=   1,
	CHECK_ABORT_BULK_OUT_STATUS			=   2,
	INITIATE_ABORT_BULK_IN				=   3,
	CHECK_ABORT_BULK_IN_STATUS			=   4,
	INITIATE_CLEAR						=   5,
	CHECK_CLEAR_STATUS					=   6,
	GET_CAPABILITIES					=   7,
	INDICATOR_PULSE						=  64,

	/* USB488 control requests */
	READ_STATUS_BYTE					= 128,
	REN_CONTROL							= 160,
	GO_TO_LOCAL							= 161,
	LOCAL_LOCKOUT						= 162,
} USB_CR_ENUM_t;

/* USBTMC status codes */
#define USBTMC_STATUS_SUCCESS			0x01

/* USBTMC capabilities */
#define USBTMC_INT_CAP_LISTEN_ONLY		0x01
#define USBTMC_INT_CAP_TALK_ONLY		0x02
#define USBTMC_INT_CAP_INDICATOR		0x04

#define USBTMC_DEV_CAP_TERMCHAR			0x01

/* USB488 capabilities byte1 */
#define USB488_DEV_CAP1_TRIG			0x01
#define USB488_DEV_CAP1_RENCON			0x02
#define USB488_DEV_CAP1_IS_488_2		0x04

/* USB488 capabilities byte2 */
#define USB488_DEV_CAP2_DT1				0x01
#define USB488_DEV_CAP2_RL1				0x02
#define USB488_DEV_CAP2_SR1				0x04
#define USB488_DEV_CAP2_SCPI			0x08

/* Bulk messages constants */
#define USBTMC_BULK_HEADER_SIZE			12

/* Bulk MsgID values */
#define DEV_DEP_MSG_OUT					1
#define REQUEST_DEV_DEP_MSG_IN			2
#define DEV_DEP_MSG_IN					2

/* bmTransferAttributes */
#define EOM								0x01
#define TERM_CHAR_ENABLED				0x02


#define SCPI_CMD_IDN "*IDN?"
#define SCPI_CMD_OPC "*OPC?"

typedef enum SCPI_CMD_ENUM {
	SCPI_CMD_SET_TRIGGER_SOURCE = 1,
	SCPI_CMD_SET_TIMEBASE,
	SCPI_CMD_SET_VERTICAL_DIV,
	SCPI_CMD_SET_TRIGGER_SLOPE,
	SCPI_CMD_SET_COUPLING,
	SCPI_CMD_SET_HORIZ_TRIGGERPOS,
	SCPI_CMD_GET_ANALOG_CHAN_STATE,
	SCPI_CMD_GET_DIG_CHAN_STATE,
	SCPI_CMD_GET_TIMEBASE,
	SCPI_CMD_GET_VERTICAL_DIV,
	SCPI_CMD_GET_VERTICAL_OFFSET,
	SCPI_CMD_GET_TRIGGER_SOURCE,
	SCPI_CMD_GET_HORIZ_TRIGGERPOS,
	SCPI_CMD_GET_TRIGGER_SLOPE,
	SCPI_CMD_GET_COUPLING,
	SCPI_CMD_SET_ANALOG_CHAN_STATE,
	SCPI_CMD_SET_DIG_CHAN_STATE,
	SCPI_CMD_GET_DIG_POD_STATE,
	SCPI_CMD_SET_DIG_POD_STATE,
	SCPI_CMD_GET_ANALOG_DATA,
	SCPI_CMD_GET_DIG_DATA,
	SCPI_CMD_GET_SAMPLE_RATE,
	SCPI_CMD_GET_SAMPLE_RATE_LIVE,
	SCPI_CMD_GET_DATA_FORMAT,
	SCPI_CMD_GET_PROBE_FACTOR,
	SCPI_CMD_SET_PROBE_FACTOR,
	SCPI_CMD_GET_PROBE_UNIT,
	SCPI_CMD_SET_PROBE_UNIT,
	SCPI_CMD_GET_ANALOG_CHAN_NAME,
	SCPI_CMD_GET_DIG_CHAN_NAME,
} SCPI_CMD_t;

typedef struct scpi_command {
	int command;
	const char *string;
} scpi_command_t;

typedef struct scpi_hw_info {
	char *manufacturer;
	char *model;
	char *serial_number;
	char *firmware_version;
} scpi_hw_info_t;

typedef struct scpi_dev_inst {
	const char *name;
	const char *prefix;
	int priv_size;
	// GSList *(*scan)(struct drv_context *drvc);  // TODO: implement GSList
	int(*dev_inst_new)(void *priv, struct drv_context *drvc, const char *resource, char **params, const char *serialcomm);
	int(*open)(struct sr_scpi_dev_inst *scpi);
	// int(*source_add)(struct sr_session *session, void *priv, int events, int timeout, receive_data_callback cb, void *cb_data);  // TODO: implement receive_data_callback
	int(*source_remove)(struct sr_session *session, void *priv);
	int(*send)(void *priv, const char *command);
	int(*read_begin)(void *priv);
	int(*read_data)(void *priv, char *buf, int maxlen);
	int(*write_data)(void *priv, char *buf, int len);
	int(*read_complete)(void *priv);
	int(*close)(struct sr_scpi_dev_inst *scpi);
	void(*free)(void *priv);
	unsigned int read_timeout_us;
	void *priv;
	/* Only used for quirk workarounds, notably the Rigol DS1000 series. */
	uint64_t firmware_version;
} scpi_dev_inst_t;

typedef struct usbtmc_blacklist {
	uint16_t	vid;
	uint16_t	pid;
} usbtmc_blacklist_t;

/* Devices that publish RL1 support, but don't support it. */
#define ALL_ZERO	{ 0x000, 0x000 }
static struct usbtmc_blacklist blacklist_remote[] = {
	{ 0x1ab1, 0x0588 }, /* Rigol DS1000 series */
	{ 0x1ab1, 0x04b0 }, /* Rigol DS2000 series */
	{ 0x1ab1, 0x0960 }, /* Rigol DSA875 series */
	{ 0x0957, 0x0588 }, /* Agilent DSO1000 series (rebadged Rigol DS1000) */
	{ 0x0b21, 0xffff }, /* All Yokogawa devices */
	ALL_ZERO
};


typedef enum INSTRUMENT_ENUM {
	INSTRUMENT_NONE						= 0,

	/* Rotors */
	INSTRUMENT_ROTORS__ALL				= 0x10,
	INSTRUMENT_ROTORS_XXX,

	/* Transmitters */
	INSTRUMENT_TRANSMITTERS__ALL		= 0x20,
	INSTRUMENT_TRANSMITTER_FG_XXX,

	/* Receivers */
	INSTRUMENT_RECEIVERS__ALL			= 0x30,
	INSTRUMENT_RECEIVER_SA_RIGOL_DSA875,
} INSTRUMENT_ENUM_t;

typedef struct scpi_usbtmc_libusb {  // TODO: to be removed later
} scpi_usbtmc_libusb_t;

typedef struct instrument {
	INSTRUMENT_ENUM_t		 type;
	int						 devs_idx;

	libusb_device			*dev;
	libusb_device_handle	*dev_handle;
	uint8_t					 dev_config;
	uint8_t					 dev_interface;
	uint8_t					 dev_bulk_out_ep;
	uint8_t					 dev_bulk_in_ep;
	uint8_t					 dev_interrupt_ep;
	uint8_t					 dev_usbtmc_int_cap;
	uint8_t					 dev_usbtmc_dev_cap;
	uint8_t					 dev_usb488_dev_cap1;
	uint8_t					 dev_usb488_dev_cap2;
	uint8_t					 dev_bTag;
	uint8_t					 dev_bulkin_attributes;
	bool					 dev_usb_up;
	bool					 dev_tmc_up;

	int						 response_length;
	int						 response_bytes_read;
	int						 remaining_length;
	uint8_t					 buffer[MAX_TRANSFER_LENGTH];

	struct sr_context		*ctx;						// SCPI USB-TMC code
	struct sr_usb_dev_inst	*usb;						// SCPI USB-TMC code
	int						 detached_kernel_driver;	// SCPI USB-TMC code
} instrument_t;


class USB_TMC
{
public:
	USB_TMC();
	virtual ~USB_TMC();

private:

	/* Methods */

	int init_libusb(bool show);
	void print_devs_libusb(libusb_device **devs);
	void print_device_cap_libusb(struct libusb_bos_dev_capability_descriptor *dev_cap);
	wchar_t* uuid_to_string_libusb(const uint8_t* uuid);
	void shutdown_libusb(void);
	bool check_usbtmc_blacklist_libusb(struct usbtmc_blacklist *blacklist, uint16_t vid, uint16_t pid);

	int findInstruments(void);
	instrument_t* addInstrument(INSTRUMENT_ENUM_t type, int devs_idx);
	void releaseInstrument_usb_iface(instrument_t *inst, int cnt);
	bool openUsb(instrument_t *inst);
	bool openTmc(instrument_t *inst);
	bool tmcInitiate(instrument_t *inst);
	bool tmcGoRemote(instrument_t *inst);
	void tmcGoLocal(instrument_t *inst);
	INSTRUMENT_ENUM_t checkIDTable(uint16_t idVendor, uint16_t idProduct);
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

	const struct libusb_version		   *version;
	libusb_device					  **devs;

	/* ... Instruments */
	int					inst_rot_cnt;
	instrument_t		inst_rot[8];

	int					inst_tx_cnt;
	instrument_t		inst_tx[8];

	int					inst_rx_cnt;
	instrument_t		inst_rx[8];
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