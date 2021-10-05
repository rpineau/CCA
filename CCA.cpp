//
//  CCA.cpp
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#include "CCA.h"

void threaded_sender(std::future<void> futureObj, CCCAController *CCAControllerObj, hid_device *hidDevice)
{
    const byte cmdData[3] = {0x00, 0x01, Dummy};

    while (futureObj.wait_for(std::chrono::milliseconds(1000)) == std::future_status::timeout) {
        if(hidDevice && CCAControllerObj && CCAControllerObj->m_DevAccessMutex.try_lock()) {
            hid_write(hidDevice, cmdData, sizeof(cmdData));
            CCAControllerObj->m_DevAccessMutex.unlock();
        }
        else {
            std::this_thread::yield();
        }
    }
}

void threaded_poller(std::future<void> futureObj, CCCAController *CCAControllerObj, hid_device *hidDevice)
{
    int nbRead;
    byte cHIDBuffer[DATA_BUFFER_SIZE];

    while (futureObj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout) {
        if(hidDevice && CCAControllerObj && CCAControllerObj->m_DevAccessMutex.try_lock()) {
            nbRead = hid_read(hidDevice, cHIDBuffer, sizeof(cHIDBuffer));
            CCAControllerObj->m_DevAccessMutex.unlock();
            if(nbRead>0){
                CCAControllerObj->parseResponse(cHIDBuffer, nbRead);
            }
        }
        else {
            std::this_thread::yield();
        }
    }
}

CCCAController::CCCAController()
{
    m_bDebugLog = false;
    m_bIsConnected = false;
    m_CCA_Settings.nCurPos = 0;
    m_nTargetPos = 0;
    m_nTempSource = AIR;
    m_DevHandle = nullptr;
    m_CCA_Settings.bIsAtOrigin = false;
    m_CCA_Settings.bIsMoving = false;
    m_CCA_Settings.bFanIsOn = false;
    m_CCA_Settings.bIsHold = false;
    m_CCA_Settings.sVersion.clear();
    m_CCA_Settings.nImmpp = 52;
    m_CCA_Settings.dMillimetersPerStep = m_CCA_Settings.nImmpp / 1000000.0;
    m_CCA_Settings.nMaxPos = 192307;
    m_CCA_Settings.fAirTemp = -100.0;
    m_CCA_Settings.fTubeTemp = -100.0;
    m_CCA_Settings.fMirorTemp = -100.0;

    m_W_CCA_Settings.nStepSize = 7;
    m_W_CCA_Settings.nBitsFlag = 40;
    m_W_CCA_Settings.nAirTempOffset = 128;
    m_W_CCA_Settings.nTubeTempOffset = 128;
    m_W_CCA_Settings.nMirorTempOffset = 128;
    m_W_CCA_Settings.nDeltaT = 30;
    m_W_CCA_Settings.nStillTime = 10;
    m_W_CCA_Settings.nImmpp = 52;
    m_W_CCA_Settings.nBackstep = 961;
    m_W_CCA_Settings.nBacklash = 384;
    m_W_CCA_Settings.nMaxPos = 192307;
    m_W_CCA_Settings.nPreset0 = 0;
    m_W_CCA_Settings.nPreset1 = 0;
    m_W_CCA_Settings.nPreset2 = 0;
    m_W_CCA_Settings.nPreset3 = 0;

    m_W_CCA_Adv_Settings.nMaxPps = 18000;
    m_W_CCA_Adv_Settings.nMinPps = 250;
    m_W_CCA_Adv_Settings.nTorqueIndex = 2;
    m_W_CCA_Adv_Settings.nGetbackRate = 120;
    m_W_CCA_Adv_Settings.nBatteryMaxRate = 43;
    m_W_CCA_Adv_Settings.nPowerTimer = 10;
    m_W_CCA_Adv_Settings.nFanTimer = 600;
    m_W_CCA_Adv_Settings.nOriginOffset = 0;

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
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CCCAController] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CCCAController] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif
}

CCCAController::~CCCAController()
{

    if(m_bIsConnected)
        Disconnect();
    
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
    
}

