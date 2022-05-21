#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include "KT_BinIO.h"
#include "KT_ProgressBar.h"

/* WCH ch55x vid and pid */
#define CH55X_VID 0x4348
#define CH55X_PID 0x55e0 

KT_BinIO ktFlash;

uint8_t u8Buff[64];
uint8_t u8Mask[8];

/* Detect MCU */
uint8_t u8DetectCmd[64] = {
	0xA1, 0x12, 0x00, 0x52, 0x11, 0x4D, 0x43, 0x55,
	0x20, 0x49, 0x53, 0x50, 0x20, 0x26, 0x20, 0x57,
	0x43, 0x48, 0x2e, 0x43, 0x4e
};
uint8_t u8DetectRespond = 6;

/* Get Bootloader Version, Chip ID */
uint8_t u8IdCmd[64] = {
	0xA7, 0x02, 0x00, 0x1F, 0x00
};
uint8_t u8IdRespond = 30;

/* Enable ISP */
uint8_t u8InitCmd[64] = {
	0xA8, 0x0E, 0x00, 0x07, 0x00, 0xFF, 0xFF, 0xFF,
	0xFF, 0x03, 0x00, 0x00, 0x00, 0xFF, 0x52, 0x00,
	0x00
};
uint8_t u8InitRespond = 6;

/* Set Flash Address */
uint8_t u8AddessCmd[64] = {
	0xA3, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};
uint8_t u8AddessRespond = 6;

/* Erase ??? */
uint8_t u8EraseCmd[64] = {
	0xA4, 0x01, 0x00, 0x08
};
uint8_t u8EraseRespond = 6;

/* Reset */
uint8_t u8ResetCmd[64] = {
	0xA2, 0x01, 0x00, 0x01 /* if 0x00 not run, 0x01 run*/
};
uint8_t u8ResetRespond = 6;

/* Write */
uint8_t u8WriteCmd[64] = {
	0xA5, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	/* byte 4 Low Address (first = 1) */
	/* byte 5 High Address */
};
uint8_t u8WriteRespond = 6;

/* Verify */
uint8_t u8VerifyCmd[64] = {
	0xA6, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	/* byte 4 Low Address (first = 1) */
	/* byte 5 High Address */
};
uint8_t u8VerifyRespond = 6;

uint8_t u8ReadCmd[64] = {
	0x00
};
uint8_t u8ReadRespond = 6;


