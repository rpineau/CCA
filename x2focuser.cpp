
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
    bool bTmp1,bTmp2;
    
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
        bTmp1 = m_pIniUtil->readInt(PARENT_KEY, AUTOFAN_STATE, 0) == 0? false : true;
        bTmp2 = m_pIniUtil->readInt(PARENT_KEY, FAN_STATE, 0) == 0? false : true;
        m_CCAController.setTemperatureSource(m_pIniUtil->readInt(PARENT_KEY, TEMP_SOURCE, AIR));
        if(bTmp1) {
            m_CCAController.setAutoFan(true);
        }
        else {
            m_CCAController.setAutoFan(false);
            if(bTmp2)
                m_CCAController.setFanOn(true);
            else
                m_CCAController.setFanOn(false);
        }
        m_CCAController.setRestorePosition(m_pIniUtil->readInt(PARENT_KEY, LAST_POSITION, 0), m_pIniUtil->readInt(PARENT_KEY, RESTORE_POSITION, 0) == 0? false : true);
    }
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
        str = "Takahashi Active Focuser Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const							
{
	return PLUGIN_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    str="Takahashi Active Focuser";
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const				
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const		
{
	str = "Takahashi Active Focuser";
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2MutexLocker ml(GetMutex());
        // get firmware version
        std::string sFirmware;
        m_CCAController.getFirmwareVersion(sFirmware);
        str = sFirmware.c_str();
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)							
{
    str="Takahashi Active Focuser";
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    int nErr;

    X2MutexLocker ml(GetMutex());
    m_CCAController.setRestorePosition(m_pIniUtil->readInt(PARENT_KEY, LAST_POSITION, 0), m_pIniUtil->readInt(PARENT_KEY, RESTORE_POSITION, 0) == 0? false : true);
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
    m_pIniUtil->writeInt(PARENT_KEY, LAST_POSITION, m_CCAController.getPosition());
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
    std::stringstream sTmpBuf;
    int nTmp;
    bool bTmp1,bTmp2;
    
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
        sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(AIR) << "ºC";
        dx->setText("airTemp", sTmpBuf.str().c_str());
        
        std::stringstream().swap(sTmpBuf);
        sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(TUBE) << "ºC";
        dx->setText("tubeTemp", sTmpBuf.str().c_str());
        
        std::stringstream().swap(sTmpBuf);
        sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(MIRROR) << "ºC";
        dx->setText("mirrorTemp", sTmpBuf.str().c_str());
    }
    else {
        dx->setText("airTemp", "");
        dx->setText("tubeTemp", "");
        dx->setText("mirrorTemp", "");
    }

    // if not connected, it will used the "saved" state, if connected the current state.
    bTmp1 = m_CCAController.getAutoFanState();
    bTmp2 = m_CCAController.getFanState();
    if(bTmp1) {
        dx->setChecked("radioButton_3", 1);
    }
    else {
        if(bTmp2)
            dx->setChecked("radioButton", 1);
        else
            dx->setChecked("radioButton_2", 1);
    }
    
    // This doesn't require to be connected as this is the user selection of what temperature source he wants reported to TSX
    dx->setEnabled("comboBox",true);
    nTmp = m_CCAController.getTemperatureSource();
    dx->setCurrentIndex("comboBox", nTmp);

    dx->setChecked("checkBox", m_CCAController.getRestoreOnConnect()?1:0);
    
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

        if(dx->isChecked("radioButton")) {
            m_pIniUtil->writeInt(PARENT_KEY, AUTOFAN_STATE, 0);
            m_pIniUtil->writeInt(PARENT_KEY, FAN_STATE, 1);
        }
        else if(dx->isChecked("radioButton_2")){
            m_pIniUtil->writeInt(PARENT_KEY, AUTOFAN_STATE, 0);
            m_pIniUtil->writeInt(PARENT_KEY, FAN_STATE, 0);
        }
        else if(dx->isChecked("radioButton_3")) {
            m_pIniUtil->writeInt(PARENT_KEY, AUTOFAN_STATE, 1);
            m_pIniUtil->writeInt(PARENT_KEY, FAN_STATE, 0);
        }

        if(dx->isChecked("checkBox"))
            m_pIniUtil->writeInt(PARENT_KEY, RESTORE_POSITION, 1);
        else
            m_pIniUtil->writeInt(PARENT_KEY, RESTORE_POSITION, 0);

        m_pIniUtil->writeInt(PARENT_KEY, LAST_POSITION, m_CCAController.getPosition());
        nErr = SB_OK;
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    std::stringstream sTmpBuf;
    bool bTmp;

    if (!strcmp(pszEvent, "on_timer")) {
        if(m_bLinked) {
            sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(AIR) << "ºC";
            uiex->setText("airTemp", sTmpBuf.str().c_str());
            
            std::stringstream().swap(sTmpBuf);
            sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(TUBE) << "ºC";
            uiex->setText("tubeTemp", sTmpBuf.str().c_str());
            
            std::stringstream().swap(sTmpBuf);
            sTmpBuf << std::fixed << std::setprecision(2) << m_CCAController.getTemperature(MIRROR) << "ºC";
            uiex->setText("mirrorTemp", sTmpBuf.str().c_str());
            bTmp = m_CCAController.getFanState();
            if(bTmp)
                uiex->setChecked("radioButton", 1);
            else
                uiex->setChecked("radioButton_2", 1);
        }
    }

    
    if (!strcmp(pszEvent, "on_radioButton_clicked")) {
        m_CCAController.log("on_radioButton_clicked, Fan On, Auto Fan Off");
        m_CCAController.setAutoFan(false,true);
        m_CCAController.setFanOn(true);
    }
    else if (!strcmp(pszEvent, "on_radioButton_2_clicked")) {
        m_CCAController.log("on_radioButton_2_clicked, Fan Off, Auto Fan Off");
        m_CCAController.setAutoFan(false,true);
        m_CCAController.setFanOn(false);
    }
    else if (!strcmp(pszEvent, "on_radioButton_3_clicked")) {
        m_CCAController.log("on_radioButton_3_clicked, Fan Off, Auto Fan On");
        m_CCAController.setAutoFan(true,true);
        m_CCAController.setFanOn(false);
    }

}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nPosition = m_CCAController.getPosition();
    m_nPosition = nPosition;
    return SB_OK;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit) 		
{
	nMinLimit = - m_CCAController.getPosLimit(); // when initallizing it seems the focuser can be far bellow 0
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
    if(bComplete)
        m_pIniUtil->writeInt(PARENT_KEY, LAST_POSITION, pMe->m_CCAController.getPosition());

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_nPosition = m_CCAController.getPosition();
    return SB_OK;
}

int X2Focuser::amountCountFocGoto(void) const					
{ 
	return 9;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
        case 1: strDisplayName="50 steps"; nAmount=10;break;
		case 2: strDisplayName="100 steps"; nAmount=100;break;
        case 3: strDisplayName="250 steps"; nAmount=100;break;
        case 4: strDisplayName="500 steps"; nAmount=100;break;
		case 5: strDisplayName="1000 steps"; nAmount=1000;break;
        case 6: strDisplayName="2500 steps"; nAmount=1000;break;
        case 7: strDisplayName="5000 steps"; nAmount=1000;break;
        case 8: strDisplayName="10000 steps"; nAmount=10000;break;
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

    dTemperature = m_CCAController.getTemperature();


    return nErr;
}