int CCCAController::Connect()
{
    int nErr = PLUGIN_OK;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    // vendor id is : 0x20E1 and the product id is : 0x0002.
    m_DevHandle = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
    if (!m_DevHandle) {
        m_bIsConnected = false;
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] hid_open failed for vendor id " << std::uppercase << std::setfill('0') << std::setw(4) << std::hex <<  VENDOR_ID << " product id " << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << PRODUCT_ID << std::dec << std::endl;
        m_sLogFile.flush();
#endif
        return CCA_CANT_CONNECT;
    }
    m_bIsConnected = true;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected to vendor id " << std::uppercase << std::setfill('0') << std::setw(4) << std::hex <<  VENDOR_ID << " product id " << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << PRODUCT_ID << std::dec << std::endl;
    m_sLogFile.flush();
#endif

    // Set the hid_read() function to be non-blocking.
    hid_set_nonblocking(m_DevHandle, 1);
    if(!m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Starting HID threads." << std::endl;
        m_sLogFile.flush();
#endif
        m_exitSignal = new std::promise<void>();
        m_futureObj = m_exitSignal->get_future();
        m_exitSignalSender = new std::promise<void>();
        m_futureObjSender = m_exitSignalSender->get_future();
        
        m_th = std::thread(&threaded_poller, std::move(m_futureObj), this, m_DevHandle);
        m_thSender = std::thread(&threaded_sender, std::move(m_futureObjSender), this,  m_DevHandle);
        m_ThreadsAreRunning = true;
    }
    
    setFanOn(m_W_CCA_Adv_Settings.bSetFanOn);

    if(m_W_CCA_Adv_Settings.bRestorePosition) {
        gotoPosition(m_W_CCA_Adv_Settings.nSavedPosistion);
    }
    
    return nErr;
}

void CCCAController::Disconnect()
{
    const std::lock_guard<std::mutex> lock(m_DevAccessMutex);

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Disconnecting from device." << std::endl;
    m_sLogFile.flush();
#endif
    if(m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Waiting for threads to exit." << std::endl;
        m_sLogFile.flush();
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
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Calling hid_exit." << std::endl;
    m_sLogFile.flush();
#endif

    hid_exit();
    m_DevHandle = nullptr;
	m_bIsConnected = false;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Disconnected from device." << std::endl;
    m_sLogFile.flush();
#endif
}

#pragma mark move commands
int CCCAController::haltFocuser()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    memset(cHIDBuffer, 0, DATA_BUFFER_SIZE);
    if(m_CCA_Settings.bIsMoving) {
        cHIDBuffer[0] = 0x00; // report ID
        cHIDBuffer[1] = 0x01; // command length
        cHIDBuffer[2] = Stop; // command

    #ifdef PLUGIN_DEBUG
        hexdump(cHIDBuffer,  REPORT_1_SIZE, hexOut);
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] sending : " << hexOut << std::endl;
        m_sLogFile.flush();
    #endif
        if(m_DevAccessMutex.try_lock()) {
            nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_1_SIZE);
            m_DevAccessMutex.unlock();
            if(nByteWriten<0)
                nErr = ERR_CMDFAILED;
        }
        else {
            nErr = ERR_CMDFAILED;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // give time to the thread to read the returned report
    return nErr;
}

int CCCAController::gotoPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE];

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    if (nPos>m_CCA_Settings.nMaxPos)
        return ERR_LIMITSEXCEEDED;

    memset(cHIDBuffer, 0, DATA_BUFFER_SIZE);
    if(m_CCA_Settings.bIsHold && !m_CCA_Settings.bIsMoving) {
    #ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] goto :  " << std::dec << nPos << " (0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << nPos <<")" << std::dec << std::endl;
        m_sLogFile.flush();
    #endif
        cHIDBuffer[0] = 0x00; // report ID
        cHIDBuffer[1] = 0x05; // size is 5 bytes
        cHIDBuffer[2] = Move; // command = move
        cHIDBuffer[3] = byte((nPos &0xFF00000) >> 24) ;
        cHIDBuffer[4] = byte((nPos &0x00FF000) >> 16);
        cHIDBuffer[5] = byte((nPos &0x0000FF00) >> 8);
        cHIDBuffer[6] = byte(nPos &0x000000FF);
        // the rest of the buffer contains all zero because of the memset above

    #ifdef PLUGIN_DEBUG
        hexdump(cHIDBuffer,  REPORT_0_SIZE, hexOut);
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] sending : " << hexOut << std::endl;
        m_sLogFile.flush();
    #endif

        if(m_DevAccessMutex.try_lock()) {
            nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_0_SIZE);
            m_DevAccessMutex.unlock();
            if(nByteWriten<0)
                nErr = ERR_CMDFAILED;
        }
        else {
            nErr = ERR_CMDFAILED;
        }
    #ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] nByteWriten : " << nByteWriten << std::endl;
        m_sLogFile.flush();
    #endif
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // give time to the thread to read the returned report
    m_gotoTimer.Reset();
    return nErr;
}