uint32_t Write(libusb_device_handle *handle, uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(handle, 0x02, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}
uint32_t Read(libusb_device_handle *handle, uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(handle, 0x82, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}

int main(int argc, char const *argv[])
{
	uint32_t i;
	int rc;
	KT_BinIO ktBin;
  KT_ProgressBar ktProg;

  libusb_context *context = NULL;
  libusb_device **deviceList = NULL;
  libusb_device_handle *handle = NULL;

  int deviceCount;
  bool foundDevice = false;

	printf("WCH ch55x programmer\n");

	if (argc != 2) {
		printf("usage: ch55xisptool flash_file.bin\n");
    return 1;
	}
    /* load flash file */
	ktBin.u32Size = 10 * 1024;
	ktBin.InitBuffer();
	if (!ktBin.Read((char*)argv[1])) {
		printf("Read file: ERROR\n");
		return 0;
	}

	rc = libusb_init(&context);
	if(rc < 0) {
		fprintf(stderr, "Cannot initialize libusb: %s\n", libusb_error_name(rc));
		return 1;
	};

	// libusb_set_debug(context, 3);

  deviceCount = libusb_get_device_list(context, &deviceList); 

  if(deviceCount <= 0) {
    fprintf(stderr, "No USB device found\n");
    return 1;
  }

  for(size_t idx = 0; idx < deviceCount; ++idx) {
     libusb_device *device = deviceList[idx];
     libusb_device_descriptor desc = {0};
     rc = libusb_get_device_descriptor(device, &desc);
     if(rc < 0) {
       fprintf(stderr, "Error happened when list usb device\n");
       return 1;
     }
     if(desc.idVendor == CH55X_VID && desc.idProduct == CH55X_PID) {
       foundDevice = true; 
     }
  }

  libusb_free_device_list(deviceList, deviceCount);

  if(foundDevice) {
    printf("Found WinChipHead ch55x device.\n");
  } else {
    printf("Not found WinChipHead ch55x device: please make sure it's in ISP mode and plugged in.\n");
    exit(1);
  }
 
	handle = libusb_open_device_with_vid_pid(context, CH55X_VID, CH55X_PID);

	if (handle == NULL) {
		fprintf(stderr, "No permission to open device, try 'sudo'\n");
		return 1;
	}
	
	libusb_claim_interface(handle, 0);
	
	/* Detect MCU */
	if (!Write(handle, u8DetectCmd, u8DetectCmd[1] + 3)) {
		fprintf(stderr, "Send Detect: Fail\n");
		return 1;
	}

	if (!Read(handle, u8Buff, u8DetectRespond)) {
		fprintf(stderr, "Read Detect: Fail\n");
		return 1;
	}

  uint8_t chipID = u8Buff[4];

  /* Check MCU series/family? ID */
	if (u8Buff[5] != 0x11) {
		fprintf(stderr, "Not supported chip: %02X, %02x\n", chipID, u8Buff[5]);
		return 1;
	}

	/* Check MCU ID */
  switch (chipID) {
    case 0x51:
    case 0x52:
    case 0x54:
    case 0x58:
    case 0x59:
      break;
    default:
	    fprintf(stderr, "Not supported chip: %02X, %02x\n", chipID, u8Buff[5]);
	    return 1;
  }

  printf("Device model: CH5%x\n", chipID);
	
	/* Bootloader and Chip ID */
	if (!Write(handle, u8IdCmd, u8IdCmd[1] + 3)) {
		fprintf(stderr, "Send ID: Fail\n");
		return 1;
	}
	
	if (!Read(handle, u8Buff, u8IdRespond)) {
		fprintf(stderr, "Read ID: Fail\n");
		return 1;
	}
	
	printf("Bootloader: %d.%d.%d\n", u8Buff[19], u8Buff[20], u8Buff[21]);
	printf("ID: %02X %02X %02X %02X\n", u8Buff[22], u8Buff[23], u8Buff[24], u8Buff[25]);

	/* check bootloader version */
  /* version 2.3.0/2.3.1/2.4.0 known works, here assume version >= 2.3 works*/
	if (u8Buff[19] != 0x02 || u8Buff[20] < 0x03) {
		printf("Bootloader version %d.%d.%d not supported.\n");
		return 1;
	}

	/* Calc XOR Mask */

	uint8_t u8Sum;

	u8Sum = u8Buff[22] + u8Buff[23] + u8Buff[24] + u8Buff[25];
	for (i = 0; i < 8; ++i) {
		u8Mask[i] = u8Sum;
	}
	u8Mask[7] += chipID;
	printf("XOR Mask: ");
	for (i = 0; i < 8; ++i) {
		printf("%02X ", u8Mask[i]);
	}
	printf("\n");

	/* init or erase ??? */
	if (!Write(handle, u8InitCmd, u8InitCmd[1] + 3)) {
		printf("Send Init: Fail\n");
		return 1;
	}
	
	if (!Read(handle, u8Buff, u8InitRespond)) {
		printf("Read Init: Fail\n");
		return 1;
	}

	/* Bootloader and Chip ID */
	if (!Write(handle, u8IdCmd, u8IdCmd[1] + 3)) {
		printf("Send ID: Fail\n");
		return 1;
	}
	
	if (!Read(handle, u8Buff, u8IdRespond)) {
		printf("Read ID: Fail\n");
		return 1;
	}

	/* Set Flash Address to 0 */
	if (!Write(handle, u8AddessCmd, u8AddessCmd[1] + 3)) {
		printf("Send Address: Fail\n");
		return 1;
	}
	
	if (!Read(handle, u8Buff, u8AddessRespond)) {
		printf("Read Address: Fail\n");
		return 1;
	}

	/* Erase or unknow */
	if (!Write(handle, u8EraseCmd, u8EraseCmd[1] + 3)) {
		printf("Send Erase: Fail\n");
		return 1;
	}
	
	if (!Read(handle, u8Buff, u8EraseRespond)) {
		printf("Read Erase: Fail\n");
		return 1;
	}
	
	/* Progress */
	uint32_t n;
	n = 10 * 1024 / 56;
  ktProg.SetMax(n);
  ktProg.SetNum(50);
  ktProg.SetPos(0);
  ktProg.Display();

	/* Write */
	printf("Write: ");


	for (i = 0; i < n; ++i) {
		uint16_t u16Tmp;
		uint32_t j;
		/* Write flash */
		memmove(&u8WriteCmd[8], &ktBin.pReadBuff[i * 0x38], 0x38);
		for (j = 0; j < 7; ++j) {
			uint32_t ii;
			for (ii = 0; ii < 8; ++ii) {
				u8WriteCmd[8 + j * 8 + ii] ^= u8Mask[ii];
			}
		}
		u16Tmp = i * 0x38;
		u8WriteCmd[3] = (uint8_t)u16Tmp;
		u8WriteCmd[4] = (uint8_t)(u16Tmp >> 8);
		if (!Write(handle, u8WriteCmd, u8WriteCmd[1] + 3)) {
			printf("Send Write: Fail\n");
			return 1;
		}
		
		if (!Read(handle, u8Buff, u8WriteRespond)) {
			printf("Read Write: Fail\n");
			return 1;
		}
		if (u8Buff[4] != 0x00) {
			printf("Failed to write\n");
			return 1;
		}
    ktProg.SetPos(i + 1);
    ktProg.Display();
	}

	/* Verify */
	printf("\nVerify");
  ktProg.SetMax(n);
  ktProg.SetNum(50);
  ktProg.SetPos(0);
  ktProg.Display();

	for (i = 0; i < n; ++i) {
		uint16_t u16Tmp;
		uint32_t j;
		/* Verify flash */
		memmove(&u8VerifyCmd[8], &ktBin.pReadBuff[i * 0x38], 0x38);
		for (j = 0; j < 7; ++j) {
			uint32_t ii;
			for (ii = 0; ii < 8; ++ii) {
				u8VerifyCmd[8 + j * 8 + ii] ^= u8Mask[ii];
			}
		}
		u16Tmp = i * 0x38;
		u8VerifyCmd[3] = (uint8_t)u16Tmp;
		u8VerifyCmd[4] = (uint8_t)(u16Tmp >> 8);
		if (!Write(handle, u8VerifyCmd, u8VerifyCmd[1] + 3)) {
			printf("Send Verify: Fail\n");
			return 1;
		}
		
		if (!Read(handle, u8Buff, u8VerifyRespond)) {
			printf("Send Verify: Fail\n");
			return 1;
		}
		if (u8Buff[4] != 0x00) {
			printf("Failed to verify\n");
			return 1;
		}
    ktProg.SetPos(i + 1);
    ktProg.Display();
	}

	/* Reset and Run */
	Write(handle, u8ResetCmd, u8ResetCmd[1] + 3);
	printf("\nWrite complete!!!\n");

	return 0;
}
