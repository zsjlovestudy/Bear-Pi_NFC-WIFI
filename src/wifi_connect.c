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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "nfc.h"
#include "NT3H.h"

#include <cJSON.h>

#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"
#include "lwip/api_shell.h"

#include "cmsis_os2.h"
#include "hos_types.h"
#include "wifi_device.h"
#include "wifiiot_errno.h"
#include "ohos_init.h"
#include "wifi_connect.h"

#define DEF_TIMEOUT 15
#define ONE_SECOND 1

#define SELECT_WIFI_SECURITYTYPE WIFI_SEC_TYPE_PSK  

static void WiFiInit(void);
static void WaitSacnResult(void);
static int WaitConnectResult(void);
static void OnWifiScanStateChangedHandler(int state, int size);
static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info);
static void OnHotspotStaJoinHandler(StationInfo *info);
static void OnHotspotStateChangedHandler(int state);
static void OnHotspotStaLeaveHandler(StationInfo *info);

static int g_staScanSuccess = 0;
static int g_ConnectSuccess = 0;
static int ssid_count = 0;
WifiEvent g_wifiEventHandler = {0};
WifiErrorCode error;

#define SELECT_WLAN_PORT "wlan0"

int WifiConnect(const char *ssid, const char *psk)
{
    WifiScanInfo *info = NULL;
    unsigned int size = WIFI_SCAN_HOTSPOT_LIMIT;
    static struct netif *g_lwip_netif = NULL;

    osDelay(200);
    printf("<--System Init-->\r\n");

    //初始化WIFI
    WiFiInit();

    //使能WIFI
    if (EnableWifi() != WIFI_SUCCESS)
    {
        printf("EnableWifi failed, error = %d\r\n", error);
        return -1;
    }

    //判断WIFI是否激活
    if (IsWifiActive() == 0)
    {
        printf("Wifi station is not actived.\r\n");
        return -1;
    }

    //分配空间，保存WiFi信息
    info = malloc(sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);
    if (info == NULL)
    {
        return -1;
    }
    //轮询查找WiFi列表
    do{
        //重置标志位
        ssid_count = 0;
        g_staScanSuccess = 0;

        //开始扫描
        Scan();

        //等待扫描结果
        WaitSacnResult();

        //获取扫描列表
        error = GetScanInfoList(info, &size);

    }while(g_staScanSuccess != 1);
    //打印WiFi列表
    printf("********************\r\n");
    for(uint8_t i = 0; i < ssid_count; i++)
    {
        printf("no:%03d, ssid:%-30s, rssi:%5d\r\n", i+1, info[i].ssid, info[i].rssi/100);
    }
    printf("********************\r\n");
    
    //连接指定的WiFi热点
    for(uint8_t i = 0; i < ssid_count; i++)
    {
        if (strcmp(ssid, info[i].ssid) == 0)
        {
            int result;

            printf("Select:%3d wireless, Waiting...\r\n", i+1);

            //拷贝要连接的热点信息
            WifiDeviceConfig select_ap_config = {0};
            strcpy(select_ap_config.ssid, info[i].ssid);
            strcpy(select_ap_config.preSharedKey, psk);
            select_ap_config.securityType = SELECT_WIFI_SECURITYTYPE;

            if (AddDeviceConfig(&select_ap_config, &result) == WIFI_SUCCESS)
            {
                if (ConnectTo(result) == WIFI_SUCCESS && WaitConnectResult() == 1)
                {
                    printf("WiFi connect succeed!\r\n");
                    g_lwip_netif = netifapi_netif_find(SELECT_WLAN_PORT);
                    break;
                }
				else 
				{
					printf("WiFi connect failed!\r\n");
					goto __exit;
				}
            }
        }

        if(i == ssid_count-1)
        {
            printf("ERROR: No wifi as expected\r\n");
            //while(1) osDelay(100);
            goto __exit;
        }
    }
     //启动DHCP
    if (g_lwip_netif)
    {
        dhcp_start(g_lwip_netif);
        printf("begain to dhcp\r\n");
    }


    //等待DHCP
    for(;;)
    {
        if(dhcp_is_bound(g_lwip_netif) == ERR_OK)
        {
            printf("<-- DHCP state:OK -->\r\n");

            //打印获取到的IP信息
            netifapi_netif_common(g_lwip_netif, dhcp_clients_info_show, NULL);
            break;
        }

        printf("<-- DHCP state:Inprogress -->\r\n");
        osDelay(100);
    }

    osDelay(100);

    return 0;
	
__exit:
	free(info);
	return -1;
	
}