int CCCAController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [moveRelativeToPosision] goto relative position : " << nSteps << std::endl;
    m_sLogFile.flush();
#endif

    m_nTargetPos = m_CCA_Settings.nCurPos + nSteps;
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

    if(m_gotoTimer.GetElapsedSeconds()<1.5) { // focuser take a bit of time to start moving and reporting it's moving.
        return nErr;
    }
    
    if(m_CCA_Settings.bIsMoving) {
        return nErr;
    }
    bComplete = true;

    m_CCA_Settings.nCurPos = getPosition();
    if(m_CCA_Settings.nCurPos != m_nTargetPos) {
        // we have an error as we're not moving but not at the target position
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isGoToComplete] **** ERROR **** Not moving and not at the target position." << std::endl;
        m_sLogFile.flush();
#endif
        m_nTargetPos = m_CCA_Settings.nCurPos;
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}

#pragma mark getters and setters

int CCCAController::getFirmwareVersion(std::string &sFirmware)
{
    int nErr = PLUGIN_OK;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;
    sFirmware.clear();
    if(m_GlobalMutex.try_lock()) {
        if(m_CCA_Settings.sVersion.size()) {
            sFirmware.assign(m_CCA_Settings.sVersion);
        }
        else {
            sFirmware = "NA";
        }
        m_GlobalMutex.unlock();
    }
    else
        sFirmware = "NA";

    return nErr;
}

double CCCAController::getTemperature()
{
    // need to allow user to select the focuser temp source
    switch(m_nTempSource) {
        case AIR:
            return m_CCA_Settings.fAirTemp;
            break;
        case TUBE:
            return m_CCA_Settings.fTubeTemp;
            break;
        case MIRROR:
            return  m_CCA_Settings.fMirorTemp;
            break;
        default:
            return m_CCA_Settings.fAirTemp;
            break;
    }
}

double CCCAController::getTemperature(int nSource)
{
    switch(nSource) {
        case AIR:
            return m_CCA_Settings.fAirTemp;
            break;
        case TUBE:
            return m_CCA_Settings.fTubeTemp;
            break;
        case MIRROR:
            return m_CCA_Settings.fMirorTemp;
            break;
        default:
            return m_CCA_Settings.fAirTemp;
            break;
    }
}


int CCCAController::getPosition()
{

    return m_CCA_Settings.nCurPos;
}


int CCCAController::getPosLimit()
{
    return m_CCA_Settings.nMaxPos;
}


int CCCAController::setFanOn(bool bOn)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int i=0;
    int nbRead;

    byte cHIDBuffer[DATA_BUFFER_SIZE];
    byte cHIDBufferIn[DATA_BUFFER_SIZE];
    
    m_W_CCA_Adv_Settings.bSetFanOn = bOn;

    if(!m_bIsConnected) {
        return ERR_COMMNOLINK;
    }
    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    memset(cHIDBuffer, 0, DATA_BUFFER_SIZE);

    cHIDBuffer[0] = 0x00; // report ID
    cHIDBuffer[1] = 0x01; // command length
    if(bOn)
        cHIDBuffer[2] = FanOn; // command
    else
        cHIDBuffer[2] = FanOff; // command
    // the rest of the buffer contains all zero because of the memset above

    // the Takahashi ASCOM drvier 1.1.2 seems to send the command 3 times..
    for(i=0; i<3; i++) {
#ifdef PLUGIN_DEBUG
        hexdump(cHIDBuffer,  REPORT_1_SIZE, hexOut);
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setFanOn] " << i << " sending : " << hexOut << std::endl;
        m_sLogFile.flush();
