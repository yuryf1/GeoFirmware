#ifdef __CUSTOMER_CODE__

#include "custom_feature_def.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_timer.h"
#include "ql_uart.h"
#include "ql_error.h"
#include "ql_gprs.h"
#include "ql_fs.h"
#include "ril.h"
#include "ril_network.h"
#include "ril_http.h"
#include "ril_util.h"


#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT1
#define DBG_BUF_LEN   512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);\
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    if (UART_PORT2 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(DBG_BUFFER);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif


/****************************************************************************
* Define http parameters
****************************************************************************/
#define APN_NAME              "internet"
#define APN_USERID            "gdata"
#define APN_PASSWD            "gdata"
#define KEY_LOCATOR_YANDEX    "AJXKcF4BAAAA2RWLSAMAEDmifDoLfZ3_4Uvpzh9dxfXqD3AAAAAAAAAAAABSacnv5mx_St_dCqjgC4_pzhxEtw=="
#define HTTP_URL_ADDR_GET     "http://mypomoshnik.azurewebsites.net/api/device/\0"
#define HTTP_URL_ADDR_POST    "http://api.lbs.yandex.net/geolocation\0"
static char httpGetString [300];

//#define TIMEOUT_COUNT 20
static u32 Stack_timer = 0x102; // timerId =99; timerID is Specified by customer, but must ensure the timer id is unique in the opencpu task
static u32 ST_Interval = 30000;
static s32 m_param1 = 0;

u8 arrHttpRcvBuf[10*1024]; // 10K buffer for http data
static void HTTP_Program(u8 http_action, char* httpString);
static s32 AT_Command(char strAT[], char* responce);
static s32 Callback_AT_Command(char* line, u32 len, void* param); //char* param
static void SIM_Card_State_Ind(u32 sim_stat);
static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static void Timer_handler(u32 timerId, void* param);
static void Cells(void);


void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    Ql_UART_Register(UART_PORT1, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    //register  a timer
    ret = Ql_Timer_Register(Stack_timer, Timer_handler, &m_param1);
    if(ret <0)
      {
        APP_DEBUG("\r\n<--failed!!, Ql_Timer_Register: timer(%d) fail ,ret = %d -->\r\n",Stack_timer,ret);
      }
    APP_DEBUG("\r\n<--Register: timerId=%d, param = %d,ret = %d -->\r\n", Stack_timer ,m_param1,ret);

    //start a timer,repeat=true;
    ret = Ql_Timer_Start(Stack_timer,ST_Interval,TRUE);
    if(ret < 0)
      {
         APP_DEBUG("\r\n<--failed!! stack timer Ql_Timer_Start ret=%d-->\r\n",ret);
      }
    APP_DEBUG("\r\n<--stack timer Ql_Timer_Start(ID=%d,Interval=%d,) ret=%d-->\r\n",Stack_timer,ST_Interval,ret);


    while (1)
        {
            Ql_OS_GetMessage(&msg);
            switch(msg.message)
            {
                case MSG_ID_RIL_READY:
                    Ql_RIL_Initialize();
                    APP_DEBUG("<-- RIL is ready -->\r\n");
                    break;

                case MSG_ID_URC_INDICATION:
                    switch (msg.param1)
                    {
                    case URC_SYS_INIT_STATE_IND:
                        APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                        break;
                    case URC_CFUN_STATE_IND:
                        APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                        break;

                    case URC_SIM_CARD_STATE_IND:
                        SIM_Card_State_Ind(msg.param2);
                        break;

                    case URC_GSM_NW_STATE_IND:
                        if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                        {
                            APP_DEBUG("<-- Module has registered to GSM network -->\r\n");

                        }else{
                            APP_DEBUG("<-- GSM network status:%d -->\r\n", msg.param2);
                            /* status: 0 = Not registered, module not currently search a new operator
                             *         2 = Not registered, but module is currently searching a new operator
                             *         3 = Registration denied
                             */
                        }
                        break;

                    case URC_GPRS_NW_STATE_IND:
                        APP_DEBUG("<-- GPRS Network Status:%d -->\r\n", msg.param2);

                        if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                        {
                          APP_DEBUG("Internet is ready\r\n");
                        }
                        break;
                    }
                    break;
                default:
                    break;
            }
        }

}


static void Cells(void)
{
    s32 ret;
    char data[300];
	s32 a[70];
	char httpPostString[700];//= "json={\"common\":{\"version\":\"1.0\",\"api_key\":\"AJXKcF4BAAAA2RWLSAMAEDmifDoLfZ3_4Uvpzh9dxfXqD3AAAAAAAAAAAABSacnv5mx_St_dCqjgC4_pzhxEtw==\"},\"gsm_cells\":[{\"countrycode\":250,\"operatorid\":2,\"cellid\":45369,\"lac\":9057,}]}\0";

	struct cellsInfo {
	    u32 lac;
	    s32 cellId;
	    u16 countrycode;
	    u16 operatorid;
	    s8 signal_strength;
	};
	struct cellsInfo cells[6];

    ret = AT_Command("AT+QENG?\0", data);

    if (RIL_AT_SUCCESS == ret)
    {
      //"data" string to "a" array
      Ql_sscanf(data, "%s %d,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x,%d,%d,%d,%d,%d,%d,%d,%d,%x,%x",
    		  data, &a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9],&a[10],
    		     &a[11],&a[12],&a[13],&a[14],&a[15],&a[16],&a[17],&a[18],&a[19],&a[20],
    		     &a[21],&a[22],&a[23],&a[24],&a[25],&a[26],&a[27],&a[28],&a[29],&a[30],
    		     &a[31],&a[32],&a[33],&a[34],&a[35],&a[36],&a[37],&a[38],&a[39],&a[40],
    		     &a[41],&a[42],&a[43],&a[44],&a[45],&a[46],&a[47],&a[48],&a[49],&a[50],
    		     &a[51],&a[52],&a[53],&a[54],&a[55],&a[56],&a[57],&a[58],&a[59],&a[60]);

      //array "a" to "cells" structure
      for (u8 i=0; i<=5; ++i)
      {
          cells[i].signal_strength = a[3 + i*10];
          cells[i].countrycode = a[7 + i*10];
          cells[i].operatorid = a[8 + i*10];
          cells[i].lac = a[9 + i*10];
          cells[i].cellId = a[10 + i*10];

          APP_DEBUG("Cell¹: %u\r\n", i+1);
          APP_DEBUG("dbm: %d\r\n", cells[i].signal_strength);
          APP_DEBUG("mcc: %d\r\n", cells[i].countrycode);
          APP_DEBUG("mnc: %d\r\n", cells[i].operatorid);
          APP_DEBUG("lac: %d\r\n", cells[i].lac);
          APP_DEBUG("cellid: %d\r\n\r\n", cells[i].cellId);
      }

    }else{
 	  APP_DEBUG("Fail to get data from this AT command!\r\n");
    }

    // "cells" structure to json post string
    Ql_sprintf(httpPostString,"json={\"common\":{\"version\":\"1.0\",\"api_key\":\"%s\"},\"gsm_cells\":[{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d},{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d},{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d},{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d},{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d},{\"countrycode\":%d,\"operatorid\":%d,\"cellid\":%d,\"lac\":%d,\"signal_strength\":%d}]}\0",
    		KEY_LOCATOR_YANDEX, cells[0].countrycode, cells[0].operatorid, cells[0].cellId, cells[0].lac, cells[0].signal_strength,
    		                    cells[1].countrycode, cells[1].operatorid, cells[1].cellId, cells[1].lac, cells[1].signal_strength,
    		                    cells[2].countrycode, cells[2].operatorid, cells[2].cellId, cells[2].lac, cells[2].signal_strength,
    		                    cells[3].countrycode, cells[3].operatorid, cells[3].cellId, cells[3].lac, cells[3].signal_strength,
    		                    cells[4].countrycode, cells[4].operatorid, cells[4].cellId, cells[4].lac, cells[4].signal_strength,
    		                    cells[5].countrycode, cells[5].operatorid, cells[5].cellId, cells[5].lac, cells[5].signal_strength);

    //Yandex.Locator
    HTTP_Program(1, httpPostString);
    //Server
    HTTP_Program(0, httpGetString);

}


