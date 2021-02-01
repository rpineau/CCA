
#include "x2focuser.h"


X2Focuser::X2Focuser(const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;	
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;
	
	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)

    // Read in settings
    if (m_pIniUtil) {
    }
    m_CCAController.setSleeper(m_pSleeper);
}

X2Focuser::~X2Focuser()
{
    //Delete objects used through composition
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();

}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);


    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Takahashi CCA Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return PLUGIN_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    str="Takahashi CCA";
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str = "Takahashi CCA Focuser";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2MutexLocker ml(GetMutex());
        // get firmware version
        char cFirmware[DATA_BUFFER_SIZE];
        m_CCAController.getFirmwareVersion(cFirmware, DATA_BUFFER_SIZE);
        str = cFirmware;
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    str="CCA";
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    nErr = m_CCAController.Connect();
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
    m_CCAController.haltFocuser();
    m_CCAController.Disconnect();
    m_bLinked = false;

	return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    mUiEnabled = false;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("CCA.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());
	// set controls values
    if(m_bLinked) {
        // new position (set to current )
    }
    else {
        // disable all controls
    }

    //Display the user interface
    mUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    mUiEnabled = false;

    //Retreive values from the user interface
    if (bPressedOK) {
        nErr = SB_OK;
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    int nTmpVal;
    char szErrorMessage[LOG_BUFFER_SIZE];

}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nErr = m_CCAController.getPosition(nPosition);
    m_nPosition = nPosition;
    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	nMinLimit = 0;
    return SB_OK;
}

int	X2Focuser::focMaximumLimit(int& nPosLimit)			
{

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nPosLimit = m_CCAController.getPosLimit();
	return SB_OK;
}

int	X2Focuser::focAbort()								
{   int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_CCAController.haltFocuser();
    return nErr;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)	
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_CCAController.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
	nErr = pMe->m_CCAController.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_CCAController.getPosition(m_nPosition);
    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 3;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    int nErr = SB_OK;

    if(!m_bLinked) {
        dTemperature = -100.0;
        return NOT_CONNECTED;
    }
    X2MutexLocker ml(GetMutex());

    // Taken from Richard's Robofocus plugin code.
    // this prevent us from asking the temperature too often
    static CStopWatch timer;

    if(timer.GetElapsedSeconds() > 30.0f || m_fLastTemp < -99.0f) {
        nErr = m_CCAController.getTemperature(m_fLastTemp);
        timer.Reset();
    }

    dTemperature = m_fLastTemp;

    return nErr;
}




