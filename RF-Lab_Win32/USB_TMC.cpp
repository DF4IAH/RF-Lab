#include "stdafx.h"
#include "USB_TMC.h"


#define DEBUG_USB 1


inline void* varcpy(void* dest, void* src, size_t size)
{
	memcpy(dest, src, size);
}


USB_TMC::USB_TMC() :
	  devs(NULL)
	, inst_rot_cnt(0)
	, inst_rot()
	, inst_tx_cnt(0)
	, inst_tx()
	, inst_rx_cnt(0)
	, inst_rx()
{
	int cnt = init_libusb(true);

	/* Find USB_TMC connected instruments */
	if (cnt > 0) {
		cnt = findInstruments();
		if (cnt) {
		}
	}
}

USB_TMC::~USB_TMC()
{
	shutdown_libusb();
}


int USB_TMC::init_libusb(bool show)
{
	int r;
	ssize_t cnt = 0;

	version = libusb_get_version();

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int)cnt;

	if (show)
		print_devs_libusb(devs);

	return cnt;
}

void USB_TMC::shutdown_libusb(void)
{
	/* Release all USB attached instruments */
	releaseInstrument_usb_iface(inst_rot, inst_rot_cnt);	inst_rot_cnt = 0;
	releaseInstrument_usb_iface(inst_tx, inst_tx_cnt);		inst_tx_cnt = 0;
	releaseInstrument_usb_iface(inst_rx, inst_rx_cnt);		inst_rx_cnt = 0;

	/* Rotors */
	for (int idx = 0; idx < inst_rot_cnt; idx++)
		if (inst_rot[idx].dev_handle)
			libusb_close(inst_rot[idx].dev_handle);
	inst_rot_cnt = 0;

	/* Transmitters */
	for (int idx = 0; idx < inst_tx_cnt; idx++)
		if (inst_tx[idx].dev_handle)
			libusb_close(inst_tx[idx].dev_handle);
	inst_tx_cnt = 0;

	/* Receivers */
	for (int idx = 0; idx < inst_rx_cnt; idx++)
		if (inst_rx[idx].dev_handle)
			libusb_close(inst_rx[idx].dev_handle);
	inst_rx_cnt = 0;

	/* Release the device list */
	if (devs) {
		libusb_free_device_list(devs, 1);
		devs = NULL;
		libusb_exit(NULL);
	}
}

void USB_TMC::print_devs_libusb(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8];
	wchar_t strbuf[256];

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			wsprintf(strbuf, L"failed to get device descriptor");  OutputDebugString(strbuf);
			return;
		}

		wsprintf(strbuf, L"%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));  OutputDebugString(strbuf);

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			wsprintf(strbuf, L" path: %d", path[0]);  OutputDebugString(strbuf);

			for (j = 1; j < r; j++) {
				wsprintf(strbuf, L".%d", path[j]);  OutputDebugString(strbuf);
			}
		}
		OutputDebugString(L"\n");
	}
}

void USB_TMC::print_device_cap_libusb(struct libusb_bos_dev_capability_descriptor *dev_cap)
{
	wchar_t strbuf[256];

	switch (dev_cap->bDevCapabilityType) {
	case LIBUSB_BT_USB_2_0_EXTENSION: {
		struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext = NULL;
		libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_ext);
		if (usb_2_0_ext) {
			wsprintf(strbuf, L"    USB 2.0 extension:\n");  OutputDebugString(strbuf);
			wsprintf(strbuf, L"      attributes             : %02X\n", usb_2_0_ext->bmAttributes);  OutputDebugString(strbuf);
			libusb_free_usb_2_0_extension_descriptor(usb_2_0_ext);
		}
		break;
	}
	case LIBUSB_BT_SS_USB_DEVICE_CAPABILITY: {
		struct libusb_ss_usb_device_capability_descriptor *ss_usb_device_cap = NULL;
		libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_usb_device_cap);
		if (ss_usb_device_cap) {
			wsprintf(strbuf, L"    USB 3.0 capabilities:\n");  OutputDebugString(strbuf);
			wsprintf(strbuf, L"      attributes             : %02X\n", ss_usb_device_cap->bmAttributes);  OutputDebugString(strbuf);
			wsprintf(strbuf, L"      supported speeds       : %04X\n", ss_usb_device_cap->wSpeedSupported);  OutputDebugString(strbuf);
			wsprintf(strbuf, L"      supported functionality: %02X\n", ss_usb_device_cap->bFunctionalitySupport);  OutputDebugString(strbuf);
			libusb_free_ss_usb_device_capability_descriptor(ss_usb_device_cap);
		}
		break;
	}
	case LIBUSB_BT_CONTAINER_ID: {
		struct libusb_container_id_descriptor *container_id = NULL;
		libusb_get_container_id_descriptor(NULL, dev_cap, &container_id);
		if (container_id) {
			wsprintf(strbuf, L"    Container ID:\n      %s\n", uuid_to_string_libusb(container_id->ContainerID));  OutputDebugString(strbuf);
			libusb_free_container_id_descriptor(container_id);
		}
		break;
	}
	default:
		wsprintf(strbuf, L"    Unknown BOS device capability %02x:\n", dev_cap->bDevCapabilityType);  OutputDebugString(strbuf);
	}
}