static s32 AT_Command(char strAT[], char* responce)
{
	s32 ret = Ql_RIL_SendATCmd(strAT, Ql_strlen(strAT), Callback_AT_Command, (void*)responce, 0); //cellsData
	//if (RIL_AT_SUCCESS == ret) ...

    if (NULL == responce)
    {
    	APP_DEBUG("Invalid AT parameter");
        return RIL_AT_INVALID_PARAM;
    }
    else
    {
    	APP_DEBUG("cellsData: %s\r\n", responce);
    }

    return ret;
}


//For AT responce
static s32 Callback_AT_Command(char* line, u32 len, void* param) //char* param
{
	//APP_DEBUG("Callback Line: %s\r\n", line);

    char* pHead = NULL;
    pHead = Ql_RIL_FindLine(line, len, "OK"); // find <CR><LF>OK<CR><LF>, <CR>OK<CR>£¬<LF>OK<LF>
    if (pHead)
    {
        return RIL_ATRSP_SUCCESS;
    }

    pHead = Ql_RIL_FindLine(line, len, "ERROR");// find <CR><LF>ERROR<CR><LF>, <CR>ERROR<CR>£¬<LF>ERROR<LF>
    if (pHead)
    {
        return RIL_ATRSP_FAILED;
    }

    pHead = Ql_RIL_FindString(line, len, "+CME ERROR:");//fail
    if (pHead)
    {
        return RIL_ATRSP_FAILED;
    }
    Ql_memcpy((char*)param, line, len - 2); //param
    return RIL_ATRSP_CONTINUE; //continue wait
}



