//
//  CCA.cpp
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#include "CCA.h"

void threaded_sender(std::future<void> futureObj, hid_device *hidDevice)
{

    while (futureObj.wait_for(std::chrono::milliseconds(1000)) == std::future_status::timeout) {
        
        const byte cmdData[3] = {0x00, 0x01, Dummy};
        if(hidDevice) {
            hid_write(hidDevice, cmdData, sizeof(cmdData));
        }
    }
}

void threaded_poller(std::future<void> futureObj, CCCAController *CCAControllerObj, hid_device *hidDevice)
{
    int nbRead;
#ifdef LOCAL_DEBUG
    byte cHIDBuffer[DATA_BUFFER_SIZE] = {0x3C, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA1, 0x04, 0x07, 0x00, 0x2A, 0x80, 0x80, 0x80, 0x1E, 0x0A, 0x01, 0x10, 0x03, 0xC1, 0x01, 0x80, 0x00, 0x34, 0x00, 0x02, 0xEF, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x01, 0xF0, 0x08, 0x5C};
    nbRead = 64;
#else
    byte cHIDBuffer[DATA_BUFFER_SIZE];
#endif
    
    while (futureObj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout) {
        if(hidDevice) {
#ifndef LOCAL_DEBUG
            nbRead = hid_read(hidDevice, cHIDBuffer, sizeof(cHIDBuffer));
#endif
            if(nbRead>0){
                if(CCAControllerObj) {
                    CCAControllerObj->parseResponse(cHIDBuffer, nbRead);
                }
            }
        }
    }
}

CCCAController::CCCAController()
{
    m_bDebugLog = false;
    m_bIsConnected = false;
    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nTempSource = AIR;
    m_DevHandle = nullptr;
    m_bIsAtOrigin = false;
    m_bIsMoving = false;
    m_bFanIsOn = false;
    m_bIsHold = false;
    m_sVersion.clear();
    m_dMillimetersPerStep = 0;
    m_nMaxPos = 192307;
    m_fAirTemp = -100.0;
    m_fTubeTemp = -100.0;
    m_fMirorTemp = -100.0;

    m_cmdTimer.Reset();
    
    m_ThreadsAreRunning = false;
    
#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\CCA-Log.txt";
    m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CCA-Log.txt";
    m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CCA-Log.txt";
    m_sPlatform = "macOS";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController::CCCAController] Version %3.2f build 2021_02_05_1705 on %s.\n", timestamp, PLUGIN_VERSION, m_sPlatform.c_str());
    fprintf(Logfile, "[%s] [CCCAController::CCCAController] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif
}

CCCAController::~CCCAController()
{

    if(m_bIsConnected)
        Disconnect();
    
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if (Logfile)
        fclose(Logfile);
#endif
    
}

int CCCAController::Connect()
{
    int nErr = PLUGIN_OK;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CCCAController::Connect] Called\n", timestamp);
	fflush(Logfile);
#endif

    // vendor id is : 0x20E1 and the product id is : 0x0002.
    m_DevHandle = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
    if (!m_DevHandle) {
        m_bIsConnected = false;
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Connect] hid_open failed for vendor id 0x%04X product id 0x%04X\n", timestamp, VENDOR_ID, PRODUCT_ID);
        fflush(Logfile);
#endif
        return CCA_CANT_CONNECT;
    }
    m_bIsConnected = true;

#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Connect] Connected to vendor id 0x%04X product id 0x%04X\n", timestamp, VENDOR_ID, PRODUCT_ID);
        fflush(Logfile);
#endif

    // Set the hid_read() function to be non-blocking.
    hid_set_nonblocking(m_DevHandle, 1);
    if(!m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Connect] Starting HID threads\n", timestamp);
        fflush(Logfile);
#endif
        m_exitSignal = new std::promise<void>();
        m_futureObj = m_exitSignal->get_future();
        m_exitSignalSender = new std::promise<void>();
        m_futureObjSender = m_exitSignalSender->get_future();
        
        m_th = std::thread(&threaded_poller, std::move(m_futureObj), this, m_DevHandle);
        m_thSender = std::thread(&threaded_sender, std::move(m_futureObjSender), m_DevHandle);
        m_ThreadsAreRunning = true;
    }
    
    setFanOn(m_bSetFanOn);

    if(m_bRestorePosition) {
        gotoPosition(m_nSavedPosistion);
    }
    
    return nErr;
}

