//
//
//  CCA.h
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#ifndef __CCA__
#define __CCA__
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#include "hidapi.h"

#define PLUGIN_VERSION      1.0

#define PLUGIN_DEBUG 3

#define DATA_BUFFER_SIZE 256
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 256

enum CCA_Errors    {PLUGIN_OK = 0, NOT_CONNECTED, CCA_CANT_CONNECT, CCA_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};
enum TempSources    {AIR, TUBE, MIRROR};
typedef unsigned char byte;

typedef enum {
    ZERO    = 0x03,
    RELEASE = 0x04,
    FREE    = 0x06,
    AUTO    = 0x07,
    MOVE    = 0x09,
    STOP    = 0x0a,
    FAN_ON  = 0x0b,
    FAN_OFF = 0x0c,
    RESET   = 0x7e,
    DUMMY   = 0xff
} Commands;


class CCCAController
{
public:
    CCCAController();
    ~CCCAController();

    int         Connect();
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int         getTemperature(double &dTemperature);
    int         getPosition(int &nPosition);
    int         getPosLimit(void);
    void        setPosLimit(int nLimit);
    
protected:

    void            parseGeneralResponse(byte *Buffer);
    int             Get32(const byte *buffer, int position);
    int             Get16(const byte *buffer, int position);
    
    SerXInterface   *m_pSerx;
    LoggerInterface *m_pLogger;
    SleeperInterface    *m_pSleeper;

    bool            m_HIDInitOk;
    hid_device      *m_DevHandle;
    byte            m_HIDBuffer[DATA_BUFFER_SIZE];
    
    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[DATA_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nTargetPos;
    bool            m_bPosLimitEnabled;
    bool            m_bMoving;

    int             m_nTempSource;
    int             m_nCurPos;
    bool            m_bIsAtOrigin;
    bool            m_bIsMoving;
    bool            m_bFanIsOn;
    bool            m_bIsHold;
    std::string     m_sVersion;
    int             m_nStepPerMillimeter;
    int             m_nMaxPos;
    float           m_fAirTemp;
    float           m_fTubeTemp;
    float           m_fMirorTemp;
    
#ifdef PLUGIN_DEBUG
    void            hexdump(const byte *inputData, byte *outBuffer, int size);
    
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif


};

#endif //__CCA__