wchar_t* USB_TMC::uuid_to_string_libusb(const uint8_t* uuid)
{
	static wchar_t uuid_string[40];

	if (!uuid)
		return NULL;

	wsprintf(uuid_string, L"{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5],
		uuid[6], uuid[7],
		uuid[8], uuid[9],
		uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	return uuid_string;
}

void USB_TMC::usbtmc_bulk_out_header_write(	uint8_t header[], uint8_t MsgID,
													uint8_t bTag,
													uint32_t TransferSize,
													uint8_t bmTransferAttributes,
													char TermChar)
{
	uint8_t		bTagN	= ~bTag;
	uint8_t		u8_0	= 0;
	uint16_t	u16_0	= 0;

	memcpy(header +  0, &MsgID,					sizeof(MsgID));
	memcpy(header +  1, &bTag,					sizeof(bTag));
	memcpy(header +  2, &bTagN,					sizeof(bTagN));
	memcpy(header +  3, &u8_0,					sizeof(u8_0));
	memcpy(header +  4, &TransferSize,			sizeof(TransferSize));
	memcpy(header +  8, &bmTransferAttributes,	sizeof(bmTransferAttributes));
	memcpy(header +  9, &TermChar,				sizeof(TermChar));
	memcpy(header + 10, &u16_0,					sizeof(u16_0));
}

bool USB_TMC::usbtmc_bulk_in_header_read(	uint8_t header[], uint8_t MsgID,
													uint8_t bTag,
													uint32_t *TransferSize,
													uint8_t *bmTransferAttributes)
{
	if (header[0]	!= MsgID ||
		header[1]	!= bTag ||
		header[2]	!= (unsigned char)~bTag)
			return false;

	if (TransferSize)
		*TransferSize =  *((uint32_t*)(header + 4));

	if (bmTransferAttributes)
		*bmTransferAttributes = header[8];

	return true;
}

bool USB_TMC::scpi_usbtmc_bulkout(instrument_t *inst, uint8_t msg_id, const void *data, int32_t size, uint8_t transfer_attributes)
{
	//struct scpi_usbtmc_libusb *uscpi = inst->uscpi;
	int		padded_size, r, transferred;
	wchar_t	strbuf[256];

	if (data && (size + USBTMC_BULK_HEADER_SIZE + 3) > (int)sizeof(inst->buffer)) {
		OutputDebugString(L"USBTMC bulk out transfer is too big.\n");
		return false;
	}

	inst->dev_bTag++;
	inst->dev_bTag += !inst->dev_bTag;	// bTag == 0 is invalid so avoid it.

	usbtmc_bulk_out_header_write(inst->buffer, msg_id, inst->dev_bTag, size, transfer_attributes, 0);
	if (data)
		memcpy(inst->buffer + USBTMC_BULK_HEADER_SIZE, data, size);
	else
		size = 0;
	size += USBTMC_BULK_HEADER_SIZE;
	padded_size = (size + 3) & ~0x3;
	memset(inst->buffer + size, 0, padded_size - size);

	r = libusb_bulk_transfer(inst->dev_handle, inst->dev_bulk_out_ep, inst->buffer, padded_size, &transferred, TRANSFER_TIMEOUT);
	if (r < 0) {
		wsprintf(strbuf, L"USBTMC bulk out transfer error: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
		return false;
	}

	if (transferred < padded_size) {
		wsprintf(strbuf, L"USBTMC bulk out partial transfer (%d/%d bytes).\n", transferred, padded_size);  OutputDebugString(strbuf);
		return false;
	}

	return transferred == USBTMC_BULK_HEADER_SIZE;
}

int USB_TMC::scpi_usbtmc_bulkin_start(instrument_t *inst, uint8_t msg_id, uint8_t *data, int32_t size, uint8_t *transfer_attributes)
{
	int r, transferred = 0;
	uint32_t message_size = 0;
	wchar_t	strbuf[256];

	r = libusb_bulk_transfer(inst->dev_handle, inst->dev_bulk_in_ep, data, size, &transferred, TRANSFER_TIMEOUT);
	if (r < 0) {
		wsprintf(strbuf, L"USBTMC bulk in transfer error: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
		return false;
	}

	if (!usbtmc_bulk_in_header_read(data, msg_id, inst->dev_bTag, &message_size, transfer_attributes)) {
		OutputDebugString(L"USBTMC invalid bulk in header.\n");
		return false;
	}

	message_size += USBTMC_BULK_HEADER_SIZE;
	inst->response_length = MIN(transferred, message_size);
	inst->response_bytes_read = USBTMC_BULK_HEADER_SIZE;
	inst->remaining_length = message_size - inst->response_length;

	return transferred == USBTMC_BULK_HEADER_SIZE;
}

int USB_TMC::scpi_usbtmc_bulkin_continue(instrument_t *inst, uint8_t *data, int size)
{
	int r, transferred = 0;
	wchar_t	strbuf[256];

	r = libusb_bulk_transfer(inst->dev_handle, inst->dev_bulk_in_ep, data, size, &transferred, TRANSFER_TIMEOUT);
	if (r < 0) {
		wsprintf(strbuf, L"USBTMC bulk in transfer error: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
		return false;
	}

	inst->response_length = MIN(transferred, inst->remaining_length);
	inst->response_bytes_read = 0;
	inst->remaining_length -= inst->response_length;

	return transferred;
}

#ifdef NEW
int USB_TMC::scpi_usbtmc_libusb_send(void *priv, const char *command)
542 {
	543         struct scpi_usbtmc_libusb *uscpi = priv;
	544
		545         if (scpi_usbtmc_bulkout(uscpi, DEV_DEP_MSG_OUT,
			546                                 command, strlen(command), EOM) <= 0)
		547                 return SR_ERR;
	548
		549         sr_spew("Successfully sent SCPI command: '%s'.", command);
	550
		551         return SR_OK;
	552 }

int USB_TMC::scpi_usbtmc_libusb_read_begin(void *priv)
555 {
	556         struct scpi_usbtmc_libusb *uscpi = priv;
	557
		558         uscpi->remaining_length = 0;
	559
		560         if (scpi_usbtmc_bulkout(uscpi, REQUEST_DEV_DEP_MSG_IN,
			561             NULL, INT32_MAX, 0) < 0)
		562                 return SR_ERR;
	563         if (scpi_usbtmc_bulkin_start(uscpi, DEV_DEP_MSG_IN,
		564                                      uscpi->buffer, sizeof(uscpi->buffer),
		565 & uscpi->bulkin_attributes) < 0)
		566                 return SR_ERR;
	567
		568         return SR_OK;
	569 }

int USB_TMC::scpi_usbtmc_libusb_read_data(void *priv, char *buf, int maxlen)
572 {
	573         struct scpi_usbtmc_libusb *uscpi = priv;
	574         int read_length;
	575
		576         if (uscpi->response_bytes_read >= uscpi->response_length) {
		577                 if (uscpi->remaining_length > 0) {
			578                         if (scpi_usbtmc_bulkin_continue(uscpi, uscpi->buffer,
				579                                                         sizeof(uscpi->buffer)) <= 0)
				580                                 return SR_ERR;
			581
		}
		else {
			582                         if (uscpi->bulkin_attributes & EOM)
				583                                 return SR_ERR;
			584                         if (scpi_usbtmc_libusb_read_begin(uscpi) < 0)
				585                                 return SR_ERR;
			586
		}
		587
	}
	588
		589         read_length = MIN(uscpi->response_length - uscpi->response_bytes_read, maxlen);
	590
		591         memcpy(buf, uscpi->buffer + uscpi->response_bytes_read, read_length);
	592
		593         uscpi->response_bytes_read += read_length;
	594
		595         return read_length;
	596 }

static int USB_TMC::scpi_usbtmc_libusb_read_complete(void *priv)
{
	600         struct scpi_usbtmc_libusb *uscpi = priv;
	601         return uscpi->response_bytes_read >= uscpi->response_length &&
		602                uscpi->remaining_length <= 0 &&
		603                uscpi->bulkin_attributes & EOM;
}
#endif

bool USB_TMC::check_usbtmc_blacklist_libusb(struct usbtmc_blacklist *blacklist, uint16_t vid, uint16_t pid)
{
	for (int i = 0; blacklist[i].vid; i++) {
		if ((blacklist[i].vid == vid && blacklist[i].pid == 0xFFFF) ||
			(blacklist[i].vid == vid && blacklist[i].pid == pid))
			return true;
	}
	return false;
}


int USB_TMC::findInstruments(void)
{
	int cnt = 0;
	libusb_device *dev;
	int idx = 0;

	/* Iterate over all instruments */
	while ((dev = devs[idx++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (!r) {
			INSTRUMENT_ENUM_t type = checkIDTable(desc.idVendor, desc.idProduct);
			if (type) {
				instrument_t *inst = addInstrument(type, idx - 1);
				cnt++;
				bool suc = openUsb(inst);
				// TODO: inform GUI about the ready device
#ifndef NORMAL
				// TODO: take that out
				if (suc) {
					suc = openTmc(inst);
				}
#endif
			}
		}
	}
	return cnt;
}

instrument_t* USB_TMC::addInstrument(INSTRUMENT_ENUM_t type, int devs_idx)
{
	instrument_t *ret = NULL;

	if (type > INSTRUMENT_ROTORS__ALL) {
		if (type < INSTRUMENT_TRANSMITTERS__ALL) {
			/* Rotors */
			if (inst_rot_cnt < (sizeof(inst_rot) / sizeof(instrument_t))) {
				inst_rot[inst_rot_cnt].type = type;
				inst_rot[inst_rot_cnt].devs_idx = devs_idx;
				ret = &(inst_rot[inst_rot_cnt]);
				inst_rot_cnt++;
			}
		}

		else if (type < INSTRUMENT_RECEIVERS__ALL) {
			/* Transmitters */
			if (inst_tx_cnt < (sizeof(inst_tx) / sizeof(instrument_t))) {
				inst_tx[inst_tx_cnt].type = type;
				inst_tx[inst_tx_cnt].devs_idx = devs_idx;
				ret = &(inst_tx[inst_rot_cnt]);
				inst_tx_cnt++;
			}

		} else {
			/* Receivers */
			if (inst_rx_cnt < (sizeof(inst_rx) / sizeof(instrument_t))) {
				inst_rx[inst_rx_cnt].type = type;
				inst_rx[inst_rx_cnt].devs_idx = devs_idx;
				ret = &(inst_rx[inst_rot_cnt]);
				inst_rx_cnt++;
			}
		}
	}
	return ret;
}

void USB_TMC::releaseInstrument_usb_iface(instrument_t *inst, int cnt)
{
	if (cnt) {
		wchar_t strbuf[256];

		for (int idx = 0; idx < cnt; idx++) {
			/* Releasing the interfaces */
			wsprintf(strbuf, L"Releasing interface %d...\n", inst[idx].dev_interface);  OutputDebugString(strbuf);
			libusb_release_interface(inst[idx].dev_handle, inst[idx].dev_interface);
		}
	}
}

bool USB_TMC::openUsb(instrument_t *inst)
{
	libusb_device							*dev		= devs[inst->devs_idx];
	libusb_device_handle					*dev_handle	= NULL;
	libusb_device_descriptor				 dev_desc;
	libusb_bos_descriptor					*bos_desc	= NULL;
	libusb_config_descriptor				*conf_desc	= NULL;
	uint8_t									 nb_ifaces	= 0;
	const struct libusb_endpoint_descriptor	*endpoint;
	uint8_t									 capabilities[24];
	int										 r;
	uint8_t									 string_index[3];	// indexes of the string descriptors
	wchar_t									 strbuf[256];

	/* Sanity check */
	if (!inst) {
		return false;
	}

	//devHandle = libusb_open_device_with_vid_pid(NULL, dev->device_descriptor.idVendor, dev->device_descriptor.idProduct);
	libusb_open(dev, &dev_handle);
	if (dev_handle) {
		inst->dev_handle	= dev_handle;
		inst->dev			= dev;

		/* Show the port path */
#ifdef DEBUG_USB
		{
			uint8_t port_path[8];
			uint8_t bus = libusb_get_bus_number(dev);
			int cnt = libusb_get_port_numbers(dev, port_path, sizeof(port_path));
			if (cnt > 0) {
				OutputDebugString(L"\nDevice properties:\n");
				wsprintf(strbuf, L"        bus number: %d\n", bus);  OutputDebugString(strbuf);
				wsprintf(strbuf, L"         port path: %d", port_path[0]);  OutputDebugString(strbuf);
				for (int i = 1; i < cnt; i++) {
					wsprintf(strbuf, L"->%d", port_path[i]);  OutputDebugString(strbuf);
				}
				OutputDebugString(L" (from root hub)\n");
			}
		}
#endif

		/* Read device descriptor */
		libusb_get_device_descriptor(dev, &dev_desc);
		// Copy the string descriptors for easier parsing
		string_index[0] = dev_desc.iManufacturer;
		string_index[1] = dev_desc.iProduct;
		string_index[2] = dev_desc.iSerialNumber;

#ifdef DEBUG_USB
		/* Read BOS descriptor if it exists */
		r = libusb_get_bos_descriptor(dev_handle, &bos_desc);
		if (r == LIBUSB_SUCCESS) {
			wsprintf(strbuf, L"%d caps\n", bos_desc->bNumDeviceCaps);  OutputDebugString(strbuf);

			for (int i = 0; i < bos_desc->bNumDeviceCaps; i++) {
				print_device_cap_libusb(bos_desc->dev_capability[i]);
			}

			libusb_free_bos_descriptor(bos_desc);
		}
#endif

		/* Read configuration descriptors */
#ifdef DEBUG_USB
		OutputDebugString(L"\nReading descriptor of the first configuration:\n");
#endif
		r = libusb_get_config_descriptor(dev, 0, &conf_desc);
		if (r == LIBUSB_SUCCESS) {
			nb_ifaces = conf_desc->bNumInterfaces;
#ifdef DEBUG_USB
			wsprintf(strbuf, L"             nb interfaces: %d\n", nb_ifaces);  OutputDebugString(strbuf);
#endif

			for (int idx_iface = 0; idx_iface < nb_ifaces; idx_iface++) {
#ifdef DEBUG_USB
				wsprintf(strbuf, L"              interface[%d]: id = %d\n", idx_iface, conf_desc->lu_interface[idx_iface].altsetting[0].bInterfaceNumber);  OutputDebugString(strbuf);
#endif
				for (int idx_alt = 0; idx_alt < conf_desc->lu_interface[idx_iface].num_altsetting; idx_alt++) {
#ifdef DEBUG_USB
					wsprintf(strbuf, L"interface[%d].altsetting[%d]: num endpoints = %d\n",	idx_iface, idx_alt, conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bNumEndpoints);  OutputDebugString(strbuf);
					wsprintf(strbuf, L"   Class.SubClass.Protocol: %02X.%02X.%02X\n",
						conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceClass,
						conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceSubClass,
						conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceProtocol);  OutputDebugString(strbuf);
#endif

					/* Skip any not USB_TMC conforming interfaces */
					if (conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceClass	!= LIBUSB_CLASS_APPLICATION	|| 
						conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceSubClass	!= SUBCLASS_USBTMC			||
						conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceProtocol != USBTMC_USB488)
						continue;

					inst->dev_config	= conf_desc->bConfigurationValue;
					inst->dev_interface = conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bInterfaceNumber;

					for (int idx_ep = 0; idx_ep < conf_desc->lu_interface[idx_iface].altsetting[idx_alt].bNumEndpoints; idx_ep++) {
						struct libusb_ss_endpoint_companion_descriptor *ep_comp = NULL;

						endpoint = &conf_desc->lu_interface[idx_iface].altsetting[idx_alt].endpoint[idx_ep];
#ifdef DEBUG_USB
						wsprintf(strbuf, L"       endpoint[%d].address: %02X\n", idx_ep, endpoint->bEndpointAddress);  OutputDebugString(strbuf);
#endif

						if (endpoint->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK && !(endpoint->bEndpointAddress & (LIBUSB_ENDPOINT_DIR_MASK))) {
							inst->dev_bulk_out_ep = endpoint->bEndpointAddress;
#ifdef DEBUG_USB
							wsprintf(strbuf, L"Bulk OUT  EP %d", inst->dev_bulk_out_ep);  OutputDebugString(strbuf);
#endif
						}
						if (endpoint->bmAttributes == LIBUSB_TRANSFER_TYPE_BULK && endpoint->bEndpointAddress & (LIBUSB_ENDPOINT_DIR_MASK)) {
							inst->dev_bulk_in_ep = endpoint->bEndpointAddress;
#ifdef DEBUG_USB
							wsprintf(strbuf, L"Bulk IN   EP %d", inst->dev_bulk_in_ep & 0x7f);  OutputDebugString(strbuf);
#endif
						}
						if (endpoint->bmAttributes == LIBUSB_TRANSFER_TYPE_INTERRUPT && endpoint->bEndpointAddress & (LIBUSB_ENDPOINT_DIR_MASK)) {
							inst->dev_interrupt_ep = endpoint->bEndpointAddress;
#ifdef DEBUG_USB
							wsprintf(strbuf, L"Interrupt EP %d", inst->dev_interrupt_ep & 0x7f);  OutputDebugString(strbuf);
#endif
						}

#ifdef DEBUG_USB
						wsprintf(strbuf, L"           max packet size: %04X\n", endpoint->wMaxPacketSize);  OutputDebugString(strbuf);
						wsprintf(strbuf, L"          polling interval: %02X\n", endpoint->bInterval);  OutputDebugString(strbuf);
#endif

#ifdef DEBUG_USB
						libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
						if (ep_comp) {
							wsprintf(strbuf, L"                 max burst: %02X   (USB 3.0)\n", ep_comp->bMaxBurst);  OutputDebugString(strbuf);
							wsprintf(strbuf, L"        bytes per interval: %04X (USB 3.0)\n", ep_comp->wBytesPerInterval);  OutputDebugString(strbuf);
							libusb_free_ss_endpoint_companion_descriptor(ep_comp);
						}
#endif
					}
				}
			}
		}
		libusb_free_config_descriptor(conf_desc);

		/* Claiming the interfaces and allow kernel to detach when needed */
		libusb_set_auto_detach_kernel_driver(dev_handle, 1);

		/* Set the right configuration */
		{
			int current_config = 0;

			if (libusb_get_configuration(inst->dev_handle, &current_config) == 0 && current_config != inst->dev_config) {
				if ((r = libusb_set_configuration(inst->dev_handle, inst->dev_config)) < 0) {
#ifdef DEBUG_USB
					wsprintf(strbuf, L"Failed to set configuration: %s.", libusb_error_name(r));  OutputDebugString(strbuf);
#endif
					return false;
				}
			}
		}

		/* Claiming the interface we need */
		if ((r = libusb_claim_interface(inst->dev_handle, inst->dev_interface)) < 0) {
#ifdef DEBUG_USB
			wsprintf(strbuf, L"Failed to claim interface: %s.", libusb_error_name(r));  OutputDebugString(strbuf);
#endif
			return false;
		}

#ifdef DEBUG_USB
		/* Reading the string descriptors */
		{
			char string[128];

			OutputDebugString(L"\nReading string descriptors:\n");
			for (int i = 0; i < sizeof(string_index); i++) {
				if (string_index[i] == 0) {
					continue;
				}
				if (libusb_get_string_descriptor_ascii(dev_handle, string_index[i], (unsigned char*)string, sizeof(string)) >= 0) {
					wsprintf(strbuf, L"   String (0x%02X): \"%s\"\n", string_index[i], string);  OutputDebugString(strbuf);
				}
			}
			OutputDebugString(L"\n");
		}
#endif

#ifdef DEBUG_USB
		/* Get capabilities of the USB_TMC interface - @see https://en.wikipedia.org/wiki/IEEE-488#Capabilities */
		r = libusb_control_transfer(inst->dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
				GET_CAPABILITIES, 0, inst->dev_interface,
				capabilities, sizeof(capabilities), TRANSFER_TIMEOUT);
		if (r == sizeof(capabilities)) {
			inst->dev_usbtmc_int_cap = capabilities[4];
			inst->dev_usbtmc_dev_cap = capabilities[5];
			inst->dev_usb488_dev_cap1 = capabilities[14];
			inst->dev_usb488_dev_cap2 = capabilities[15];
		}

		wsprintf(strbuf, L"USBTMC device capabilities: TermChar %s, L%s, T%s\n",
			inst->dev_usbtmc_dev_cap & USBTMC_DEV_CAP_TERMCHAR		? L"YES"	: L"NO",
			inst->dev_usbtmc_int_cap & USBTMC_INT_CAP_LISTEN_ONLY	? L"3"		:
			 inst->dev_usbtmc_int_cap & USBTMC_INT_CAP_TALK_ONLY	? L"-"		: L"4",
			inst->dev_usbtmc_int_cap & USBTMC_INT_CAP_TALK_ONLY		? L"5"		:
			 inst->dev_usbtmc_int_cap & USBTMC_INT_CAP_LISTEN_ONLY	? L"-"		: L"6");  OutputDebugString(strbuf);

		wsprintf(strbuf, L"Device capabilities1: TRIGGER %s, REN_CON %s, USB488.2 %s\n",
			inst->dev_usb488_dev_cap1 & USB488_DEV_CAP1_TRIG		?  L"YES"	: L"NO",
			inst->dev_usb488_dev_cap1 & USB488_DEV_CAP1_RENCON		?  L"YES"	: L"NO",
			inst->dev_usb488_dev_cap1 & USB488_DEV_CAP1_IS_488_2	?  L"YES"	: L"NO");  OutputDebugString(strbuf);

		wsprintf(strbuf, L"Device capabilities2: SCPI %s, SR%s, RL%s, DT%s\n\n",
			inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_SCPI		?  L"YES"	: L"NO",
			inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_SR1			?  L"1"		: L"0",
			inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_RL1			?  L"1"		: L"0",
			inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_DT1			?  L"1"		: L"0");  OutputDebugString(strbuf);
#endif

		/* Instrument USB port is up and running */
		inst->dev_usb_up = true;
		return inst->dev_usb_up;
	}

	/* Got no device handle */
	return false;
}

bool USB_TMC::openTmc(instrument_t *inst)
{
	/* Initiate instrument */
	bool r = tmcInitiate(inst);

	/* Switch instrument from local to remote */
	r = tmcGoRemote(inst);

	

	/* Instrument TMC connection is up and running */
	inst->dev_tmc_up = true;
	return inst->dev_tmc_up;
}

bool USB_TMC::tmcInitiate(instrument_t *inst)
{
	int									 r;
	uint8_t								 status;
	wchar_t								 strbuf[256];
	bool								 ret = true;

	r = libusb_control_transfer(inst->dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		INITIATE_CLEAR, 0, inst->dev_interface,
		&status, 1, TRANSFER_TIMEOUT);
	if (r < 0 || status != USBTMC_STATUS_SUCCESS) {
#ifdef DEBUG_USB
		if (r < 0) {
			wsprintf(strbuf, L"Failed to INITIATE_CLEAR: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
		}
		else {
			wsprintf(strbuf, L"Failed to INITIATE_CLEAR: USBTMC status %d.\n", status);  OutputDebugString(strbuf);
		}
#endif
		ret = false;
	}

	return ret;
}

bool USB_TMC::tmcGoRemote(instrument_t *inst)
{
	struct libusb_device_descriptor		 des;
	uint8_t								 status;
	int									 r;
	wchar_t								 strbuf[256];
	bool								 ret = true;

	/* Sanity check */
	if (!inst)
		return false;

	/* No cap. to toggle between local and remote - accepting already */
	if (!(inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_RL1))
		return true;

	do {
		//dev = libusb_get_device(inst->dev_handle);
		libusb_get_device_descriptor(inst->dev, &des);

		/* Check if REN_REMOTE and friends are supported */
		if (!(inst->dev_usb488_dev_cap1 & USB488_DEV_CAP1_RENCON)) {
			ret = true;
			break;
		}

		/* Blacklisted devices are not able to handle the go remote request */
		if (check_usbtmc_blacklist_libusb(blacklist_remote, des.idVendor, des.idProduct)) {
			ret = true;
			break;
		}

#ifdef DEBUG_USB
		OutputDebugString(L"Locking out local control.\n");
#endif

		/* The device allows REN_CONTROL commands */
		r = libusb_control_transfer(inst->dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			REN_CONTROL, 1, inst->dev_interface,
			&status, 1, TRANSFER_TIMEOUT);
		if (r < 0 || status != USBTMC_STATUS_SUCCESS) {
#ifdef DEBUG_USB
			if (r < 0) {
				wsprintf(strbuf, L"Failed to enter REN state: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
			}
			else {
				wsprintf(strbuf, L"Failed to enter REN state: USBTMC status %d.\n", status);  OutputDebugString(strbuf);
			}
#endif
			ret = false;
			break;
		}

		/* The device allows REN_CONTROL commands */
		r = libusb_control_transfer(inst->dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			LOCAL_LOCKOUT, 0, inst->dev_interface,
			&status, 1, TRANSFER_TIMEOUT);
		if (r < 0 || status != USBTMC_STATUS_SUCCESS) {
#ifdef DEBUG_USB
			if (r < 0) {
				wsprintf(strbuf, L"Failed to enter local lockout state: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
			}
			else {
				wsprintf(strbuf, L"Failed to enter local lockout state: USBTMC status %d.\n", status);  OutputDebugString(strbuf);
			}
#endif
			ret = false;
			break;
		}
	} while (false);

	return ret;
}

void USB_TMC::tmcGoLocal(instrument_t *inst)
{
	/* Sanity check */
	if (!inst)
		return;

	{
		struct libusb_device_descriptor	des;
		int								r;
		uint8_t							status;
		wchar_t							strbuf[256];

		if (!(inst->dev_usb488_dev_cap2 & USB488_DEV_CAP2_RL1))
			return;

		libusb_get_device_descriptor(inst->dev, &des);

		/* Blacklisted devices are not able to handle the go local request */
		if (check_usbtmc_blacklist_libusb(blacklist_remote, des.idVendor, des.idProduct))
			return;

#ifdef DEBUG_USB
		OutputDebugString(L"Returning local control.\n");
#endif

		r = libusb_control_transfer(inst->dev_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
									GO_TO_LOCAL, 0, inst->dev_interface,
									&status, 1, TRANSFER_TIMEOUT);
#ifdef DEBUG_USB
		if (r < 0 || status != USBTMC_STATUS_SUCCESS) {
			if (r < 0) {
				wsprintf(strbuf, L"Failed to clear local lockout state: %s.\n", libusb_error_name(r));  OutputDebugString(strbuf);
			} else {
				wsprintf(strbuf, L"Failed to clear local lockout state: USBTMC status %d.\n", status);  OutputDebugString(strbuf);
			}
		}
#endif
	}
}

INSTRUMENT_ENUM_t USB_TMC::checkIDTable(uint16_t idVendor, uint16_t idProduct)
{
	switch (idVendor) {
	case 0x1ab1:
		switch (idProduct) {
		case 0x0960:
			return INSTRUMENT_RECEIVER_SA_RIGOL_DSA875;
			break;
		}
		break;
	}

	/* Not a known instrument */
	return INSTRUMENT_NONE;
}


#if 0

/*
* xusb: Generic USB test program
* Copyright � 2009-2012 Pete Batard <pete@akeo.ie>
* Contributions to Mass Storage by Alan Stern.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "libusb.h"

#if defined(_WIN32)
#define msleep(msecs) Sleep(msecs)
#else
#include <time.h>
#define msleep(msecs) nanosleep(&(struct timespec){msecs / 1000, (msecs * 1000000) % 1000000000UL}, NULL);
#endif

#if defined(_MSC_VER)
#define snprintf _snprintf
#define putenv _putenv
#endif

#if !defined(bool)
#define bool int
#endif
#if !defined(true)
#define true (1 == 1)
#endif
#if !defined(false)
#define false (!true)
#endif

// Future versions of libusb will use usb_interface instead of interface
// in libusb_config_descriptor => catter for that
#define usb_interface lu_interface

// Global variables
static bool binary_dump = false;
static bool extra_info = false;
static bool force_device_request = false;	// For WCID descriptor queries
static const char* binary_name = NULL;

static int perr(char const *format, ...)
{
	va_list args;
	int r;

	va_start(args, format);
	r = vfprintf(stderr, format, args);
	va_end(args);

	return r;
}

#define ERR_EXIT(errcode) do { perr("   %s\n", libusb_strerror((enum libusb_error)errcode)); return -1; } while (0)
#define CALL_CHECK(fcall) do { r=fcall; if (r < 0) ERR_EXIT(r); } while (0);
#define B(x) (((x)!=0)?1:0)
#define be_to_int32(buf) (((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|(buf)[3])

#define RETRY_MAX                     5
#define REQUEST_SENSE_LENGTH          0x12
#define INQUIRY_LENGTH                0x24
#define READ_CAPACITY_LENGTH          0x08

// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03

// Mass Storage Requests values. See section 3 of the Bulk-Only Mass Storage Class specifications
#define BOMS_RESET                    0xFF
#define BOMS_GET_MAX_LUN              0xFE

// Section 5.1: Command Block Wrapper (CBW)
struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

// Section 5.2: Command Status Wrapper (CSW)
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static uint8_t cdb_length[256] = {
	//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};

static enum test_type {
	USE_GENERIC,
	USE_PS3,
	USE_XBOX,
	USE_SCSI,
	USE_HID,
} test_mode;
static uint16_t VID, PID;

static void display_buffer_hex(unsigned char *buffer, unsigned size)
{
	unsigned i, j, k;

	for (i = 0; i<size; i += 16) {
		printf("\n  %08x  ", i);
		for (j = 0, k = 0; k<16; j++, k++) {
			if (i + j < size) {
				printf("%02x", buffer[i + j]);
			}
			else {
				printf("  ");
			}
			printf(" ");
		}
		printf(" ");
		for (j = 0, k = 0; k<16; j++, k++) {
			if (i + j < size) {
				if ((buffer[i + j] < 32) || (buffer[i + j] > 126)) {
					printf(".");
				}
				else {
					printf("%c", buffer[i + j]);
				}
			}
		}
	}
	printf("\n");
}

static char* uuid_to_string(const uint8_t* uuid)
{
	static char uuid_string[40];
	if (uuid == NULL) return NULL;
	sprintf(uuid_string, "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	return uuid_string;
}

// The PS3 Controller is really a HID device that got its HID Report Descriptors
// removed by Sony
static int display_ps3_status(libusb_device_handle *handle)
{
	int r;
	uint8_t input_report[49];
	uint8_t master_bt_address[8];
	uint8_t device_bt_address[18];

	// Get the controller's bluetooth address of its master device
	CALL_CHECK(libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_GET_REPORT, 0x03f5, 0, master_bt_address, sizeof(master_bt_address), 100));
	printf("\nMaster's bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X\n", master_bt_address[2], master_bt_address[3],
		master_bt_address[4], master_bt_address[5], master_bt_address[6], master_bt_address[7]);

	// Get the controller's bluetooth address
	CALL_CHECK(libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_GET_REPORT, 0x03f2, 0, device_bt_address, sizeof(device_bt_address), 100));
	printf("\nMaster's bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X\n", device_bt_address[4], device_bt_address[5],
		device_bt_address[6], device_bt_address[7], device_bt_address[8], device_bt_address[9]);

	// Get the status of the controller's buttons via its HID report
	printf("\nReading PS3 Input Report...\n");
	CALL_CHECK(libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_GET_REPORT, (HID_REPORT_TYPE_INPUT << 8) | 0x01, 0, input_report, sizeof(input_report), 1000));
	switch (input_report[2]) {	/** Direction pad plus start, select, and joystick buttons */
	case 0x01:
		printf("\tSELECT pressed\n");
		break;
	case 0x02:
		printf("\tLEFT 3 pressed\n");
		break;
	case 0x04:
		printf("\tRIGHT 3 pressed\n");
		break;
	case 0x08:
		printf("\tSTART presed\n");
		break;
	case 0x10:
		printf("\tUP pressed\n");
		break;
	case 0x20:
		printf("\tRIGHT pressed\n");
		break;
	case 0x40:
		printf("\tDOWN pressed\n");
		break;
	case 0x80:
		printf("\tLEFT pressed\n");
		break;
	}
	switch (input_report[3]) {	/** Shapes plus top right and left buttons */
	case 0x01:
		printf("\tLEFT 2 pressed\n");
		break;
	case 0x02:
		printf("\tRIGHT 2 pressed\n");
		break;
	case 0x04:
		printf("\tLEFT 1 pressed\n");
		break;
	case 0x08:
		printf("\tRIGHT 1 presed\n");
		break;
	case 0x10:
		printf("\tTRIANGLE pressed\n");
		break;
	case 0x20:
		printf("\tCIRCLE pressed\n");
		break;
	case 0x40:
		printf("\tCROSS pressed\n");
		break;
	case 0x80:
		printf("\tSQUARE pressed\n");
		break;
	}
	printf("\tPS button: %d\n", input_report[4]);
	printf("\tLeft Analog (X,Y): (%d,%d)\n", input_report[6], input_report[7]);
	printf("\tRight Analog (X,Y): (%d,%d)\n", input_report[8], input_report[9]);
	printf("\tL2 Value: %d\tR2 Value: %d\n", input_report[18], input_report[19]);
	printf("\tL1 Value: %d\tR1 Value: %d\n", input_report[20], input_report[21]);
	printf("\tRoll (x axis): %d Yaw (y axis): %d Pitch (z axis) %d\n",
		//(((input_report[42] + 128) % 256) - 128),
		(int8_t)(input_report[42]),
		(int8_t)(input_report[44]),
		(int8_t)(input_report[46]));
	printf("\tAcceleration: %d\n\n", (int8_t)(input_report[48]));
	return 0;
}
// The XBOX Controller is really a HID device that got its HID Report Descriptors
// removed by Microsoft.
// Input/Output reports described at http://euc.jp/periphs/xbox-controller.ja.html
static int display_xbox_status(libusb_device_handle *handle)
{
	int r;
	uint8_t input_report[20];
	printf("\nReading XBox Input Report...\n");
	CALL_CHECK(libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_GET_REPORT, (HID_REPORT_TYPE_INPUT << 8) | 0x00, 0, input_report, 20, 1000));
	printf("   D-pad: %02X\n", input_report[2] & 0x0F);
	printf("   Start:%d, Back:%d, Left Stick Press:%d, Right Stick Press:%d\n", B(input_report[2] & 0x10), B(input_report[2] & 0x20),
		B(input_report[2] & 0x40), B(input_report[2] & 0x80));
	// A, B, X, Y, Black, White are pressure sensitive
	printf("   A:%d, B:%d, X:%d, Y:%d, White:%d, Black:%d\n", input_report[4], input_report[5],
		input_report[6], input_report[7], input_report[9], input_report[8]);
	printf("   Left Trigger: %d, Right Trigger: %d\n", input_report[10], input_report[11]);
	printf("   Left Analog (X,Y): (%d,%d)\n", (int16_t)((input_report[13] << 8) | input_report[12]),
		(int16_t)((input_report[15] << 8) | input_report[14]));
	printf("   Right Analog (X,Y): (%d,%d)\n", (int16_t)((input_report[17] << 8) | input_report[16]),
		(int16_t)((input_report[19] << 8) | input_report[18]));
	return 0;
}

static int set_xbox_actuators(libusb_device_handle *handle, uint8_t left, uint8_t right)
{
	int r;
	uint8_t output_report[6];

	printf("\nWriting XBox Controller Output Report...\n");

	memset(output_report, 0, sizeof(output_report));
	output_report[1] = sizeof(output_report);
	output_report[3] = left;
	output_report[5] = right;

	CALL_CHECK(libusb_control_transfer(handle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		HID_SET_REPORT, (HID_REPORT_TYPE_OUTPUT << 8) | 0x00, 0, output_report, 06, 1000));
	return 0;
}

static int send_mass_storage_command(libusb_device_handle *handle, uint8_t endpoint, uint8_t lun,
	uint8_t *cdb, uint8_t direction, int data_length, uint32_t *ret_tag)
{
	static uint32_t tag = 1;
	uint8_t cdb_len;
	int i, r, size;
	struct command_block_wrapper cbw;

	if (cdb == NULL) {
		return -1;
	}

	if (endpoint & LIBUSB_ENDPOINT_IN) {
		perr("send_mass_storage_command: cannot send command on IN endpoint\n");
		return -1;
	}

	cdb_len = cdb_length[cdb[0]];
	if ((cdb_len == 0) || (cdb_len > sizeof(cbw.CBWCB))) {
		perr("send_mass_storage_command: don't know how to handle this command (%02X, length %d)\n",
			cdb[0], cdb_len);
		return -1;
	}

	memset(&cbw, 0, sizeof(cbw));
	cbw.dCBWSignature[0] = 'U';
	cbw.dCBWSignature[1] = 'S';
	cbw.dCBWSignature[2] = 'B';
	cbw.dCBWSignature[3] = 'C';
	*ret_tag = tag;
	cbw.dCBWTag = tag++;
	cbw.dCBWDataTransferLength = data_length;
	cbw.bmCBWFlags = direction;
	cbw.bCBWLUN = lun;
	// Subclass is 1 or 6 => cdb_len
	cbw.bCBWCBLength = cdb_len;
	memcpy(cbw.CBWCB, cdb, cdb_len);

	i = 0;
	do {
		// The transfer length must always be exactly 31 bytes.
		r = libusb_bulk_transfer(handle, endpoint, (unsigned char*)&cbw, 31, &size, 1000);
		if (r == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(handle, endpoint);
		}
		i++;
	} while ((r == LIBUSB_ERROR_PIPE) && (i<RETRY_MAX));
	if (r != LIBUSB_SUCCESS) {
		perr("   send_mass_storage_command: %s\n", libusb_strerror((enum libusb_error)r));
		return -1;
	}

	printf("   sent %d CDB bytes\n", cdb_len);
	return 0;
}

static int get_mass_storage_status(libusb_device_handle *handle, uint8_t endpoint, uint32_t expected_tag)
{
	int i, r, size;
	struct command_status_wrapper csw;

	// The device is allowed to STALL this transfer. If it does, you have to
	// clear the stall and try again.
	i = 0;
	do {
		r = libusb_bulk_transfer(handle, endpoint, (unsigned char*)&csw, 13, &size, 1000);
		if (r == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(handle, endpoint);
		}
		i++;
	} while ((r == LIBUSB_ERROR_PIPE) && (i<RETRY_MAX));
	if (r != LIBUSB_SUCCESS) {
		perr("   get_mass_storage_status: %s\n", libusb_strerror((enum libusb_error)r));
		return -1;
	}
	if (size != 13) {
		perr("   get_mass_storage_status: received %d bytes (expected 13)\n", size);
		return -1;
	}
	if (csw.dCSWTag != expected_tag) {
		perr("   get_mass_storage_status: mismatched tags (expected %08X, received %08X)\n",
			expected_tag, csw.dCSWTag);
		return -1;
	}
	// For this test, we ignore the dCSWSignature check for validity...
	printf("   Mass Storage Status: %02X (%s)\n", csw.bCSWStatus, csw.bCSWStatus ? "FAILED" : "Success");
	if (csw.dCSWTag != expected_tag)
		return -1;
	if (csw.bCSWStatus) {
		// REQUEST SENSE is appropriate only if bCSWStatus is 1, meaning that the
		// command failed somehow.  Larger values (2 in particular) mean that
		// the command couldn't be understood.
		if (csw.bCSWStatus == 1)
			return -2;	// request Get Sense
		else
			return -1;
	}

	// In theory we also should check dCSWDataResidue.  But lots of devices
	// set it wrongly.
	return 0;
}

static void get_sense(libusb_device_handle *handle, uint8_t endpoint_in, uint8_t endpoint_out)
{
	uint8_t cdb[16];	// SCSI Command Descriptor Block
	uint8_t sense[18];
	uint32_t expected_tag;
	int size;
	int rc;

	// Request Sense
	printf("Request Sense:\n");
	memset(sense, 0, sizeof(sense));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x03;	// Request Sense
	cdb[4] = REQUEST_SENSE_LENGTH;

	send_mass_storage_command(handle, endpoint_out, 0, cdb, LIBUSB_ENDPOINT_IN, REQUEST_SENSE_LENGTH, &expected_tag);
	rc = libusb_bulk_transfer(handle, endpoint_in, (unsigned char*)&sense, REQUEST_SENSE_LENGTH, &size, 1000);
	if (rc < 0)
	{
		printf("libusb_bulk_transfer failed: %s\n", libusb_error_name(rc));
		return;
	}
	printf("   received %d bytes\n", size);

	if ((sense[0] != 0x70) && (sense[0] != 0x71)) {
		perr("   ERROR No sense data\n");
	}
	else {
		perr("   ERROR Sense: %02X %02X %02X\n", sense[2] & 0x0F, sense[12], sense[13]);
	}
	// Strictly speaking, the get_mass_storage_status() call should come
	// before these perr() lines.  If the status is nonzero then we must
	// assume there's no data in the buffer.  For xusb it doesn't matter.
	get_mass_storage_status(handle, endpoint_in, expected_tag);
}

// Mass Storage device to test bulk transfers (non destructive test)
static int test_mass_storage(libusb_device_handle *handle, uint8_t endpoint_in, uint8_t endpoint_out)
{
	int r, size;
	uint8_t lun;
	uint32_t expected_tag;
	uint32_t i, max_lba, block_size;
	double device_size;
	uint8_t cdb[16];	// SCSI Command Descriptor Block
	uint8_t buffer[64];
	char vid[9], pid[9], rev[5];
	unsigned char *data;
	FILE *fd;

	printf("Reading Max LUN:\n");
	r = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		BOMS_GET_MAX_LUN, 0, 0, &lun, 1, 1000);
	// Some devices send a STALL instead of the actual value.
	// In such cases we should set lun to 0.
	if (r == 0) {
		lun = 0;
	}
	else if (r < 0) {
		perr("   Failed: %s", libusb_strerror((enum libusb_error)r));
	}
	printf("   Max LUN = %d\n", lun);

	// Send Inquiry
	printf("Sending Inquiry:\n");
	memset(buffer, 0, sizeof(buffer));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x12;	// Inquiry
	cdb[4] = INQUIRY_LENGTH;

	send_mass_storage_command(handle, endpoint_out, lun, cdb, LIBUSB_ENDPOINT_IN, INQUIRY_LENGTH, &expected_tag);
	CALL_CHECK(libusb_bulk_transfer(handle, endpoint_in, (unsigned char*)&buffer, INQUIRY_LENGTH, &size, 1000));
	printf("   received %d bytes\n", size);
	// The following strings are not zero terminated
	for (i = 0; i<8; i++) {
		vid[i] = buffer[8 + i];
		pid[i] = buffer[16 + i];
		rev[i / 2] = buffer[32 + i / 2];	// instead of another loop
	}
	vid[8] = 0;
	pid[8] = 0;
	rev[4] = 0;
	printf("   VID:PID:REV \"%8s\":\"%8s\":\"%4s\"\n", vid, pid, rev);
	if (get_mass_storage_status(handle, endpoint_in, expected_tag) == -2) {
		get_sense(handle, endpoint_in, endpoint_out);
	}

	// Read capacity
	printf("Reading Capacity:\n");
	memset(buffer, 0, sizeof(buffer));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x25;	// Read Capacity

	send_mass_storage_command(handle, endpoint_out, lun, cdb, LIBUSB_ENDPOINT_IN, READ_CAPACITY_LENGTH, &expected_tag);
	CALL_CHECK(libusb_bulk_transfer(handle, endpoint_in, (unsigned char*)&buffer, READ_CAPACITY_LENGTH, &size, 1000));
	printf("   received %d bytes\n", size);
	max_lba = be_to_int32(&buffer[0]);
	block_size = be_to_int32(&buffer[4]);
	device_size = ((double)(max_lba + 1))*block_size / (1024 * 1024 * 1024);
	printf("   Max LBA: %08X, Block Size: %08X (%.2f GB)\n", max_lba, block_size, device_size);
	if (get_mass_storage_status(handle, endpoint_in, expected_tag) == -2) {
		get_sense(handle, endpoint_in, endpoint_out);
	}

	// coverity[tainted_data]
	data = (unsigned char*)calloc(1, block_size);
	if (data == NULL) {
		perr("   unable to allocate data buffer\n");
		return -1;
	}

	// Send Read
	printf("Attempting to read %d bytes:\n", block_size);
	memset(cdb, 0, sizeof(cdb));

	cdb[0] = 0x28;	// Read(10)
	cdb[8] = 0x01;	// 1 block

	send_mass_storage_command(handle, endpoint_out, lun, cdb, LIBUSB_ENDPOINT_IN, block_size, &expected_tag);
	libusb_bulk_transfer(handle, endpoint_in, data, block_size, &size, 5000);
	printf("   READ: received %d bytes\n", size);
	if (get_mass_storage_status(handle, endpoint_in, expected_tag) == -2) {
		get_sense(handle, endpoint_in, endpoint_out);
	}
	else {
		display_buffer_hex(data, size);
		if ((binary_dump) && ((fd = fopen(binary_name, "w")) != NULL)) {
			if (fwrite(data, 1, (size_t)size, fd) != (unsigned int)size) {
				perr("   unable to write binary data\n");
			}
			fclose(fd);
		}
	}
	free(data);

	return 0;
}

// HID
static int get_hid_record_size(uint8_t *hid_report_descriptor, int size, int type)
{
	uint8_t i, j = 0;
	uint8_t offset;
	int record_size[3] = { 0, 0, 0 };
	int nb_bits = 0, nb_items = 0;
	bool found_record_marker;

	found_record_marker = false;
	for (i = hid_report_descriptor[0] + 1; i < size; i += offset) {
		offset = (hid_report_descriptor[i] & 0x03) + 1;
		if (offset == 4)
			offset = 5;
		switch (hid_report_descriptor[i] & 0xFC) {
		case 0x74:	// bitsize
			nb_bits = hid_report_descriptor[i + 1];
			break;
		case 0x94:	// count
			nb_items = 0;
			for (j = 1; j<offset; j++) {
				nb_items = ((uint32_t)hid_report_descriptor[i + j]) << (8 * (j - 1));
			}
			break;
		case 0x80:	// input
			found_record_marker = true;
			j = 0;
			break;
		case 0x90:	// output
			found_record_marker = true;
			j = 1;
			break;
		case 0xb0:	// feature
			found_record_marker = true;
			j = 2;
			break;
		case 0xC0:	// end of collection
			nb_items = 0;
			nb_bits = 0;
			break;
		default:
			continue;
		}
		if (found_record_marker) {
			found_record_marker = false;
			record_size[j] += nb_items*nb_bits;
		}
	}
	if ((type < HID_REPORT_TYPE_INPUT) || (type > HID_REPORT_TYPE_FEATURE)) {
		return 0;
	}
	else {
		return (record_size[type - HID_REPORT_TYPE_INPUT] + 7) / 8;
	}
}

static int test_hid(libusb_device_handle *handle, uint8_t endpoint_in)
{
	int r, size, descriptor_size;
	uint8_t hid_report_descriptor[256];
	uint8_t *report_buffer;
	FILE *fd;

	printf("\nReading HID Report Descriptors:\n");
	descriptor_size = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_INTERFACE,
		LIBUSB_REQUEST_GET_DESCRIPTOR, LIBUSB_DT_REPORT << 8, 0, hid_report_descriptor, sizeof(hid_report_descriptor), 1000);
	if (descriptor_size < 0) {
		printf("   Failed\n");
		return -1;
	}
	display_buffer_hex(hid_report_descriptor, descriptor_size);
	if ((binary_dump) && ((fd = fopen(binary_name, "w")) != NULL)) {
		if (fwrite(hid_report_descriptor, 1, descriptor_size, fd) != descriptor_size) {
			printf("   Error writing descriptor to file\n");
		}
		fclose(fd);
	}

	size = get_hid_record_size(hid_report_descriptor, descriptor_size, HID_REPORT_TYPE_FEATURE);
	if (size <= 0) {
		printf("\nSkipping Feature Report readout (None detected)\n");
	}
	else {
		report_buffer = (uint8_t*)calloc(size, 1);
		if (report_buffer == NULL) {
			return -1;
		}

		printf("\nReading Feature Report (length %d)...\n", size);
		r = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			HID_GET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | 0, 0, report_buffer, (uint16_t)size, 5000);
		if (r >= 0) {
			display_buffer_hex(report_buffer, size);
		}
		else {
			switch (r) {
			case LIBUSB_ERROR_NOT_FOUND:
				printf("   No Feature Report available for this device\n");
				break;
			case LIBUSB_ERROR_PIPE:
				printf("   Detected stall - resetting pipe...\n");
				libusb_clear_halt(handle, 0);
				break;
			default:
				printf("   Error: %s\n", libusb_strerror((enum libusb_error)r));
				break;
			}
		}
		free(report_buffer);
	}

	size = get_hid_record_size(hid_report_descriptor, descriptor_size, HID_REPORT_TYPE_INPUT);
	if (size <= 0) {
		printf("\nSkipping Input Report readout (None detected)\n");
	}
	else {
		report_buffer = (uint8_t*)calloc(size, 1);
		if (report_buffer == NULL) {
			return -1;
		}

		printf("\nReading Input Report (length %d)...\n", size);
		r = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			HID_GET_REPORT, (HID_REPORT_TYPE_INPUT << 8) | 0x00, 0, report_buffer, (uint16_t)size, 5000);
		if (r >= 0) {
			display_buffer_hex(report_buffer, size);
		}
		else {
			switch (r) {
			case LIBUSB_ERROR_TIMEOUT:
				printf("   Timeout! Please make sure you act on the device within the 5 seconds allocated...\n");
				break;
			case LIBUSB_ERROR_PIPE:
				printf("   Detected stall - resetting pipe...\n");
				libusb_clear_halt(handle, 0);
				break;
			default:
				printf("   Error: %s\n", libusb_strerror((enum libusb_error)r));
				break;
			}
		}

		// Attempt a bulk read from endpoint 0 (this should just return a raw input report)
		printf("\nTesting interrupt read using endpoint %02X...\n", endpoint_in);
		r = libusb_interrupt_transfer(handle, endpoint_in, report_buffer, size, &size, 5000);
		if (r >= 0) {
			display_buffer_hex(report_buffer, size);
		}
		else {
			printf("   %s\n", libusb_strerror((enum libusb_error)r));
		}

		free(report_buffer);
	}
	return 0;
}

// Read the MS WinUSB Feature Descriptors, that are used on Windows 8 for automated driver installation
static void read_ms_winsub_feature_descriptors(libusb_device_handle *handle, uint8_t bRequest, int iface_number)
{
#define MAX_OS_FD_LENGTH 256
	int i, r;
	uint8_t os_desc[MAX_OS_FD_LENGTH];
	uint32_t length;
	void* le_type_punning_IS_fine;
	struct {
		const char* desc;
		uint8_t recipient;
		uint16_t index;
		uint16_t header_size;
	} os_fd[2] = {
		{ "Extended Compat ID", LIBUSB_RECIPIENT_DEVICE, 0x0004, 0x10 },
		{ "Extended Properties", LIBUSB_RECIPIENT_INTERFACE, 0x0005, 0x0A }
	};

	if (iface_number < 0) return;
	// WinUSB has a limitation that forces wIndex to the interface number when issuing
	// an Interface Request. To work around that, we can force a Device Request for
	// the Extended Properties, assuming the device answers both equally.
	if (force_device_request)
		os_fd[1].recipient = LIBUSB_RECIPIENT_DEVICE;

	for (i = 0; i<2; i++) {
		printf("\nReading %s OS Feature Descriptor (wIndex = 0x%04d):\n", os_fd[i].desc, os_fd[i].index);

		// Read the header part
		r = libusb_control_transfer(handle, (uint8_t)(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | os_fd[i].recipient),
			bRequest, (uint16_t)(((iface_number) << 8) | 0x00), os_fd[i].index, os_desc, os_fd[i].header_size, 1000);
		if (r < os_fd[i].header_size) {
			perr("   Failed: %s", (r<0) ? libusb_strerror((enum libusb_error)r) : "header size is too small");
			return;
		}
		le_type_punning_IS_fine = (void*)os_desc;
		length = *((uint32_t*)le_type_punning_IS_fine);
		if (length > MAX_OS_FD_LENGTH) {
			length = MAX_OS_FD_LENGTH;
		}

		// Read the full feature descriptor
		r = libusb_control_transfer(handle, (uint8_t)(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | os_fd[i].recipient),
			bRequest, (uint16_t)(((iface_number) << 8) | 0x00), os_fd[i].index, os_desc, (uint16_t)length, 1000);
		if (r < 0) {
			perr("   Failed: %s", libusb_strerror((enum libusb_error)r));
			return;
		}
		else {
			display_buffer_hex(os_desc, r);
		}
	}
}

static void print_device_cap(struct libusb_bos_dev_capability_descriptor *dev_cap)
{
	switch (dev_cap->bDevCapabilityType) {
	case LIBUSB_BT_USB_2_0_EXTENSION: {
		struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext = NULL;
		libusb_get_usb_2_0_extension_descriptor(NULL, dev_cap, &usb_2_0_ext);
		if (usb_2_0_ext) {
			printf("    USB 2.0 extension:\n");
			printf("      attributes             : %02X\n", usb_2_0_ext->bmAttributes);
			libusb_free_usb_2_0_extension_descriptor(usb_2_0_ext);
		}
		break;
	}
	case LIBUSB_BT_SS_USB_DEVICE_CAPABILITY: {
		struct libusb_ss_usb_device_capability_descriptor *ss_usb_device_cap = NULL;
		libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_usb_device_cap);
		if (ss_usb_device_cap) {
			printf("    USB 3.0 capabilities:\n");
			printf("      attributes             : %02X\n", ss_usb_device_cap->bmAttributes);
			printf("      supported speeds       : %04X\n", ss_usb_device_cap->wSpeedSupported);
			printf("      supported functionality: %02X\n", ss_usb_device_cap->bFunctionalitySupport);
			libusb_free_ss_usb_device_capability_descriptor(ss_usb_device_cap);
		}
		break;
	}
	case LIBUSB_BT_CONTAINER_ID: {
		struct libusb_container_id_descriptor *container_id = NULL;
		libusb_get_container_id_descriptor(NULL, dev_cap, &container_id);
		if (container_id) {
			printf("    Container ID:\n      %s\n", uuid_to_string(container_id->ContainerID));
			libusb_free_container_id_descriptor(container_id);
		}
		break;
	}
	default:
		printf("    Unknown BOS device capability %02x:\n", dev_cap->bDevCapabilityType);
	}
}

static int test_device(uint16_t vid, uint16_t pid)
{
	libusb_device_handle *handle;
	libusb_device *dev;
	uint8_t bus, port_path[8];
	struct libusb_bos_descriptor *bos_desc;
	struct libusb_config_descriptor *conf_desc;
	const struct libusb_endpoint_descriptor *endpoint;
	int i, j, k, r;
	int iface, nb_ifaces, first_iface = -1;
	struct libusb_device_descriptor dev_desc;
	const char* speed_name[5] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
		"480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)" };
	char string[128];
	uint8_t string_index[3];	// indexes of the string descriptors
	uint8_t endpoint_in = 0, endpoint_out = 0;	// default IN and OUT endpoints

	printf("Opening device %04X:%04X...\n", vid, pid);
	handle = libusb_open_device_with_vid_pid(NULL, vid, pid);

	if (handle == NULL) {
		perr("  Failed.\n");
		return -1;
	}

	dev = libusb_get_device(handle);
	bus = libusb_get_bus_number(dev);
	if (extra_info) {
		r = libusb_get_port_numbers(dev, port_path, sizeof(port_path));
		if (r > 0) {
			printf("\nDevice properties:\n");
			printf("        bus number: %d\n", bus);
			printf("         port path: %d", port_path[0]);
			for (i = 1; i<r; i++) {
				printf("->%d", port_path[i]);
			}
			printf(" (from root hub)\n");
		}
		r = libusb_get_device_speed(dev);
		if ((r<0) || (r>4)) r = 0;
		printf("             speed: %s\n", speed_name[r]);
	}

	printf("\nReading device descriptor:\n");
	CALL_CHECK(libusb_get_device_descriptor(dev, &dev_desc));
	printf("            length: %d\n", dev_desc.bLength);
	printf("      device class: %d\n", dev_desc.bDeviceClass);
	printf("               S/N: %d\n", dev_desc.iSerialNumber);
	printf("           VID:PID: %04X:%04X\n", dev_desc.idVendor, dev_desc.idProduct);
	printf("         bcdDevice: %04X\n", dev_desc.bcdDevice);
	printf("   iMan:iProd:iSer: %d:%d:%d\n", dev_desc.iManufacturer, dev_desc.iProduct, dev_desc.iSerialNumber);
	printf("          nb confs: %d\n", dev_desc.bNumConfigurations);
	// Copy the string descriptors for easier parsing
	string_index[0] = dev_desc.iManufacturer;
	string_index[1] = dev_desc.iProduct;
	string_index[2] = dev_desc.iSerialNumber;

	printf("\nReading BOS descriptor: ");
	if (libusb_get_bos_descriptor(handle, &bos_desc) == LIBUSB_SUCCESS) {
		printf("%d caps\n", bos_desc->bNumDeviceCaps);
		for (i = 0; i < bos_desc->bNumDeviceCaps; i++)
			print_device_cap(bos_desc->dev_capability[i]);
		libusb_free_bos_descriptor(bos_desc);
	}
	else {
		printf("no descriptor\n");
	}

	printf("\nReading first configuration descriptor:\n");
	CALL_CHECK(libusb_get_config_descriptor(dev, 0, &conf_desc));
	nb_ifaces = conf_desc->bNumInterfaces;
	printf("             nb interfaces: %d\n", nb_ifaces);
	if (nb_ifaces > 0)
		first_iface = conf_desc->usb_interface[0].altsetting[0].bInterfaceNumber;
	for (i = 0; i<nb_ifaces; i++) {
		printf("              interface[%d]: id = %d\n", i,
			conf_desc->usb_interface[i].altsetting[0].bInterfaceNumber);
		for (j = 0; j<conf_desc->usb_interface[i].num_altsetting; j++) {
			printf("interface[%d].altsetting[%d]: num endpoints = %d\n",
				i, j, conf_desc->usb_interface[i].altsetting[j].bNumEndpoints);
			printf("   Class.SubClass.Protocol: %02X.%02X.%02X\n",
				conf_desc->usb_interface[i].altsetting[j].bInterfaceClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol);
			if ((conf_desc->usb_interface[i].altsetting[j].bInterfaceClass == LIBUSB_CLASS_MASS_STORAGE)
				&& ((conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x01)
					|| (conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x06))
				&& (conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol == 0x50)) {
				// Mass storage devices that can use basic SCSI commands
				test_mode = USE_SCSI;
			}
			for (k = 0; k<conf_desc->usb_interface[i].altsetting[j].bNumEndpoints; k++) {
				struct libusb_ss_endpoint_companion_descriptor *ep_comp = NULL;
				endpoint = &conf_desc->usb_interface[i].altsetting[j].endpoint[k];
				printf("       endpoint[%d].address: %02X\n", k, endpoint->bEndpointAddress);
				// Use the first interrupt or bulk IN/OUT endpoints as default for testing
				if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_BULK | LIBUSB_TRANSFER_TYPE_INTERRUPT)) {
					if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						if (!endpoint_in)
							endpoint_in = endpoint->bEndpointAddress;
					}
					else {
						if (!endpoint_out)
							endpoint_out = endpoint->bEndpointAddress;
					}
				}
				printf("           max packet size: %04X\n", endpoint->wMaxPacketSize);
				printf("          polling interval: %02X\n", endpoint->bInterval);
				libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
				if (ep_comp) {
					printf("                 max burst: %02X   (USB 3.0)\n", ep_comp->bMaxBurst);
					printf("        bytes per interval: %04X (USB 3.0)\n", ep_comp->wBytesPerInterval);
					libusb_free_ss_endpoint_companion_descriptor(ep_comp);
				}
			}
		}
	}
	libusb_free_config_descriptor(conf_desc);

	libusb_set_auto_detach_kernel_driver(handle, 1);
	for (iface = 0; iface < nb_ifaces; iface++)
	{
		printf("\nClaiming interface %d...\n", iface);
		r = libusb_claim_interface(handle, iface);
		if (r != LIBUSB_SUCCESS) {
			perr("   Failed.\n");
		}
	}

	printf("\nReading string descriptors:\n");
	for (i = 0; i<3; i++) {
		if (string_index[i] == 0) {
			continue;
		}
		if (libusb_get_string_descriptor_ascii(handle, string_index[i], (unsigned char*)string, 128) >= 0) {
			printf("   String (0x%02X): \"%s\"\n", string_index[i], string);
		}
	}
	// Read the OS String Descriptor
	if (libusb_get_string_descriptor_ascii(handle, 0xEE, (unsigned char*)string, 128) >= 0) {
		printf("   String (0x%02X): \"%s\"\n", 0xEE, string);
		// If this is a Microsoft OS String Descriptor,
		// attempt to read the WinUSB extended Feature Descriptors
		if (strncmp(string, "MSFT100", 7) == 0)
			read_ms_winsub_feature_descriptors(handle, string[7], first_iface);
	}

	switch (test_mode) {
	case USE_PS3:
		CALL_CHECK(display_ps3_status(handle));
		break;
	case USE_XBOX:
		CALL_CHECK(display_xbox_status(handle));
		CALL_CHECK(set_xbox_actuators(handle, 128, 222));
		msleep(2000);
		CALL_CHECK(set_xbox_actuators(handle, 0, 0));
		break;
	case USE_HID:
		test_hid(handle, endpoint_in);
		break;
	case USE_SCSI:
		CALL_CHECK(test_mass_storage(handle, endpoint_in, endpoint_out));
	case USE_GENERIC:
		break;
	}

	printf("\n");
	for (iface = 0; iface<nb_ifaces; iface++) {
		printf("Releasing interface %d...\n", iface);
		libusb_release_interface(handle, iface);
	}

	printf("Closing device...\n");
	libusb_close(handle);

	return 0;
}

