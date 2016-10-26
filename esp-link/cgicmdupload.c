// Copyright (c) 2015 by Thorsten von Eicken, see LICENSE.txt in the esp-link repo

#include <esp8266.h>
#include <osapi.h>
#include "cgi.h"
#include "cgioptiboot.h"
#include "multipart.h"
#include "config.h"
#include "web-server.h"
#include "cmd.h"

#define DBG(format, ...) do { os_printf(format, ## __VA_ARGS__); } while(0)

// multipart callback for uploading user defined pages
int ICACHE_FLASH_ATTR mcuUploadMultipartCallback(MultipartCmd cmd, char *data, int dataLen, int position)
{
  switch(cmd) {
    case FILE_UPLOAD_START:
    	// nothing to do
      break;
    case FILE_START:
      {
        int len = dataLen + 1;
        while(( len & 3 ) != 0 )
          len++;

        char nameBuf[len];
        os_memset(nameBuf, 0, len);
        os_memcpy(nameBuf, data, dataLen);
        DBG("starting upload file '%s'\n", nameBuf);

        cmdResponseBody(nameBuf, len);
        cmdResponseEnd();
      }
      break;
#define UPLOAD_CHUNK_SIZE 512
    case FILE_DATA:
		{
			int remain = dataLen;
			int pos = 0;
			int toSend = (remain > UPLOAD_CHUNK_SIZE) ? UPLOAD_CHUNK_SIZE : remain;
			while( toSend > 0 ) {
				DBG("sending upload data '%d'\n", toSend);
				cmdResponseStart(CMD_UPLOAD_DATA, toSend, 1);
				cmdResponseBody(data+pos, toSend);
		    	cmdResponseEnd();
				remain -= toSend;
				pos += toSend;
				toSend = (remain > UPLOAD_CHUNK_SIZE) ? UPLOAD_CHUNK_SIZE : remain;
			}
		}
      break;
    case FILE_DONE:
		cmdResponseStart(CMD_UPLOAD_END, 0, 0);
    	cmdResponseEnd();
      break;

    case FILE_UPLOAD_DONE:
      break;
  }
  return 0;
}

MultipartCtx * mcuUploadContext = NULL; // multipart upload context for web server

// this callback is called when user uploads mcu filesystem data
int ICACHE_FLASH_ATTR cgiCmdUpload(HttpdConnData *connData) {
  if( mcuUploadContext == NULL )
	  mcuUploadContext = multipartCreateContext( mcuUploadMultipartCallback );

  cmdResponseStart(CMD_UPLOAD_START, 0, 4); // number of arguments ip, port, url, filename, len, content
  cmdResponseBody(&connData->conn->proto.tcp->remote_ip, 4);                  // 2nd argument: IP
  cmdResponseBody(&connData->conn->proto.tcp->remote_port, sizeof(uint16_t)); // 3rd argument: port
  cmdResponseBody(connData->url, os_strlen(connData->url));                   // 4th argument: URL
  DBG("cgiCmdUpload called");
  return multipartProcess(mcuUploadContext, connData);
}

// this method is called when MCU transmits mcu_upload command
void ICACHE_FLASH_ATTR mcu_upload(CmdPacket *cmd) {
	// create response for the browser
	// first
}

