//
//
//  CCA.h
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#ifndef __CCA__
#define __CCA__

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
#include <future>
#include <chrono>
#include <mutex>
#include <thread>
#include <iomanip>
#include <fstream>

#include "../../licensedinterfaces/sberrorx.h"

#include "hidapi.h"
#include "StopWatch.h"

#define PLUGIN_VERSION      1.15

#define PLUGIN_DEBUG 1

#define MAX_TIMEOUT         1000
#define REPORT_SIZE         65 // 64 byte buffer + report ID
#define MAX_GOTO_RETRY      3   // 3 retiries on goto if the focuser didn't move

#define VENDOR_ID   0x20E1
#define PRODUCT_ID  0x0002

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

typedef struct cca_setting {
    std::atomic<int>            nCurPos;
    std::atomic<bool>           bIsWired;
    std::atomic<bool>           bIsAtOrigin;
    std::atomic<bool>           bIsMoving;
    std::atomic<bool>           bFanIsOn;
    std::atomic<bool>           bIsBatteryOperated;
    std::atomic<bool>           bIsHold;
    std::atomic<byte>           nDriveMode;
    std::atomic<byte>           nStepSize;
    std::atomic<byte>           nBitsFlag;    // Settings.BitFlags = (byte)(SystemState.BitFlags & -16 | (BlackoutLedCheckbox.Checked ? 2 : 0) | (AutoFanControlCheckbox.Checked ? 4 : 0) | (AutoSynchronizeCheckbox.Checked ? 8 : 0));
    std::atomic<byte>           nAirTempOffset;   // (value -128)/10.0
    std::atomic<byte>           nTubeTempOffset;  // (value -128)/10.0
    std::atomic<byte>           nMirorTempOffset; // (value -128)/10.0
    std::atomic<byte>           nDeltaT;
    std::atomic<byte>           nStillTime;
    std::string                 sVersion;
    std::atomic<word>           nBackstep;
    std::atomic<word>           nBacklash;
    std::atomic<word>           nImmpp;
    std::atomic<double>         dMillimetersPerStep;
    std::atomic<int>            nMaxPos;
    std::atomic<int>            nPreset0;
    std::atomic<int>            nPreset1;
    std::atomic<int>            nPreset2;
    std::atomic<int>            nPreset3;
    std::atomic<float>          fAirTemp;
    std::atomic<float>          fTubeTemp;
    std::atomic<float>          fMirorTemp;
    std::atomic<int>            nBacklashSteps;
} CCA_Settings;

typedef struct cca_adv_settings {
    std::atomic<word>           nMaxPps;
    std::atomic<word>           nMinPps;
    std::atomic<byte>           nGetbackRate;
    std::atomic<byte>           nBatteryMaxRate;
    std::atomic<word>           nPowerTimer;
    std::atomic<word>           nFanTimer;
    std::atomic<word>           nOriginOffset;
    std::atomic<bool>           bSetFanOn;
    std::atomic<bool>           bRestorePosition;
    std::atomic<int>            nSavedPosistion;
    std::atomic<byte>           nTorqueIndex;

} CCA_Adv_Settings;

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
    int         getFirmwareVersion(std::string &sFirmware);
    double      getTemperature();
    double      getTemperature(int nSource);
    int         getPosition(void);
    int         getPosLimit(void);
    
    int         setFanOn(bool bOn);
    bool        getFanState();
    
    void        setTemperatureSource(int nSource);
    int         getTemperatureSource();

    void        setRestorePosition(int nPosition, bool bRestoreOnConnect);
    bool        getRestoreOnConnect();

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
    char            m_szFirmwareVersion[64];

    int             m_nTargetPos;
    bool            m_bPosLimitEnabled;
    int             m_nGotoTries;

    int             m_nTempSource;

    // the read thread keep updating these
    // Takahashi focuser data for 0x3C from device
    CCA_Settings        m_CCA_Settings;

    // Takahashi focuser data for 0x11 from device
    CCA_Adv_Settings    m_CCA_Adv_Settings;

    // we need these to update the device as the above ones get refreshe constantly by the read thread
    // these get updated from the settings fialog and writen to the device using sendSettings() and sendSettings2()
    // Takahashi focuser data for 0x3C to device
    CCA_Settings        m_W_CCA_Settings;

    // Takahashi focuser data for 0x11 to device
    CCA_Adv_Settings    m_W_CCA_Adv_Settings;

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