#define NFC_WIFI_USED_PAGE_NUM 10
uint8_t NFC_PAGE_BUF[NFC_WIFI_USED_PAGE_NUM * NFC_PAGE_SIZE];
uint8_t NFC_WIFI_PackHeader[4] = {0x02, 0x65, 0x6E, 0x7B};
uint8_t NFC_WIFI_PackEnder = 0x7D;

char g_lastest_NFC_WIFI_SSID[50];	//WIFI账号
char g_lastest_NFC_WIFI_KEY[50];	//WIFI密码


static uint8_t Parse_Data(uint8_t *str1, uint16_t len1, uint8_t* str2, uint16_t len2)
{
	uint16_t n, m;
	uint16_t j = 0, k = 0;

	for(n = 0; n < len1; n++){
		if(str1[j] == str2[k]){
			if (len2 == 1)return n;
			for(m = 1; m < len2; m++){
				if(str1[j + m] != str2[k + m])break;
				else {
					if(m == (len2 - 1))return n;
				}
			}
		}else{
			j++;
		}
	}
	return 0;
}


uint8_t Parse_NFC_WIFIinfo(void)
{
	uint8_t i;
	uint8_t res;
	uint16_t len;
	
	static char* s_NFC_WIFI_JSON_STR;	//提取出字符串给json解包
	char* s_NFC_WIFI_SSID;	//WIFI账号 
	char* s_NFC_WIFI_KEY;	//WIFI密码 
	cJSON * pJson;
	cJSON * pSSID;
	cJSON * pKEY;
	
    // if the requested page is out of the register exit with error
    if ((USER_START_REG  + NFC_WIFI_USED_PAGE_NUM) > USER_END_REG) {
		printf("NT3HERROR_INVALID_USER_MEMORY_PAGE\r\n");
        return NFCERR_USER_INVALID_MEMORY_PAGE;
    }
	
	for(i = 0; i < NFC_WIFI_USED_PAGE_NUM; i++){	
		//res = readTimeout((USER_START_REG + i), NFC_PAGE_BUF[i * NFC_PAGE_SIZE]);
		res = NT3HReadUserData(i);
		memcpy(&NFC_PAGE_BUF[i * NFC_PAGE_SIZE], nfcPageBuffer, NFC_PAGE_SIZE);
		if(res == 0){
			printf("NT3HERROR_READ_USER_MEMORY_PAGE\r\n");
			return	NFCERR_READ_USER_MEMORY_PAGE;
		}
	}
	
	res = Parse_Data(NFC_PAGE_BUF, NFC_WIFI_USED_PAGE_NUM*NFC_PAGE_SIZE, NFC_WIFI_PackHeader, 4);
  	if(res != 0){
		len = Parse_Data(NFC_PAGE_BUF, NFC_WIFI_USED_PAGE_NUM*NFC_PAGE_SIZE, &NFC_WIFI_PackEnder, 1);
 	}else 	return NFCERR_PARSE_DATA;	//parse fail

	s_NFC_WIFI_JSON_STR = (char*)malloc((len-res+1-3+1));//len-res+1为数据长度，3为前三个标志，1为结束符。
	if(NULL == s_NFC_WIFI_JSON_STR)
    {
		return NFCERR_MALLOC_FAILED;	//malloc fail
    }
	memset(s_NFC_WIFI_JSON_STR, 0, (len-res+1-3+1));
	memcpy(s_NFC_WIFI_JSON_STR, &NFC_PAGE_BUF[res+3], (len-res+1-3));
	//printf("%s\r\n", s_NFC_WIFI_JSON_STR);
	
	pJson = cJSON_Parse(s_NFC_WIFI_JSON_STR);
	if(NULL == pJson)
    {
		res = NFCERR_PARSE_PJSON_FAILED;    
		goto __pJsonERROR;	//parse fail	
    }
	
	pSSID = cJSON_GetObjectItem(pJson, "ssid");
	if(NULL == pSSID)
    {
		res = NFCERR_PARSE_PSSID_FAILED;
		goto __pSSIDERROR;  //get object named "ssid" faild
    }
	s_NFC_WIFI_SSID = pSSID->valuestring;
	
	pKEY = cJSON_GetObjectItem(pJson, "key");
	if(NULL == pKEY)
	{
		res = NFCERR_PARSE_PKEY_FAILED;
		goto __pKEYERROR;	//get object named "key" fail
	}
	s_NFC_WIFI_KEY = pKEY->valuestring;

	if(strcmp(g_lastest_NFC_WIFI_SSID, s_NFC_WIFI_SSID) != 0 || strcmp(g_lastest_NFC_WIFI_KEY, s_NFC_WIFI_KEY) != 0){
		//need update wifi connect
		strcpy(g_lastest_NFC_WIFI_SSID, s_NFC_WIFI_SSID);
		strcpy(g_lastest_NFC_WIFI_KEY, s_NFC_WIFI_KEY);
		if(g_ConnectSuccess == 1)
		{
			Disconnect();
			printf("g_ConnectSuccess = %d\n", g_ConnectSuccess);	
		}
		DisableWifi();
		UnRegisterWifiEvent(&g_wifiEventHandler);
		res = NFC_NEED_RECONNECT;		
		goto __exit;
	}

	res = NFC_PARSE_SUCCESS;
__exit:
	cJSON_Delete(pKEY);
	cJSON_Delete(pSSID);
	cJSON_Delete(pJson);
	free(s_NFC_WIFI_JSON_STR);	
	//printf("ssid = %s  key = %s\r\n", g_lastest_NFC_WIFI_SSID, g_lastest_NFC_WIFI_KEY);
	return res;	//success
	
__pKEYERROR:
	cJSON_Delete(pSSID);
__pSSIDERROR:
	cJSON_Delete(pJson);
__pJsonERROR:
	free(s_NFC_WIFI_JSON_STR);	
	return res;	
}