int main(int argc, char** argv)
{
	bool show_help = false;
	bool debug_mode = false;
	const struct libusb_version* version;
	int j, r;
	size_t i, arglen;
	unsigned tmp_vid, tmp_pid;
	uint16_t endian_test = 0xBE00;
	char *error_lang = NULL, *old_dbg_str = NULL, str[256];

	// Default to generic, expecting VID:PID
	VID = 0;
	PID = 0;
	test_mode = USE_GENERIC;

	if (((uint8_t*)&endian_test)[0] == 0xBE) {
		printf("Despite their natural superiority for end users, big endian\n"
			"CPUs are not supported with this program, sorry.\n");
		return 0;
	}

	if (argc >= 2) {
		for (j = 1; j<argc; j++) {
			arglen = strlen(argv[j]);
			if (((argv[j][0] == '-') || (argv[j][0] == '/'))
				&& (arglen >= 2)) {
				switch (argv[j][1]) {
				case 'd':
					debug_mode = true;
					break;
				case 'i':
					extra_info = true;
					break;
				case 'w':
					force_device_request = true;
					break;
				case 'b':
					if ((j + 1 >= argc) || (argv[j + 1][0] == '-') || (argv[j + 1][0] == '/')) {
						printf("   Option -b requires a file name\n");
						return 1;
					}
					binary_name = argv[++j];
					binary_dump = true;
					break;
				case 'l':
					if ((j + 1 >= argc) || (argv[j + 1][0] == '-') || (argv[j + 1][0] == '/')) {
						printf("   Option -l requires an ISO 639-1 language parameter\n");
						return 1;
					}
					error_lang = argv[++j];
					break;
				case 'j':
					// OLIMEX ARM-USB-TINY JTAG, 2 channel composite device - 2 interfaces
					if (!VID && !PID) {
						VID = 0x15BA;
						PID = 0x0004;
					}
					break;
				case 'k':
					// Generic 2 GB USB Key (SCSI Transparent/Bulk Only) - 1 interface
					if (!VID && !PID) {
						VID = 0x0204;
						PID = 0x6025;
					}
					break;
					// The following tests will force VID:PID if already provided
				case 'p':
					// Sony PS3 Controller - 1 interface
					VID = 0x054C;
					PID = 0x0268;
					test_mode = USE_PS3;
					break;
				case 's':
					// Microsoft Sidewinder Precision Pro Joystick - 1 HID interface
					VID = 0x045E;
					PID = 0x0008;
					test_mode = USE_HID;
					break;
				case 'x':
					// Microsoft XBox Controller Type S - 1 interface
					VID = 0x045E;
					PID = 0x0289;
					test_mode = USE_XBOX;
					break;
				default:
					show_help = true;
					break;
				}
			}
			else {
				for (i = 0; i<arglen; i++) {
					if (argv[j][i] == ':')
						break;
				}
				if (i != arglen) {
					if (sscanf(argv[j], "%x:%x", &tmp_vid, &tmp_pid) != 2) {
						printf("   Please specify VID & PID as \"vid:pid\" in hexadecimal format\n");
						return 1;
					}
					VID = (uint16_t)tmp_vid;
					PID = (uint16_t)tmp_pid;
				}
				else {
					show_help = true;
				}
			}
		}
	}

	if ((show_help) || (argc == 1) || (argc > 7)) {
		printf("usage: %s [-h] [-d] [-i] [-k] [-b file] [-l lang] [-j] [-x] [-s] [-p] [-w] [vid:pid]\n", argv[0]);
		printf("   -h      : display usage\n");
		printf("   -d      : enable debug output\n");
		printf("   -i      : print topology and speed info\n");
		printf("   -j      : test composite FTDI based JTAG device\n");
		printf("   -k      : test Mass Storage device\n");
		printf("   -b file : dump Mass Storage data to file 'file'\n");
		printf("   -p      : test Sony PS3 SixAxis controller\n");
		printf("   -s      : test Microsoft Sidewinder Precision Pro (HID)\n");
		printf("   -x      : test Microsoft XBox Controller Type S\n");
		printf("   -l lang : language to report errors in (ISO 639-1)\n");
		printf("   -w      : force the use of device requests when querying WCID descriptors\n");
		printf("If only the vid:pid is provided, xusb attempts to run the most appropriate test\n");
		return 0;
	}

	// xusb is commonly used as a debug tool, so it's convenient to have debug output during libusb_init(),
	// but since we can't call on libusb_set_debug() before libusb_init(), we use the env variable method
	old_dbg_str = getenv("LIBUSB_DEBUG");
	if (debug_mode) {
		if (putenv("LIBUSB_DEBUG=4") != 0)	// LIBUSB_LOG_LEVEL_DEBUG
			printf("Unable to set debug level");
	}

	version = libusb_get_version();
	printf("Using libusb v%d.%d.%d.%d\n\n", version->major, version->minor, version->micro, version->nano);
	r = libusb_init(NULL);
	if (r < 0)
		return r;

	// If not set externally, and no debug option was given, use info log level
	if ((old_dbg_str == NULL) && (!debug_mode))
		libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_INFO);
	if (error_lang != NULL) {
		r = libusb_setlocale(error_lang);
		if (r < 0)
			printf("Invalid or unsupported locale '%s': %s\n", error_lang, libusb_strerror((enum libusb_error)r));
	}

	test_device(VID, PID);

	libusb_exit(NULL);

	if (debug_mode) {
		snprintf(str, sizeof(str), "LIBUSB_DEBUG=%s", (old_dbg_str == NULL) ? "" : old_dbg_str);
		str[sizeof(str) - 1] = 0;	// Windows may not NUL terminate the string
	}

	return 0;
}

#endif