#endif
        if(m_DevAccessMutex.try_lock()) {
            nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_1_SIZE);
            m_DevAccessMutex.unlock();
            if(nByteWriten<0) {
#ifdef PLUGIN_DEBUG
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setFanOn] nByteWriten : " << nByteWriten << std::endl;
                m_sLogFile.flush();
#endif
                return ERR_CMDFAILED;
            }
        }
        else {
#ifdef PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setFanOn] Couldn't lock device" << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // give time to the thread to read the returned report
    }

    return nErr;
}

bool CCCAController::getFanState()
{
    if(!m_bIsConnected)
        return m_W_CCA_Adv_Settings.bSetFanOn;
    else
        return m_CCA_Settings.bFanIsOn;
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
    m_W_CCA_Adv_Settings.bRestorePosition = bRestoreOnConnect;
    m_W_CCA_Adv_Settings.nSavedPosistion = nPosition;
    
#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setRestorePosition] m_bRestorePosition : " << (m_W_CCA_Adv_Settings.bRestorePosition?"Yes":"No") << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setRestorePosition] m_nSavedPosistion : " << m_W_CCA_Adv_Settings.nSavedPosistion << std::endl;
    m_sLogFile.flush();
#endif

}

#pragma mark command and response functions


void CCCAController::parseResponse(byte *Buffer, int nLength)
{

    // locking the mutex to prevent access while we're accessing to the data.
    const std::lock_guard<std::mutex> lock(m_GlobalMutex);
    int nTmp;

#ifdef PLUGIN_DEBUG
    hexdump(Buffer,  nLength, hexOut);
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] Buffer size " << std::dec << nLength <<", content : " << std::endl << hexOut << std::endl;
    m_sLogFile.flush();