void CCCAController::Disconnect()
{
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Disconnect] disconnecting from device\n", timestamp);
        fflush(Logfile);
#endif
    if(m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Disconnect] Waiting for threads to exit\n", timestamp);
        fflush(Logfile);
#endif
        m_exitSignal->set_value();
        m_exitSignalSender->set_value();
        m_th.join();
        m_thSender.join();
        delete m_exitSignal;
        delete m_exitSignalSender;
        m_exitSignal = nullptr;
        m_exitSignalSender = nullptr;
        m_ThreadsAreRunning = false;
    }

    if(m_bIsConnected)
            hid_close(m_DevHandle);

#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::Disconnect] calling hid_exit\n", timestamp);
        fflush(Logfile);
#endif

    hid_exit();
    m_DevHandle = nullptr;
	m_bIsConnected = false;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController::Disconnect] disconnected from device\n", timestamp);
    fflush(Logfile);
#endif
}

#pragma mark move commands
int CCCAController::haltFocuser()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE + 1];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    if(m_bIsMoving) {
        cHIDBuffer[0] = 0x00; // report ID
        cHIDBuffer[1] = 0x01; // command length
        cHIDBuffer[2] = Stop; // command

    #ifdef PLUGIN_DEBUG
        byte hexBuffer[DATA_BUFFER_SIZE * 4];
        hexdump(cHIDBuffer, hexBuffer, REPORT_1_SIZE);
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::haltFocuser] sending : %s\n", timestamp, hexBuffer);
        fflush(Logfile);
    #endif

        nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_1_SIZE);
        if(nByteWriten<0)
            nErr = ERR_CMDFAILED;
    }
    return nErr;
}

int CCCAController::gotoPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE + 1];

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    if (nPos>m_nMaxPos)
        return ERR_LIMITSEXCEEDED;

    if(m_bIsHold && !m_bIsMoving) {
    #ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::gotoPosition] goto position  : %d (0x%08X)\n", timestamp, nPos, nPos);
        fflush(Logfile);
    #endif
        cHIDBuffer[0] = 0x00; // report ID
        cHIDBuffer[1] = 0x05; // size is 5 bytes
        cHIDBuffer[2] = Move; // command = move
        cHIDBuffer[3] = (nPos &0xFF00000) >> 24 ;
        cHIDBuffer[4] = (nPos &0x00FF000) >> 16;
        cHIDBuffer[5] = (nPos &0x0000FF00) >> 8;
        cHIDBuffer[6] = (nPos &0x000000FF);
        cHIDBuffer[7] = 0x00;
        
    #ifdef PLUGIN_DEBUG
        byte hexBuffer[DATA_BUFFER_SIZE * 4];
        hexdump(cHIDBuffer, hexBuffer, REPORT_0_SIZE);
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::gotoPosition] sending : %s\n", timestamp, hexBuffer);
        fflush(Logfile);
    #endif

        nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_0_SIZE);
        if(nByteWriten<0)
            nErr = ERR_CMDFAILED;

    #ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::gotoPosition] nByteWriten = %d\n", timestamp, nByteWriten);
        fflush(Logfile);
    #endif
    }
    m_gotoTimer.Reset();
    return nErr;
}

int CCCAController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CCCAController::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CCCAController::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    bComplete = false;

    if(m_gotoTimer.GetElapsedSeconds()<1.0) { // focuser take a bit of time to start moving and reporting it's moving.
        return nErr;
    }
    
    if(m_bIsMoving) {
        return nErr;
    }
    bComplete = true;

    m_nCurPos = getPosition();
    if(m_nCurPos != m_nTargetPos) {
        // we have an error as we're not moving but not at the target position
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::isGoToComplete] **** ERROR **** Not moving and not at the target position\n", timestamp);
        fflush(Logfile);
#endif
        m_nTargetPos = m_nCurPos;
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}

#pragma mark getters and setters

int CCCAController::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = PLUGIN_OK;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    if(m_sVersion.size()) {
        strncpy(pszVersion, m_sVersion.c_str(), nStrMaxLen);
    }
    else
        strncpy(pszVersion, "NA", nStrMaxLen);

    return nErr;
}

double CCCAController::getTemperature()
{
    // need to allow user to select the focuser temp source
    switch(m_nTempSource) {
        case AIR:
            return m_fAirTemp;
            break;
        case TUBE:
            return m_fTubeTemp;
            break;
        case MIRROR:
            return  m_fMirorTemp;
            break;
        default:
            return m_fAirTemp;
            break;
    }
}

