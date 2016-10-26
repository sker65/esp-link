#ifndef CGICMDUPLOAD_H
#define CGICMDUPLOAD_H

#include <httpd.h>

int ICACHE_FLASH_ATTR cgiCmdUpload(HttpdConnData *connData);
void ICACHE_FLASH_ATTR mcu_upload(CmdPacket *cmd);

#endif /* CGIWEBSERVER_H */