void Timer_handler(u32 timerId, void* param)
{
    *((s32*)param) +=1;
    if(Stack_timer == timerId)
    {
        APP_DEBUG("<-- stack Timer_handler, param:%d -->\r\n", *((s32*)param));

        Cells();

        // stack_timer repeat
/*        if(*((s32*)param) >= TIMEOUT_COUNT)
        {
            s32 ret;
            ret = Ql_Timer_Stop(Stack_timer);
            if(ret < 0)
            {
                  APP_DEBUG("\r\n<--failed!! stack timer Ql_Timer_Stop ret=%d-->\r\n",ret);
            }
            APP_DEBUG("\r\n<--stack timer Ql_Timer_Stop(ID=%d,) ret=%d-->\r\n",Stack_timer,ret);
        }*/
    }

}


// Data from transfer (Yandex.Locator or mypomoshnik.azurewebsites.net)
static u32 m_rcvDataLen = 0;
static void HTTP_RcvData(u8* ptrData, u32 dataLen, void* reserved)
{
    APP_DEBUG("<-- Data coming on http, total len:%d -->\r\n", m_rcvDataLen + dataLen);
    APP_DEBUG("DATA: %s \r\n", (const void*)ptrData);

    char str [6][40];

    Ql_sscanf((const void*)ptrData, "%s%s%s%s%s%s", str[0], str[1], str[2], str[3], str[4], str[5]);
    Ql_sscanf(str[3], "%11s%17s", str[0], str[1]);
    Ql_sscanf(str[4], "%12s%17s", str[2], str[5]);

    //http://mypomoshnik.azurewebsites.net/api/device/37.5283088684082,56.35969543457031
    Ql_sprintf(httpGetString,"%s%s,%s", HTTP_URL_ADDR_GET, str[5], str[1]);
    APP_DEBUG("url from core: %s \r\n", httpGetString);


    if ((m_rcvDataLen + dataLen) <= sizeof(arrHttpRcvBuf))
    {
        Ql_memcpy((void*)(arrHttpRcvBuf + m_rcvDataLen), (const void*)ptrData, dataLen);
    } else {
        if (m_rcvDataLen < sizeof(arrHttpRcvBuf))
        {// buffer is not enough
            u32 realAcceptLen = sizeof(arrHttpRcvBuf) - m_rcvDataLen;
            Ql_memcpy((void*)(arrHttpRcvBuf + m_rcvDataLen), (const void*)ptrData, realAcceptLen);
            APP_DEBUG("<-- Rcv-buffer is not enough, discard part of data (len:%d/%d) -->\r\n", dataLen - realAcceptLen, dataLen);
        } else {// No more buffer
            APP_DEBUG("<-- No more buffer, discard data (len:%d) -->\r\n", dataLen);
        }
    }
    m_rcvDataLen += dataLen;
}

