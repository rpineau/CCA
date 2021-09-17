//
//
//  CCA.h
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#ifndef __CCA__
#define __CCA__

#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif

#include <string>
#include <vector>
#include <sstream>
#include <future>
#include <chrono>
#include <mutex>
#include <thread>
#include <iomanip>
#include <fstream>

#include "../../licensedinterfaces/sberrorx.h"

#include "hidapi.h"
#include "StopWatch.h"

#define PLUGIN_VERSION      1.1

#define PLUGIN_DEBUG 3

#define DATA_BUFFER_SIZE    64
#define MAX_TIMEOUT         1000
#define LOG_BUFFER_SIZE     256
#define REPORT_0_SIZE   8
#define REPORT_1_SIZE   3

#define LOCAL_DEBUG
#ifndef LOCAL_DEBUG
#define VENDOR_ID   0x20E1
#define PRODUCT_ID  0x0002
#else
// just to test HID stuff with my only HID device that is not a keyboard or a mouse
#define VENDOR_ID 0x2341    // Arduino
#define PRODUCT_ID 0x8036   // Leonardo
#endif

enum CCA_Errors    {PLUGIN_OK = 0, NOT_CONNECTED, CCA_CANT_CONNECT, CCA_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};
enum TempSources    {AIR, TUBE, MIRROR};
typedef unsigned char byte;
typedef unsigned short word;

typedef enum {
    Clockwise = 0x01,
    CounterClockwise = 0x02,
    Zero = 0x03,
    Release = 0x04,
    Function = 0x05,
    Free = 0x06,
    Auto = 0x07,
    Manual = 0x08,
    Move = 0x09,
    Stop = 0x0A,
    FanOn = 0x0B,
    FanOff = 0x0C,
    Settings2 = 0x7C,
    Settings = 0x7D,
    Reset = 0x7E,
    Dummy = 0xFF
} Commands;



class CCCAController
{
public:
    CCCAController();
    ~CCCAController();

    int         Connect();
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    double      getTemperature();
    double      getTemperature(int nSource);
    int         getPosition(void);
    int         getPosLimit(void);
    
    int         setFanOn(bool bOn);
    bool        getFanState();
    
    void        setTemperatureSource(int nSource);
    int         getTemperatureSource();

    void        setRestorePosition(int nPosition, bool bRestoreOnConnect);
    
    void        parseResponse(byte *Buffer, int nLength);
    int         sendSettings();
    int         sendSettings2();
    int         resetToFactoryDefault();

    std::mutex          m_GlobalMutex;
    std::mutex          m_DevAccessMutex;

protected:

    int             Get32(const byte *buffer, int position);
    int             Get16(const byte *buffer, int position);

    void            put32(byte *buffer, int position, int value);
    void            put16(byte *buffer, int position, int value);

