// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "uart.h"
#include "serbridge.h"
#include "config.h"
#include "console.h"

// Microcontroller console capturing the last 1024 characters received on the uart so
// they can be shown on a web page

// Buffer to hold concole contents.
// Invariants:
// - console_rd==console_wr <=> buffer empty
// - *console_rd == next char to read
// - *console_wr == next char to write
// - 0 <= console_xx < BUF_MAX
// - (console_wr+1)%BUF_MAX) == console_rd <=> buffer full
#define BUF_MAX (1024)
static char console_buf[BUF_MAX];
static int console_wr, console_rd;
static int console_pos; // offset since reset of buffer

static void ICACHE_FLASH_ATTR
console_write(char c) {
  console_buf[console_wr] = c;
  console_wr = (console_wr+1) % BUF_MAX;
  if (console_wr == console_rd) {
    // full, we write anyway and loose the oldest char
    console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
    console_pos++;
  }
}

#if 0
// return previous character in console, 0 if at start
static char ICACHE_FLASH_ATTR
console_prev(void) {
  if (console_wr == console_rd) return 0;
  return console_buf[(console_wr-1+BUF_MAX)%BUF_MAX];
}
#endif

void ICACHE_FLASH_ATTR
console_write_char(char c) {
  //if (c == '\n' && console_prev() != '\r') console_write('\r'); // does more harm than good
  console_write(c);
}

