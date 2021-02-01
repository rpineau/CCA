//
//  CCA.cpp
//  Takahashi CCA X2 plugin
//
//  Created by Rodolphe Pineau on 2012/01/16.


#include "CCA.h"



CCCAController::CCCAController()
{

    m_pSerx = NULL;
    m_pLogger = NULL;


    m_bDebugLog = false;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_bMoving = false;
    m_DevHandle = 0;

    m_nTempSource = AIR;
    
    m_bIsAtOrigin = false;
    m_bIsMoving = false;
    m_bFanIsOn = false;
    m_bIsHold = false;
    m_sVersion.clear();
    m_nStepPerMillimeter = 0;
    m_nMaxPos = 0;
    m_fAirTemp = 0.0;
    m_fTubeTemp = 0.0;
    m_fMirorTemp = 0.0;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\CCA-Log.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CCA-Log.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/CCA-Log.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController] Version %3.2f build 2021_02_01_1100.\n", timestamp, PLUGIN_VERSION);
    fprintf(Logfile, "[%s] [CCCAController] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

    
    if (hid_init())
        m_HIDInitOk = false;
    else
        m_HIDInitOk = true;
}

CCCAController::~CCCAController()
{
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if (Logfile)
        fclose(Logfile);
#endif
    if(m_HIDInitOk)
        hid_exit();
}

int CCCAController::Connect()
{
    int nErr = PLUGIN_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CCCAController::Connect Called\n", timestamp);
	fflush(Logfile);
#endif

    // vendor id is : 0x20E1 and the product id is : 0x0002.
    m_DevHandle = hid_open(0x20E1, 0x0002, NULL);
    if (!m_DevHandle) {
        m_bIsConnected = false;
        return CCA_CANT_CONNECT;
    }
    m_bIsConnected = true;

    // Set the hid_read() function to be non-blocking.
    hid_set_nonblocking(m_DevHandle, 1);
    

    if (m_bDebugLog && m_pLogger) {
        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CCCAController::Connect] Connected.\n");
        m_pLogger->out(m_szLogBuffer);

        snprintf(m_szLogBuffer,LOG_BUFFER_SIZE,"[CCCAController::Connect] Getting Firmware.\n");
        m_pLogger->out(m_szLogBuffer);
    }

    nErr = getFirmwareVersion(m_szFirmwareVersion, DATA_BUFFER_SIZE);
    if(nErr) {
		m_bIsConnected = false;
#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CCCAController::Connect **** ERROR **** getting device status\n", timestamp);
		fflush(Logfile);
#endif
        return nErr;
    }
    // m_globalStatus.deviceType now contains the device type
    return nErr;
}

void CCCAController::Disconnect()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];

    // release the focuser once we're not connected to it
    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = RELEASE; // command

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);

    if(m_bIsConnected )
        hid_close(m_DevHandle);
    
    m_DevHandle = 0;
	m_bIsConnected = false;
}

#pragma mark move commands
int CCCAController::haltFocuser()
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];

    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = STOP; // command

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);
    return nErr;
}

int CCCAController::gotoPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];
    
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (nPos>m_nMaxPos)
        return ERR_LIMITSEXCEEDED;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CCCAController::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif
    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x00; // report ID
    m_HIDBuffer[1] = 0x05; // size is 5 bytes
    m_HIDBuffer[2] = MOVE; // command = move
    m_HIDBuffer[3] = (nPos &0xFF00000) >> 24 ;
    m_HIDBuffer[4] = (nPos &0x00FF000) >> 16;
    m_HIDBuffer[5] = (nPos &0x0000FF00) >> 8;
    m_HIDBuffer[6] = (nPos &0x000000FF);

    // CCACommand(m_HIDBuffer, NULL, 0);
    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 7);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;
    
    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);
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

    getPosition(m_nCurPos);
    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CCCAController::isMotorMoving(bool &bMoving)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = DUMMY; // dummy command to get a report to check status

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);

    bMoving = m_bIsMoving;
    return nErr;
}