double CCCAController::getTemperature(int nSource)
{
    switch(nSource) {
        case AIR:
            return m_fAirTemp;
            break;
        case TUBE:
            return m_fTubeTemp;
            break;
        case MIRROR:
            return m_fMirorTemp;
            break;
        default:
            return m_fAirTemp;
            break;
    }
}


int CCCAController::getPosition()
{

    return m_nCurPos;
}


int CCCAController::getPosLimit()
{
    return m_nMaxPos;
}


int CCCAController::setFanOn(bool bOn)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE + 1];

    
    m_bSetFanOn = bOn;

    if(!m_bIsConnected) {
        return ERR_COMMNOLINK;
    }
    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    cHIDBuffer[0] = 0x00; // report ID
    cHIDBuffer[1] = 0x01; // command length
    if(bOn)
        cHIDBuffer[2] = FanOn; // command
    else
        cHIDBuffer[2] = FanOff; // command

   

#ifdef PLUGIN_DEBUG
    byte hexBuffer[DATA_BUFFER_SIZE * 4];
    hexdump(cHIDBuffer, hexBuffer, REPORT_1_SIZE);
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController::setFanOn] sending : %s\n", timestamp, hexBuffer);
    fflush(Logfile);
#endif

    nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_1_SIZE);
    if(nByteWriten<0) {
        nErr = ERR_CMDFAILED;
    }
    
    return nErr;
}

bool CCCAController::getFanState()
{
    if(!m_bIsConnected)
        return m_bSetFanOn;
    else
        return m_bFanIsOn;
}

void CCCAController::setTemperatureSource(int nSource)
{
    m_nTempSource = nSource;
}

int CCCAController::getTemperatureSource()
{
    return m_nTempSource;

}

void CCCAController::setRestorePosition(int nPosition, bool bRestoreOnConnect)
{
    m_bRestorePosition = bRestoreOnConnect;
    m_nSavedPosistion = nPosition;
    
#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController::setRestorePosition] m_bRestorePosition = %s\n", timestamp, m_bRestorePosition?"Yes":"No");
    fprintf(Logfile, "[%s] [CCCAController::setRestorePosition] m_nSavedPosistion = %d\n", timestamp, m_nSavedPosistion);
    fflush(Logfile);
#endif

}

#pragma mark command and response functions


void CCCAController::parseResponse(byte *Buffer, int nLength)
{

#ifdef PLUGIN_DEBUG
        byte hexBuffer[DATA_BUFFER_SIZE * 4];
        hexdump(Buffer, hexBuffer, (nLength > DATA_BUFFER_SIZE-1 ? DATA_BUFFER_SIZE -1 : nLength));
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] Buffer size %d, content :\n%s\n", timestamp, nLength, hexBuffer);
        fflush(Logfile);