int ICACHE_FLASH_ATTR
ajaxConsoleReset(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  jsonHeader(connData, 200);
  console_rd = console_wr = console_pos = 0;
  serbridgeReset();
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsoleBaud(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[512];
  int len, status = 400;
  len = httpdFindArg(connData->getArgs, "rate", buff, sizeof(buff));
  if (len > 0) {
    int rate = atoi(buff);
    if (rate >= 9600 && rate <= 1000000) {
      uart0_baud(rate);
      flashConfig.baud_rate = rate;
      status = configSave() ? 200 : 400;
    }
  } else if (connData->requestType == HTTPD_METHOD_GET) {
    status = 200;
  }

  jsonHeader(connData, status);
  os_sprintf(buff, "{\"rate\": %d}", flashConfig.baud_rate);
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}
int ICACHE_FLASH_ATTR
ajaxConsoleFormat(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[16];
  int len, status = 400;
  uint32 conf0;

  len = httpdFindArg(connData->getArgs, "fmt", buff, sizeof(buff));
  if (len >= 3) {
    int c = buff[0];
    if (c >= '5' && c <= '8')
       flashConfig.data_bits = c - '5' + FIVE_BITS;
    if (buff[1] == 'N' || buff[1] == 'E')
       flashConfig.parity = buff[1] == 'E' ? EVEN_BITS : NONE_BITS;
    if (buff[2] == '1' || buff[2] == '2')
       flashConfig.stop_bits = buff[2] == '2' ? TWO_STOP_BIT : ONE_STOP_BIT;
    conf0 = CALC_UARTMODE(flashConfig.data_bits, flashConfig.parity, flashConfig.stop_bits);
    uart_config(0, flashConfig.baud_rate, conf0);
    status = configSave() ? 200 : 400;
  } else if (connData->requestType == HTTPD_METHOD_GET) {
    status = 200;
  }

  jsonHeader(connData, status);
  os_sprintf(buff, "{\"fmt\": \"%c%c%c\"}",
		  flashConfig.data_bits + '5',
		  flashConfig.parity ? 'E' : 'N',
		  flashConfig.stop_bits ? '2': '1');
  httpdSend(connData, buff, -1);
  return HTTPD_CGI_DONE;
}


int ICACHE_FLASH_ATTR
ajaxConsoleRest(HttpdConnData *connData) {
  if (connData->conn==NULL) {
	  return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  }
  char buff[2048];
  int len, status = 400;

  if (connData->cgiData == NULL ) { // first request
	  if( connData->requestType == HTTPD_METHOD_POST ) {
		  os_sprintf(buff, "{\"resource\": \"%s\", \"verb\": \"post\", \"data\": \"",
				  (char*)connData->url+12 );
		  int i = connData->post->len;
		  char *p = connData->post->buff;
		  char *t = buff + strlen(buff);
		  while( i > 0 && t < buff+2048) {
			  *t++ = *p++;
			  i--;
		  }
		  *t++ = 13; *t++ = 10; *t++ = 0;
		  console_rd = console_wr = console_pos = 0;
		  uart0_tx_buffer(buff, strlen(buff));
		  status = 200;
	  } else {
		  // path prefix: /godmd/rest/ 12 chars
		  console_rd = console_wr = console_pos = 0;
		  os_sprintf(buff, "get%s\r\n", (char*)connData->url+12);
		  uart0_tx_buffer(buff, strlen(buff));
		  status = 200;
	  }
	  status = 200;
	  noCacheHeaders(connData, status);
	  httpdHeader(connData, "Content-Type", "application/json");
	  httpdHeader(connData, "Access-Control-Allow-Origin", "*");
	  httpdEndHeaders(connData);
	  connData->cgiData = (void*) (console_rd | 0x10000); // store last read pos
	  return HTTPD_CGI_MORE;
  } else {  // sub sequent request
	  int console_rd = (int)(connData->cgiData) & 0x0FFFF;
	  int done = 0;
	  len = 0;
	  while (len < 2040 && console_rd != console_wr) {
	    uint8_t c = console_buf[console_rd];
	    if (c == '\r') {
	      // this is crummy, but browsers display a newline for \r\n sequences
	    	done= 1;
	    	break;
	    } else if (c < ' ') {
	      len += os_sprintf(buff+len, "\\u%04x", c);
	    } else {
	      buff[len++] = c;
	    }
	    console_rd = (console_rd + 1) % BUF_MAX;
	  }
	  if( len > 0 ) httpdSend(connData, buff, len);
	  if( done == 0 ) {
		  connData->cgiData = (void*)  (console_rd | 0x10000); // store last read pos
		  return HTTPD_CGI_MORE;
	  }
  }
  httpdFlush(connData);
  connData->cgiData=NULL;
  return HTTPD_CGI_DONE;

}


int ICACHE_FLASH_ATTR
ajaxConsoleSend(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[2048];
  int len, status = 400;

  // figure out where to start in buffer based on URI param
  len = httpdFindArg(connData->getArgs, "text", buff, sizeof(buff));
  if (len > 0) {
    uart0_tx_buffer(buff, len);
    status = 200;
  }

  jsonHeader(connData, status);
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsole(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[2048];
  int len; // length of text in buff
  int console_len = (console_wr+BUF_MAX-console_rd) % BUF_MAX; // num chars in console_buf
  int start = 0; // offset onto console_wr to start sending out chars

  jsonHeader(connData, 200);

  // figure out where to start in buffer based on URI param
  len = httpdFindArg(connData->getArgs, "start", buff, sizeof(buff));
  if (len > 0) {
    start = atoi(buff);
    if (start < console_pos) {
      start = 0;
    } else if (start >= console_pos+console_len) {
      start = console_len;
    } else {
      start = start - console_pos;
    }
  }

  // start outputting
  len = os_sprintf(buff, "{\"len\":%d, \"start\":%d, \"text\": \"",
      console_len-start, console_pos+start);

  int rd = (console_rd+start) % BUF_MAX;
  while (len < 2040 && rd != console_wr) {
    uint8_t c = console_buf[rd];
    if (c == '\\' || c == '"') {
      buff[len++] = '\\';
      buff[len++] = c;
    } else if (c == '\r') {
      // this is crummy, but browsers display a newline for \r\n sequences
    } else if (c < ' ') {
      len += os_sprintf(buff+len, "\\u%04x", c);
    } else {
      buff[len++] = c;
    }
    rd = (rd + 1) % BUF_MAX;
  }
  os_strcpy(buff+len, "\"}"); len+=2;
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR consoleInit() {
  console_wr = 0;
  console_rd = 0;
}


