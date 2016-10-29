#ifndef CGICMDUPLOAD_H
#define CGICMDUPLOAD_H

#include <httpd.h>

int ICACHE_FLASH_ATTR cgiCmdUpload(HttpdConnData *connData);
void ICACHE_FLASH_ATTR mcu_upload(CmdPacket *cmd);

// gets called when mcu send response to upload
void WEB_Upload(CmdPacket *cmd);
void WEB_UploadAck(CmdPacket *cmd);

#endif /* CGIWEBSERVER_H */