    hid_device      *m_DevHandle;
    
    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[DATA_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nTargetPos;
    bool            m_bPosLimitEnabled;

    int             m_nTempSource;

    // the read thread keep updating these
    // Takahashi focuser data for 0x3C from device
    std::atomic<int>            m_nCurPos;
    std::atomic<bool>           m_bIsWired;
    std::atomic<bool>           m_bIsAtOrigin;
    std::atomic<bool>           m_bIsMoving;
    std::atomic<bool>           m_bFanIsOn;
    std::atomic<bool>           m_bIsBatteryOperated;
    std::atomic<bool>           m_bIsHold;
    std::atomic<byte>           m_nDriveMode;
    std::atomic<byte>           m_nStepSize;
    std::atomic<byte>           m_nBitsFlag;    // Settings.BitFlags = (byte)(SystemState.BitFlags & -16 | (BlackoutLedCheckbox.Checked ? 2 : 0) | (AutoFanControlCheckbox.Checked ? 4 : 0) | (AutoSynchronizeCheckbox.Checked ? 8 : 0));
    std::atomic<byte>           m_nAirTempOffset;   // (value -128)/10.0
    std::atomic<byte>           m_nTubeTempOffset;  // (value -128)/10.0
    std::atomic<byte>           m_nMirorTempOffset; // (value -128)/10.0
    std::atomic<byte>           m_nDeltaT;
    std::atomic<byte>           m_nStillTime;
    std::string                 m_sVersion;
    std::atomic<word>           m_nBackstep;
    std::atomic<word>           m_nBacklash;
    std::atomic<word>           m_nImmpp;
    std::atomic<double>         m_dMillimetersPerStep;
    std::atomic<int>            m_nMaxPos;
    std::atomic<int>            m_nPreset0;
    std::atomic<int>            m_nPreset1;
    std::atomic<int>            m_nPreset2;
    std::atomic<int>            m_nPreset3;
    std::atomic<float>          m_fAirTemp;
    std::atomic<float>          m_fTubeTemp;
    std::atomic<float>          m_fMirorTemp;
    std::atomic<int>            m_nBacklashSteps;
    
    // Takahashi focuser data for 0x11 from device
    std::atomic<word>           m_nMaxPps;
    std::atomic<word>           m_nMinPps;
    std::atomic<byte>           m_nGetbackRate;
    std::atomic<byte>           m_nBatteryMaxRate;
    std::atomic<word>           m_nPowerTimer;
    std::atomic<word>           m_nFanTimer;
    std::atomic<word>           m_nOriginOffset;
    std::atomic<bool>           m_bSetFanOn;
    std::atomic<bool>           m_bRestorePosition;
    std::atomic<int>            m_nSavedPosistion;
    std::atomic<byte>           m_nTorqueIndex;

    // we need these to update the device as the above ones get refreshe constantly by the read thread
    // these get updated from the settings fialog and writen to the device using sendSettings() and sendSettings2()
    // Takahashi focuser data for 0x3C to device
    std::atomic<byte>           m_W_nDriveMode;
    std::atomic<byte>           m_W_nStepSize;
    std::atomic<byte>           m_W_nBitsFlag;          // Settings.BitFlags = (byte)(SystemState.BitFlags & -16 | (BlackoutLedCheckbox.Checked ? 2 : 0) | (AutoFanControlCheckbox.Checked ? 4 : 0) | (AutoSynchronizeCheckbox.Checked ? 8 : 0));
    std::atomic<byte>           m_W_nAirTempOffset;     // int((float value * 10.0) + 128)
    std::atomic<byte>           m_W_nTubeTempOffset;    // int((float value * 10.0) + 128)
    std::atomic<byte>           m_W_nMirorTempOffset;   // int((float value * 10.0) + 128)
    std::atomic<byte>           m_W_nDeltaT;
    std::atomic<byte>           m_W_nStillTime;
    std::atomic<word>           m_W_nBackstep;
    std::atomic<word>           m_W_nBacklash;
    std::atomic<word>           m_W_nImmpp;
    std::atomic<int>            m_W_nMaxPos;
    std::atomic<int>            m_W_nPreset0;
    std::atomic<int>            m_W_nPreset1;
    std::atomic<int>            m_W_nPreset2;
    std::atomic<int>            m_W_nPreset3;

    // Takahashi focuser data for 0x11 to device
    std::atomic<word>           m_W_nMaxPps;
    std::atomic<word>           m_W_nMinPps;
    std::atomic<byte>           m_W_nTorqueIndex;
    std::atomic<byte>           m_W_nGetbackRate;
    std::atomic<byte>           m_W_nBatteryMaxRate;
    std::atomic<word>           m_W_nPowerTimer;
    std::atomic<word>           m_W_nFanTimer;
    std::atomic<word>           m_W_nOriginOffset;


    CStopWatch      m_cmdTimer;
    CStopWatch      m_gotoTimer;

    // threads
    bool                m_ThreadsAreRunning;
    std::promise<void> *m_exitSignal;
    std::future<void>   m_futureObj;
    std::promise<void> *m_exitSignalSender;
    std::future<void>   m_futureObjSender;
    std::thread         m_th;
    std::thread         m_thSender;


#ifdef PLUGIN_DEBUG
    void    hexdump(const byte *inputData, int inputSize,  std::string &outHex);
    // timestamp for logs
    const std::string getTimeStamp();
    std::string hexOut;
    std::ofstream m_sLogFile;
    std::string m_sPlatform;
    std::string m_sLogfilePath;
#endif


};

#endif //__CCA__
