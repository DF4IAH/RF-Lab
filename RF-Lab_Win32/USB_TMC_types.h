#pragma once

#ifndef MIN
#define MIN(a,b) ((a) < (b) ?  (a) : (b))
#endif


#define USB_MAX_TRANSFER_LENGTH			2048
#define AGENT_PATTERN_USBTMC_TIMEOUT	 500
#define TRANSFER_TIMEOUT				1000


typedef enum C_USB_TMC_RUNSTATES_ENUM {
	C_USB_TMC_RUNSTATES_NOOP = 0,
	C_USB_TMC_RUNSTATES_INIT,
	C_USB_TMC_RUNSTATES_RUN,
	C_USB_TMC_RUNSTATES_STOP,
} C_USB_TMC_RUNSTATES_ENUM_t;


typedef enum C_USB_TMC_INSTRUMENT_TYPE {
	C_USB_TMC_INSTRUMENT_NONE = 0,
	C_USB_TMC_INSTRUMENT_TX,
	C_USB_TMC_INSTRUMENT_RX,
} C_USB_TMC_INSTRUMENT_TYPE_t;


/* Some USBTMC-specific enums, as defined in the USBTMC standard. */
//#define LIBUSB_CLASS_APPLICATION		0xFE  --> @see: libusb.h  enum libusb_class_code { ... };
#define SUBCLASS_USBTMC					0x03
#define USBTMC_USB488					0x01

typedef enum USB_CR_ENUM {
	/* USBTMC control requests */
	INITIATE_ABORT_BULK_OUT = 1,
	CHECK_ABORT_BULK_OUT_STATUS = 2,
	INITIATE_ABORT_BULK_IN = 3,
	CHECK_ABORT_BULK_IN_STATUS = 4,
	INITIATE_CLEAR = 5,
	CHECK_CLEAR_STATUS = 6,
	GET_CAPABILITIES = 7,
	INDICATOR_PULSE = 64,

	/* USB488 control requests */
	READ_STATUS_BYTE = 128,
	REN_CONTROL = 160,
	GO_TO_LOCAL = 161,
	LOCAL_LOCKOUT = 162,
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