#pragma mark getters and setters

int CCCAController::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = DUMMY; // dummy command to get a report to get version

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);
    
    strncpy(pszVersion, m_sVersion.c_str(), nStrMaxLen);
    return nErr;
}

int CCCAController::getTemperature(double &dTemperature)
{
    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = DUMMY; // dummy command to get a report to get version

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);
    
    // need to allow user to select the focuser temp source
    switch(m_nTempSource) {
        case AIR:
            dTemperature = m_fAirTemp;
            break;
        case TUBE:
            dTemperature = m_fTubeTemp;
            break;
        case MIRROR:
            dTemperature = m_fMirorTemp;
            break;
    }
    return nErr;
}

int CCCAController::getPosition(int &nPosition)
{

    int nErr = PLUGIN_OK;
    int nByteWriten = 0;
    int nbRead;
    byte szRespBuffer[DATA_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    memset(m_HIDBuffer, 0x00, DATA_BUFFER_SIZE);
    m_HIDBuffer[0] = 0x01; // report ID
    m_HIDBuffer[1] = 0x01; // size is 1 bytes
    m_HIDBuffer[2] = DUMMY; // dummy command to get a report to get version

    nByteWriten = hid_write(m_DevHandle, m_HIDBuffer, 3);
    if(nByteWriten<0)
        nErr = ERR_CMDFAILED;

    nbRead = hid_read(m_DevHandle, szRespBuffer, DATA_BUFFER_SIZE);
    if(nbRead > 0)
      parseGeneralResponse(szRespBuffer);

    nPosition = m_nCurPos;

    return nErr;
}


int CCCAController::getPosLimit()
{
    return m_nMaxPos;
}


#pragma mark command and response functions


void CCCAController::parseGeneralResponse(byte *Buffer)
{
#ifdef PLUGIN_DEBUG
    byte hexBuffer[DATA_BUFFER_SIZE];
    
    hexdump(Buffer, hexBuffer, DATA_BUFFER_SIZE);
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] Buffer :\n %s\n", timestamp, hexBuffer);
    fflush(Logfile);
#endif
    
    if(Buffer[0] == 0x3C) {
        m_nCurPos       = Get32(Buffer, 2);
        m_bIsAtOrigin   = (Buffer[7] & 128) != 0;
        m_bIsMoving     = (Buffer[7] &  64) != 0;
        m_bFanIsOn      = (Buffer[7] &  32) != 0;
        m_bIsHold       = (Buffer[7] &   3) != 0;
        m_sVersion      = std::to_string(Get16(Buffer, 17)>>8) + "." + std::to_string(Get16(Buffer, 17) & 0xFF);
        m_nStepPerMillimeter = Get16(Buffer,23);
        m_nMaxPos       = Get32(Buffer, 25);
        m_fAirTemp      = Get32(Buffer, 45)/ 10.0;
        m_fTubeTemp     = Get32(Buffer, 49) / 10.0;
        m_fMirorTemp    = Get32(Buffer, 43) / 10.0;
#ifdef PLUGIN_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_nCurPos             : %d\n", timestamp, m_nCurPos);
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_bIsAtOrigin         : %s\n", timestamp, m_bIsAtOrigin?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_bIsMoving           : %s\n", timestamp, m_bIsMoving?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_bFanIsOn            : %s\n", timestamp, m_bFanIsOn?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_bIsHold             : %s\n", timestamp, m_bIsHold?"Yes":"No");
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_sVersion            : %s\n", timestamp, m_sVersion.c_str());
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_nStepPerMillimeter  : %d\n", timestamp, m_nStepPerMillimeter);
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_nMaxPos             : %d\n", timestamp, m_nMaxPos);
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_fAirTemp            : %3.2f\n", timestamp, m_fAirTemp);
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_fTubeTemp           : %3.2f\n", timestamp, m_fTubeTemp);
        fprintf(Logfile, "[%s] [CCCAController::parseGeneralResponse] m_fMirorTemp          : %3.2f\n", timestamp, m_fMirorTemp);
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

