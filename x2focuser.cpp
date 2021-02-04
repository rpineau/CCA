
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

    // Read in settings
    if (m_pIniUtil) {
        int nTmp =  m_pIniUtil->readInt(PARENT_KEY, TEMP_SOURCE, AIR);
        m_CCAController.setTemperatureSource(nTmp);
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
    if(nErr)
        nErr = ERR_NOLINK;
    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
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
    double dTemperature;
    char szTmp[255];
    int nTmp;
    bool bTmp;
    
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
        m_CCAController.getTemperatureSource(nTmp);
        dx->setCurrentIndex("comboBox", nTmp);
        m_CCAController.getFanState(bTmp);
        if(bTmp)
            dx->setChecked("radioButton", 1);
        else
            dx->setChecked("radioButton", 1);

        m_CCAController.getTemperature(AIR, dTemperature);
        snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
        dx->setText("airTemp", szTmp);
        
        m_CCAController.getTemperature(TUBE, dTemperature);
        snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
        dx->setText("tubeTemp", szTmp);
        
        m_CCAController.getTemperature(MIRROR, dTemperature);
        snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
        dx->setText("mirrorTemp", szTmp);
    }
    else {
        dx->setEnabled("comboBox",false);
        dx->setEnabled("radioButton",false);
        dx->setText("airTemp", "");
        dx->setText("tubeTemp", "");
        dx->setText("mirrorTemp", "");
    }

    //Display the user interface
    mUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    mUiEnabled = false;

    //Retreive values from the user interface
    if (bPressedOK) {
        nTmp = dx->currentIndex("comboBox");
        m_CCAController.setTemperatureSource(nTmp);
        m_pIniUtil->writeInt(PARENT_KEY, TEMP_SOURCE, nTmp);
        nErr = SB_OK;
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    double dTemperature;
    char szTmp[255];

    if (!strcmp(pszEvent, "on_timer")) {
        if(m_bLinked) {
            m_CCAController.getTemperature(AIR, dTemperature);
            snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
            uiex->setText("airTemp", szTmp);
            
            m_CCAController.getTemperature(TUBE, dTemperature);
            snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
            uiex->setText("tubeTemp", szTmp);
            
            m_CCAController.getTemperature(MIRROR, dTemperature);
            snprintf(szTmp, 255, "%3.2f ºC", dTemperature);
            uiex->setText("mirrorTemp", szTmp);
        }
    }
    
    if (!strcmp(pszEvent, "on_radioButton_clicked")) {
        m_CCAController.setFanOn(true);
    }
    else if (!strcmp(pszEvent, "on_radioButton_2_clicked")) {
        m_CCAController.setFanOn(false);
    }

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
	return 4;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
        case 3: strDisplayName="10000 steps"; nAmount=10000;break;
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

    nErr = m_CCAController.getTemperature(dTemperature);


    return nErr;
}