static void WiFiInit(void)
{
    printf("<--Wifi Init-->\r\n");
    g_wifiEventHandler.OnWifiScanStateChanged = OnWifiScanStateChangedHandler;
    g_wifiEventHandler.OnWifiConnectionChanged = OnWifiConnectionChangedHandler;
    g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;
    error = RegisterWifiEvent(&g_wifiEventHandler);
    if (error != WIFI_SUCCESS)
    {
        printf("register wifi event fail!\r\n");
    }
    else
    {
        printf("register wifi event succeed!\r\n");
    }
}

static void OnWifiScanStateChangedHandler(int state, int size)
{
    if (size > 0)
    {
        ssid_count = size;
        g_staScanSuccess = 1;
    }
    printf("callback function for wifi scan:%d, %d\r\n", state, size);
    return;
}

static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info)
{
    if (info == NULL)
    {
        printf("WifiConnectionChanged:info is null, stat is %d.\n", state);
    }
    else
    {
        if (state == WIFI_STATE_AVALIABLE)
        {
            g_ConnectSuccess = 1;
        }
        else
        {
            g_ConnectSuccess = 0;
        }
    }
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    (void)info;
    printf("STA join AP\n");
    return;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    (void)info;
    printf("HotspotStaLeave:info is null.\n");
    return;
}

static void OnHotspotStateChangedHandler(int state)
{
    printf("HotspotStateChanged:state is %d.\n", state);
    return;
}

static void WaitSacnResult(void)
{
    int scanTimeout = DEF_TIMEOUT;
    while (scanTimeout > 0)
    {
        sleep(ONE_SECOND);
        scanTimeout--;
        if (g_staScanSuccess == 1)
        {
            printf("WaitSacnResult:wait success[%d]s\n", (DEF_TIMEOUT - scanTimeout));
            break;
        }
    }
    if (scanTimeout <= 0)
    {
        printf("WaitSacnResult:timeout!\n");
    }
}

static int WaitConnectResult(void)
{
    int ConnectTimeout = DEF_TIMEOUT;
    while (ConnectTimeout > 0)
    {
        sleep(ONE_SECOND);
        ConnectTimeout--;
        if (g_ConnectSuccess == 1)
        {
            printf("WaitConnectResult:wait success[%d]s\n", (DEF_TIMEOUT - ConnectTimeout));
            break;
        }
    }
    if (ConnectTimeout <= 0)
    {
        printf("WaitConnectResult:timeout!\n");
        return 0;
    }

    return 1;
}
