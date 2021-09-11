/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WIFI_CONNECT_H__
#define __WIFI_CONNECT_H__

typedef enum {
	NFCERR_USER_INVALID_MEMORY_PAGE,
	NFCERR_READ_USER_MEMORY_PAGE,
	NFCERR_PARSE_DATA,
	NFCERR_MALLOC_FAILED,
	NFCERR_PARSE_PJSON_FAILED,
	NFCERR_PARSE_PSSID_FAILED,
	NFCERR_PARSE_PKEY_FAILED,
	NFC_PARSE_SUCCESS,
	NFC_NEED_RECONNECT,	//只要发生账号密码更新就得重连。
}Nfc_Wifi_FLAG;
	
extern char g_lastest_NFC_WIFI_SSID[50];	//WIFI账号
extern char g_lastest_NFC_WIFI_KEY[50];	    //WIFI密码

int WifiConnect(const char *ssid,const char *psk);
uint8_t Parse_NFC_WIFIinfo(void);

#endif /* __WIFI_CONNECT_H__ */