static void HTTP_Program(u8 http_action, char* httpString)
{
    s32 ret;
    //u32 readDataLen = 0;

    // Set PDP context
    ret = RIL_NW_SetGPRSContext(Ql_GPRS_GetPDPContextId());
    APP_DEBUG("<-- Set GPRS PDP context, ret=%d -->\r\n", ret);

    // Set APN
    ret = RIL_NW_SetAPN(1, APN_NAME, APN_USERID, APN_PASSWD);
    APP_DEBUG("<-- Set GPRS APN, ret=%d -->\r\n", ret);

    // Open/Activate PDP context
    ret = RIL_NW_OpenPDPContext();
    APP_DEBUG("<-- Open PDP context, ret=%d -->\r\n", ret);


    // Send get/post request
    m_rcvDataLen = 0;
    if (0 == http_action)
    {
        // Set HTTP server address (URL)
        ret = RIL_HTTP_SetServerURL(httpString, Ql_strlen(httpString));
        APP_DEBUG("<-- Set http server URL, ret=%d -->\r\n", ret);

        // get-request
        ret = RIL_HTTP_RequestToGet(120);   // 100s timetout
        APP_DEBUG("<-- Send get-request, ret=%d -->\r\n", ret);

        // Read response from server
        //ret = RIL_HTTP_ReadResponse(120, HTTP_RcvData);   //120s timeout
        //APP_DEBUG("<-- Read http response data, ret=%d, dataLen=%d -->\r\n", ret, m_rcvDataLen);
    }
    else if (1 == http_action)
    {
        // Set HTTP server address (URL)
        ret = RIL_HTTP_SetServerURL(HTTP_URL_ADDR_POST, Ql_strlen(HTTP_URL_ADDR_POST));
        APP_DEBUG("<-- Set http server URL, ret=%d -->\r\n", ret);

        // post-request
        ret = RIL_HTTP_RequestToPost(httpString, Ql_strlen(httpString));
        APP_DEBUG("<-- Send post-request, postMsg=json..., ret=%d -->\r\n", ret);

        // Read response from server
        ret = RIL_HTTP_ReadResponse(120, HTTP_RcvData);
        APP_DEBUG("<-- Read http response data, ret=%d, dataLen=%d -->\r\n", ret, m_rcvDataLen);
    }


    // Close PDP context
    ret = RIL_NW_ClosePDPContext();
    APP_DEBUG("<-- Close PDP context, ret=%d -->\r\n", ret);
 }


static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    APP_DEBUG("Callback_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
}

static void SIM_Card_State_Ind(u32 sim_stat)
{
    switch (sim_stat)
    {
    case SIM_STAT_NOT_INSERTED:
        APP_DEBUG("<-- SIM Card Status: NOT INSERTED -->\r\n");
    	break;
    case SIM_STAT_READY:
        APP_DEBUG("<-- SIM Card Status: READY -->\r\n");
        break;
    case SIM_STAT_PIN_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PIN -->\r\n");
        break;
    case SIM_STAT_PUK_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PUK -->\r\n");
        break;
    case SIM_STAT_PH_PIN_REQ:
        APP_DEBUG("<-- SIM Card Status: PH-SIM PIN -->\r\n");
        break;
    case SIM_STAT_PH_PUK_REQ:
        APP_DEBUG("<-- SIM Card Status: PH-SIM PUK -->\r\n");
        break;
    case SIM_STAT_PIN2_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PIN2 -->\r\n");
        break;
    case SIM_STAT_PUK2_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PUK2 -->\r\n");
        break;
    case SIM_STAT_BUSY:
        APP_DEBUG("<-- SIM Card Status: BUSY -->\r\n");
        break;
    case SIM_STAT_NOT_READY:
        APP_DEBUG("<-- SIM Card Status: NOT READY -->\r\n");
        break;
    default:
        APP_DEBUG("<-- SIM Card Status: ERROR -->\r\n");
        break;
    }
}


void proc_subtask1(s32 TaskId)
{
	u64 boot = Ql_GetMsSincePwrOn();
	APP_DEBUG("boot = %d ms\r\n", boot);

/*	int i =1;
	while(TRUE)
	{
	    MyPWM(i++);
	    i %= 50;
	}*/
}



/*void MyPWM(int i)
{
	Ql_PWM_Init(PINNAME_NETLIGHT, PWMSOURCE_32K, PWMSOURCE_DIV2, i, 2);
	Ql_PWM_Output(PINNAME_NETLIGHT, 1);
	Ql_Sleep(40);
	Ql_PWM_Output(PINNAME_NETLIGHT, 0);
	Ql_PWM_Uninit(PINNAME_NETLIGHT);
}


void MyGPIO(void)
{
	Ql_GPIO_Init(PINNAME_NETLIGHT, PINDIRECTION_OUT, PINLEVEL_LOW,  PINPULLSEL_PULLUP);
    Ql_GPIO_SetLevel(PINNAME_NETLIGHT, PINLEVEL_HIGH);
    Ql_Sleep(500);
	Ql_GPIO_SetLevel(PINNAME_NETLIGHT, PINLEVEL_LOW);
	Ql_Sleep(500);
	Ql_GPIO_Uninit(PINNAME_NETLIGHT);
}*/


#endif  __CUSTOMER_CODE__