#endif

    if((Buffer[0] == 0x3C) && (nLength >=64)) {
        m_CCA_Settings.nCurPos               = Get32(Buffer, 2);
        m_CCA_Settings.bIsWired              = (Buffer[6] == 0);
        m_CCA_Settings.bIsAtOrigin           = (Buffer[7] & 128) != 0;
        m_CCA_Settings.bIsMoving             = (Buffer[7] &  64) != 0;
        m_CCA_Settings.bFanIsOn              = (Buffer[7] &  32) != 0;
        m_CCA_Settings.bIsBatteryOperated    = (Buffer[7] &  16) != 0;
        m_CCA_Settings.bIsHold               = (Buffer[7] &   3) != 0;
        m_CCA_Settings.nDriveMode            = Buffer[8];
        m_CCA_Settings.nStepSize             = Buffer[9];
        m_CCA_Settings.nBitsFlag             = Buffer[11];
        m_CCA_Settings.nAirTempOffset        = Buffer[12];
        m_CCA_Settings.nTubeTempOffset       = Buffer[13];
        m_CCA_Settings.nMirorTempOffset      = Buffer[14];
        m_CCA_Settings.nDeltaT               = Buffer[15];
        m_CCA_Settings.nStillTime            = Buffer[16];
        m_CCA_Settings.sVersion              = std::to_string(Get16(Buffer, 17)>>8) + "." + std::to_string(Get16(Buffer, 17) & 0xFF);
        m_CCA_Settings.nBackstep             = Get16(Buffer, 19);
        m_CCA_Settings.nBacklash             = Get16(Buffer, 21);
        m_CCA_Settings.nImmpp                = Get16(Buffer,23);
        m_CCA_Settings.dMillimetersPerStep   = m_CCA_Settings.nImmpp / 1000000.0;
        m_CCA_Settings.nMaxPos               = Get32(Buffer, 25);

        nTmp                    = Get32(Buffer, 29);
        if(nTmp<0)
            nTmp = 0;
        else if(nTmp > m_CCA_Settings.nMaxPos)
            nTmp = m_CCA_Settings.nMaxPos;
        m_CCA_Settings.nPreset0 = nTmp;

        nTmp                    = Get32(Buffer, 33);
        if(nTmp<0)
            nTmp = 0;
        else if(nTmp > m_CCA_Settings.nMaxPos)
            nTmp = m_CCA_Settings.nMaxPos;
        m_CCA_Settings.nPreset1 = nTmp;

        nTmp                    = Get32(Buffer, 37);
        if(nTmp<0)
            nTmp = 0;
        else if(nTmp > m_CCA_Settings.nMaxPos)
            nTmp = m_CCA_Settings.nMaxPos;
        m_CCA_Settings.nPreset2 = nTmp;

        nTmp                    = Get32(Buffer, 42);
        if(nTmp<0)
            nTmp = 0;
        else if(nTmp > m_CCA_Settings.nMaxPos)
            nTmp = m_CCA_Settings.nMaxPos;
        m_CCA_Settings.nPreset3 = nTmp;

        m_CCA_Settings.fAirTemp              = float(Get32(Buffer, 45)) / 10.0;
        m_CCA_Settings.fTubeTemp             = float(Get32(Buffer, 49)) / 10.0;
        m_CCA_Settings.fMirorTemp            = float(Get32(Buffer, 53)) / 10.0;
        m_CCA_Settings.nBacklashSteps        = Get32(Buffer, 57);
         
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nCurPos                : " << std::dec << m_CCA_Settings.nCurPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bIsWired               : " << (m_CCA_Settings.bIsWired?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bIsAtOrigin            : " << (m_CCA_Settings.bIsAtOrigin?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bIsMoving              : " << (m_CCA_Settings.bIsMoving?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bFanIsOn               : " << (m_CCA_Settings.bFanIsOn?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bIsBatteryOperated     : " << (m_CCA_Settings.bIsBatteryOperated?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_bIsHold                : " << (m_CCA_Settings.bIsHold?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nDriveMode             : " << std::dec << int(m_CCA_Settings.nDriveMode) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nStepSize              : " << std::dec << int(m_CCA_Settings.nStepSize) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nBitsFlag              : " << std::dec << int(m_CCA_Settings.nBitsFlag) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nAirTempOffset         : " << std::dec << int(m_CCA_Settings.nAirTempOffset) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nTubeTempOffset        : " << std::dec << int(m_CCA_Settings.nTubeTempOffset) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nMirorTempOffset       : " << std::dec << int(m_CCA_Settings.nMirorTempOffset) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nDeltaT                : " << std::dec << int(m_CCA_Settings.nDeltaT) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nStillTime             : " << std::dec << int(m_CCA_Settings.nStillTime) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_sVersion               : " << m_CCA_Settings.sVersion << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nBackstep              : " << std::dec << int(m_CCA_Settings.nBackstep) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nBacklash              : " << std::dec << int(m_CCA_Settings.nBacklash) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nImmpp                 : " << std::dec << m_CCA_Settings.nImmpp << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_dMillimetersPerStep    : " << std::fixed << std::setprecision(6) << m_CCA_Settings.dMillimetersPerStep << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nMaxPos                : " << std::dec << m_CCA_Settings.nMaxPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nPreset0               : " << std::dec << m_CCA_Settings.nPreset0 << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nPreset1               : " << std::dec << m_CCA_Settings.nPreset1 << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nPreset2               : " << std::dec << m_CCA_Settings.nPreset2 << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nPreset3               : " << std::dec << m_CCA_Settings.nPreset3 << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_fAirTemp               : " << std::fixed << std::setprecision(2) << m_CCA_Settings.fAirTemp << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_fTubeTemp              : " << std::fixed << std::setprecision(2) << m_CCA_Settings.fTubeTemp << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_fMirorTemp             : " << std::fixed << std::setprecision(2) << m_CCA_Settings.fMirorTemp << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nBacklashSteps         : " << std::dec << m_CCA_Settings.nBacklashSteps << std::endl;
        m_sLogFile.flush();
#endif
    }
    if((Buffer[0] == 0x11) && (nLength>= 16)) {
        m_CCA_Adv_Settings.nMaxPps           = Get16(Buffer, 2);
        m_CCA_Adv_Settings.nMinPps           = Get16(Buffer, 4);
        m_CCA_Adv_Settings.nGetbackRate      = Buffer[7];
        m_CCA_Adv_Settings.nBatteryMaxRate   = Buffer[8];
        m_CCA_Adv_Settings.nPowerTimer       = Get16(Buffer, 10);
        m_CCA_Adv_Settings.nFanTimer         = Get16(Buffer, 12);
        m_CCA_Adv_Settings.nOriginOffset     = Get16(Buffer, 14);
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nMaxPps            : " << std::dec << m_CCA_Adv_Settings.nMaxPps << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nMinPps            : " << std::dec << m_CCA_Adv_Settings.nMinPps << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nGetbackRate       : " << std::dec << int(m_CCA_Adv_Settings.nGetbackRate) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nBatteryMaxRate    : " << std::dec << int(m_CCA_Adv_Settings.nBatteryMaxRate) << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nPowerTimer        : " << std::dec << m_CCA_Adv_Settings.nPowerTimer << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nFanTimer          : " << std::dec << m_CCA_Adv_Settings.nFanTimer << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [parseResponse] m_nOriginOffset      : " << std::dec << m_CCA_Adv_Settings.nOriginOffset << std::endl;
        m_sLogFile.flush();
#endif
    }

}

int CCCAController::sendSettings()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    memset(cHIDBuffer, 0, DATA_BUFFER_SIZE);

    cHIDBuffer[0] = 0x00; // report ID
    cHIDBuffer[1] = 36; // size = 1+1+1+1+1+1+1+1+1+1+2+2+2+4+4+4+4+4
    cHIDBuffer[2] = Settings; // command
    cHIDBuffer[3] = 4; // m_W_nDriveMode;
    cHIDBuffer[4] = m_W_CCA_Settings.nStepSize;
    cHIDBuffer[5] = 0; // ??
    cHIDBuffer[6] = m_W_CCA_Settings.nBitsFlag;
    cHIDBuffer[7] = m_W_CCA_Settings.nAirTempOffset;
    cHIDBuffer[8] = m_W_CCA_Settings.nTubeTempOffset;
    cHIDBuffer[9] = m_W_CCA_Settings.nMirorTempOffset;
    cHIDBuffer[10] = m_W_CCA_Settings.nDeltaT;
    cHIDBuffer[11] = m_W_CCA_Settings.nStillTime;
    put16(cHIDBuffer, 12, m_W_CCA_Settings.nImmpp);
    put16(cHIDBuffer, 14, m_W_CCA_Settings.nBackstep);
    put16(cHIDBuffer, 16, m_W_CCA_Settings.nBacklash);
    put32(cHIDBuffer, 18, m_W_CCA_Settings.nMaxPos);
    put32(cHIDBuffer, 22, m_W_CCA_Settings.nPreset0);
    put32(cHIDBuffer, 26, m_W_CCA_Settings.nPreset1);
    put32(cHIDBuffer, 30, m_W_CCA_Settings.nPreset2);
    put32(cHIDBuffer, 34, m_W_CCA_Settings.nPreset3);

#ifdef PLUGIN_DEBUG
    hexdump(cHIDBuffer,  REPORT_0_SIZE, hexOut);
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [sendSettings] sending : " << hexOut << std::endl;
    m_sLogFile.flush();
#endif

    if(m_DevAccessMutex.try_lock()) {
        nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_0_SETTINGS_SIZE);
        m_DevAccessMutex.unlock();
        if(nByteWriten<0)
            nErr = ERR_CMDFAILED;
    }
    else {
        nErr = ERR_CMDFAILED;
    }
#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [sendSettings] nByteWriten : " << nByteWriten << std::endl;
    m_sLogFile.flush();
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // give time to the thread to read the returned report
    return nErr;
}