#endif

    if((Buffer[0] == 0x3C) && (nLength >=64)) {
        m_nCurPos               = Get32(Buffer, 2);
        m_bIsWired              = (Buffer[6] == 0);
        m_bIsAtOrigin           = (Buffer[7] & 128) != 0;
        m_bIsMoving             = (Buffer[7] &  64) != 0;
        m_bFanIsOn              = (Buffer[7] &  32) != 0;
        m_bIsHold               = (Buffer[7] &   3) != 0;
        m_nDriveMode            = Buffer[8];
        m_nStepSize             = Buffer[9];
        m_nBitsFlag             = Buffer[11];
        m_nAirTempOffset        = Buffer[12];
        m_nTubeTempOffset       = Buffer[13];
        m_nMirorTempOffset      = Buffer[14];
        m_nDeltaT               = Buffer[15];
        m_nStillTime            = Buffer[16];
        m_sVersion              = std::to_string(Get16(Buffer, 17)>>8) + "." + std::to_string(Get16(Buffer, 17) & 0xFF);
        m_nBackstep             = Get16(Buffer, 19);
        m_nBacklash             = Get16(Buffer, 21);
        m_dMillimetersPerStep   = Get16(Buffer,23) / 1000000.0;
        m_nMaxPos               = Get32(Buffer, 25);
        m_nPreset0              = Get32(Buffer, 29);
        m_nPreset1              = Get32(Buffer, 33);
        m_nPreset2              = Get32(Buffer, 37);
        m_nPreset3              = Get32(Buffer, 41);
        m_fAirTemp              = Get32(Buffer, 45) / 10.0;
        m_fTubeTemp             = Get32(Buffer, 49) / 10.0;
        m_fMirorTemp            = Get32(Buffer, 53) / 10.0;
        m_nBacklashSteps        = Get32(Buffer, 57);
         
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nCurPos             : %d\n", timestamp, m_nCurPos);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_bIsWired            : %s\n", timestamp, m_bIsWired?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_bIsAtOrigin         : %s\n", timestamp, m_bIsAtOrigin?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_bIsMoving           : %s\n", timestamp, m_bIsMoving?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_bFanIsOn            : %s\n", timestamp, m_bFanIsOn?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_bIsHold             : %s\n", timestamp, m_bIsHold?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nDriveMode          : %d\n", timestamp, m_nDriveMode);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nStepSize           : %d\n", timestamp, m_nStepSize);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nBitsFlag           : %d\n", timestamp, m_nBitsFlag);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nAirTempOffset      : %d\n", timestamp, m_nAirTempOffset);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nTubeTempOffset     : %d\n", timestamp, m_nTubeTempOffset);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nMirorTempOffset    : %d\n", timestamp, m_nMirorTempOffset);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nDeltaT             : %d\n", timestamp, m_nDeltaT);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nStillTime          : %d\n", timestamp, m_nStillTime);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_sVersion            : %s\n", timestamp, m_sVersion.c_str());
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nBackstep           : %d\n", timestamp, m_nBackstep);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nBacklash           : %d\n", timestamp, m_nBacklash);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_dMillimetersPerStep : %f\n", timestamp, m_dMillimetersPerStep);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nMaxPos             : %d\n", timestamp, m_nMaxPos);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nPreset0            : %d\n", timestamp, m_nPreset0);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nPreset1            : %d\n", timestamp, m_nPreset1);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nPreset2            : %d\n", timestamp, m_nPreset2);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nPreset3            : %d\n", timestamp, m_nPreset3);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_fAirTemp            : %3.2f\n", timestamp, m_fAirTemp);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_fTubeTemp           : %3.2f\n", timestamp, m_fTubeTemp);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_fMirorTemp          : %3.2f\n", timestamp, m_fMirorTemp);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nBacklashSteps      : %d\n", timestamp, m_nBacklashSteps);
        fflush(Logfile);
#endif
         if(m_bFanIsOn != m_bSetFanOn)
             setFanOn(m_bSetFanOn);
    }
    if((Buffer[0] == 0x11) && (nLength>= 16)) {
        m_nMaxPps           = Get16(Buffer, 2);
        m_nMinPps           = Get16(Buffer, 4);
        m_nGetbackRate      = Buffer[7];
        m_nBatteryMaxRate   = Buffer[8];
        m_nPowerTimer       = Get16(Buffer, 10);
        m_nFanTimer         = Get16(Buffer, 12);
        m_nOriginOffset     = Get16(Buffer, 14);
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nMaxPps             : %d\n", timestamp, m_nMaxPps);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nMinPps             : %d\n", timestamp, m_nMinPps);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nGetbackRate        : %d\n", timestamp, m_nGetbackRate);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nBatteryMaxRate     : %d\n", timestamp, m_nBatteryMaxRate);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nPowerTimer         : %d\n", timestamp, m_nPowerTimer);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nTubeTempOffset     : %d\n", timestamp, m_nTubeTempOffset);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nMirorTempOffset    : %d\n", timestamp, m_nMirorTempOffset);
        fprintf(Logfile, "[%s] [CCCAController::parseResponse] m_nOriginOffset       : %d\n", timestamp, m_nOriginOffset);
        fflush(Logfile);
#endif
    }

}

int CCCAController::Get32(const byte *buffer, int position)
{

    int num = 0;
    for (int i = 0; i < 4; ++i) {
        num = num << 8 | buffer[position + i];
    }
    return num;

}

int CCCAController::Get16(const byte *buffer, int position)
{

    return buffer[position] << 8 | buffer[position + 1];

}
#ifdef PLUGIN_DEBUG
void  CCCAController::hexdump(const byte* inputData, byte *outBuffer, int size)
{
    byte *buf = outBuffer;
    int idx=0;
    for(idx=0; idx<size; idx++){
        snprintf((char *)buf,4,"%02X ", inputData[idx]);
        buf+=3;
    }
    *buf = 0;
}

#endif