int CCCAController::sendSettings2()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    byte cHIDBuffer[DATA_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(!m_DevHandle)
        return ERR_COMMNOLINK;

    memset(cHIDBuffer, 0, DATA_BUFFER_SIZE);

    cHIDBuffer[0] = 0x00; // report ID
    cHIDBuffer[1] = 16; // size = 1+2+2+1+1+1+2+2+4
    cHIDBuffer[2] = Settings2; // command
    put16(cHIDBuffer, 3, m_W_CCA_Adv_Settings.nMaxPps);
    put16(cHIDBuffer, 5, m_W_CCA_Adv_Settings.nMinPps);
    cHIDBuffer[7] = m_W_CCA_Adv_Settings.nTorqueIndex;
    cHIDBuffer[8] = m_W_CCA_Adv_Settings.nGetbackRate;
    cHIDBuffer[9] = m_W_CCA_Adv_Settings.nBatteryMaxRate;
    put16(cHIDBuffer, 11, m_W_CCA_Adv_Settings.nPowerTimer);
    put16(cHIDBuffer, 13, m_W_CCA_Adv_Settings.nFanTimer);
    put32(cHIDBuffer, 15, m_W_CCA_Adv_Settings.nOriginOffset);

#ifdef PLUGIN_DEBUG
    hexdump(cHIDBuffer,  REPORT_0_SIZE, hexOut);
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [sendSettings2] sending : " << hexOut << std::endl;
    m_sLogFile.flush();
#endif

    if(m_DevAccessMutex.try_lock()) {
        nByteWriten = hid_write(m_DevHandle, cHIDBuffer, REPORT_0_SETTINGS2_SIZE);
        m_DevAccessMutex.unlock();
        if(nByteWriten<0)
            nErr = ERR_CMDFAILED;
    }
    else {
        nErr = ERR_CMDFAILED;
    }
#ifdef PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [sendSettings2] nByteWriten : " << nByteWriten << std::endl;
    m_sLogFile.flush();
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // give time to the thread to read the returned report
    return nErr;
}


int CCCAController::resetToFactoryDefault()
{
    int nErr = PLUGIN_OK;
    // factory settings
    m_W_CCA_Settings.nStepSize = 7;
    m_W_CCA_Settings.nBitsFlag = 40;
    m_W_CCA_Settings.nAirTempOffset = 128;
    m_W_CCA_Settings.nTubeTempOffset = 128;
    m_W_CCA_Settings.nMirorTempOffset = 128;
    m_W_CCA_Settings.nDeltaT = 30;
    m_W_CCA_Settings.nStillTime = 10;
    m_W_CCA_Settings.nImmpp = 52;
    m_W_CCA_Settings.nBackstep = 961;
    m_W_CCA_Settings.nBacklash = 384;
    m_W_CCA_Settings.nMaxPos = 192307;
    m_W_CCA_Settings.nPreset0 = 0;
    m_W_CCA_Settings.nPreset1 = 0;
    m_W_CCA_Settings.nPreset2 = 0;
    m_W_CCA_Settings.nPreset3 = 0;

    m_W_CCA_Adv_Settings.nMaxPps = 18000;
    m_W_CCA_Adv_Settings.nMinPps = 250;
    m_W_CCA_Adv_Settings.nTorqueIndex = 2;
    m_W_CCA_Adv_Settings.nGetbackRate = 120;
    m_W_CCA_Adv_Settings.nBatteryMaxRate = 43;
    m_W_CCA_Adv_Settings.nPowerTimer = 10;
    m_W_CCA_Adv_Settings.nFanTimer = 600;
    m_W_CCA_Adv_Settings.nOriginOffset = 0;

    nErr = sendSettings();
    nErr |= sendSettings2();

    return nErr;
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

void CCCAController::put32(byte *buffer, int position, int value)
{
    buffer[position    ] = byte((value>>24)  & 0xff);
    buffer[position + 1] = byte((value>>16)  & 0xff);
    buffer[position + 2] = byte((value>>8)  & 0xff);
    buffer[position + 3] = byte(value & 0xff);
}

void CCCAController::put16(byte *buffer, int position, int value)
{
    buffer[position    ] = byte((value>>8)  & 0xff);
    buffer[position + 1] = byte(value & 0xff);
}


#ifdef PLUGIN_DEBUG
void  CCCAController::hexdump(const byte *inputData, int inputSize,  std::string &outHex)
{
    int idx=0;
    std::stringstream ssTmp;

    outHex.clear();
    for(idx=0; idx<inputSize; idx++){
        if((idx%16) == 0 && idx>0)
            ssTmp << std::endl;
        ssTmp << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)inputData[idx] <<" ";
    }
    outHex.assign(ssTmp.str());
}

const std::string CCCAController::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
