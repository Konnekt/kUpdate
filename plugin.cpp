#include "stdafx.h"


#include "konnekt/kupdate.h"
#include "update.h"
#include "mirror.h"
#include "CentralList.h"

HANDLE timerChecker;

//---------------------------------------------------------------------------

namespace kUpdate {
CStdString globalID;
//const char * host = "test.konnekt.info";
//const char * URL  = "update/central.php"; // centralka

//const char * host = "www.konnekt.info";
//const char * URL  = "update/central.php"; // centralka
const char * defaultCentral = "# Poni¿ej znajduje siê g³ówna centralka projektu.\r\n" 
	"# Powinna byæ zawsze pierwsza!"
	"\r\nhttp://www.konnekt.info/update/central.php"
	"\r\n# ---------------------------------------- \r\n";

Tables::oTable dtDefs;

int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
        return 1;
}
//---------------------------------------------------------------------------
CStdString getCheck() {
    Time64 tim(true);
    return inttostr(ICMessage(IMC_VERSION),16) + "|" + tim.strftime("%d-%m-%y") + "|" + string((char*)ICMessage(IMC_GETBETALOGIN));
}


// Tworzy stringa z wersjami...
CStdString getVersions() {
    string ver = "";
    int c = ICMessage(IMC_PLUG_COUNT);
    for (int i = 0; i < c; i++) {
		oPlugin plugin = Ctrl->getPlugin((tPluginId) i);
		S_ASSERT(plugin.isValid());
        ver += plugin->getSig();
        ver += "=" + inttostr(plugin->getVersion().getInt());
        ver+="\n";
    }
    return ver;
}

// ----------------------------------------------------------

struct ConnectThreadPack {
    bool force;
    CStdString request;
	bool fetchFromSettings;
};



// Procedurka sprawdzaj¹ca update...
unsigned int __stdcall ConnectThreadProc(ConnectThreadPack * pack) {
	Stamina::randomSeed();
	CancelWaitableTimer(timerChecker);
    int count = 0;
	IMLOG("* Parsowanie centralek...");
	map <CStdString , CStdString> centrals;
	map <CStdString , CStdString>::iterator central;
	{
		CStdString setting;

		if (pack->fetchFromSettings) {
			setting = CentralList::serialize();// UIActionCfgGetValue(sUIAction(kUpdate::IMIA::gCfg, kUpdate::CFG::centrals | IMIB_CFG), 0, 0);
			if (ShowBits::checkLevel(ShowBits::levelAdvanced)) {
				setting += UIActionCfgGetValue(sUIAction(kUpdate::IMIA::gCfgCentrals, kUpdate::CFG::userCentrals | IMIB_CFG), 0, 0);
			}
		} else {
			setting = GETSTR(kUpdate::CFG::centrals);
			if (ShowBits::checkLevel(ShowBits::levelAdvanced)) {
				setting += GETSTR(kUpdate::CFG::userCentrals);
			}
		}

		size_t pos = 0;
		size_t found = 0;
		RegEx urlTest;
		urlTest.setPattern("#^!?http://.+?/.+$#i");
		do {
			found = setting.find("\n" , pos);
			CStdString line = setting.substr(pos , found == -1 ? -1 : found - pos);
			line = line.Trim();
			if (line[0] == '#') continue;
			urlTest.setSubject(line);
			if (!urlTest.match()) continue;
			CStdString id = line.substr((line[0] == '!' ? 1 : 0) , (line.find('?') == line.npos ? line.npos : line.find('?') - (line[0] == '!' ? 1 : 0)));
			id = urlEncode(id , '%' , "/:?&.-_=");
			if (line[0] == '!') {
				if (centrals.find(id) != centrals.end())
					centrals.erase(id);
			} else {
				centrals[id] = line;
			}
		} while ((pos = found+1));

		if (centrals.size() == 0) {
			static bool oneAtATime = true;
			if (oneAtATime) {
				oneAtATime = false;
				SETSTR(CFG::centrals , CentralList::serialize());
				ConnectThreadProc(pack);
				oneAtATime = true;
			}
			return 0;
		}
	}

	IMLOG("* Próbujê po³¹czyæ...");
	/* Tworzymy wspóln¹ sesjê dla wszystkich */
	CStdString version = "kUpdate v";
	ICMessage(IMC_PLUG_VERSION , ICMessage(IMC_PLUGID_POS , Ctrl->ID()), (int) version.GetBuffer(16));
	version.ReleaseBuffer();
	version = "kUpdate v" + version;
	Stamina::oInternet internet = new Stamina::Internet((HINTERNET) ICMessage(IMC_HINTERNET_OPEN , (int)version.c_str()));

	cUpdate::tUrlList updates;
	int failed = 0;
	int netFailed = 0;

	sDIALOG_long sdl;
	if (pack->force) {
		sdl.flag = DLONG_AINET | DLONG_CANCEL;
		sdl.progress = centrals.size() * 3;
		sdl.info = "Pobieram informacje z serwera";
		sdl.title = "Aktualizacje";
		ICMessage(IMI_LONGSTART , (int)&sdl);
		sdl.progress = 0;
	}

	for (central = centrals.begin(); central != centrals.end(); central ++) {	
		if (pack->force && sdl.cancel) break;
		IMLOG("- Fetching [%s]='%s'" , central->first.c_str() , central->second.c_str());
		CStdString title = central->first;
		if (pack->force) {
			CStdString info = "Pobieram informacje o aktualizacji z \r\n\r\n" + central->second;
			sdl.info = info;
			ICMessage(IMI_LONGSET , (int) &sdl , DSET_INFO);
		}
		try {
			CStdString centralUrl = central->second;

			if (centralUrl.find('?') != -1) {
				//centralUrl += "";
			} else {
				centralUrl += "?";
			}

			if (pack->force) {
				sdl.progress++;
				ICMessage(IMI_LONGSET , (int) &sdl , DSET_PROGRESS);
			} else {
				centralUrl += "&auto=1";
			}
			centralUrl += "&request=" + pack->request;

			HKEY hKey=HKEY_CURRENT_USER;
			CStdString globalID;
			if (!RegOpenKeyEx(hKey,"Software\\Stamina\\Konnekt\\UpdateGlobals",0,KEY_READ,&hKey))
			{
				globalID=regQueryString(hKey,central->first);
				RegCloseKey(hKey);
			}
			CStdString hdrs = "Content-Type: application/x-www-form-urlencoded";
			CStdString serverId = centralUrl.substr(7 , centralUrl.find('/' , 8)-7);
			CStdString lastID = GetExtParam(GETSTR(CFG::lastID) , central->first);
			if (globalID.length() > maxIdSize) {
				globalID = "";
			}
			if (lastID.length() > maxIdSize) {
				lastID = "";
			}


			Stamina::oRequest request;
			
			if (centralUrl.find("nopost=1") == -1) {
				CStdString frmData = "last="+urlEncode( lastID )
						+ "&global=" + urlEncode(globalID)
						+ "&request=" + urlEncode(pack->request)
						+ "&version=" + urlEncode(getVersions())
						+ "&serverId=" + urlEncode(serverId)
						+ "&betaLogin=" + urlEncode((char*)ICMessage(IMC_GETBETALOGIN))
						+ "&betaPass=" + urlEncode(MD5Hex(serverId + (char*)ICMessage(IMC_GETBETAPASSMD5)))
					;
				request = internet->handlePostWithRedirects(centralUrl, hdrs, frmData);
			}

			if (!request || request->getStatusCode() == 405 /*Method not allowed*/) {
				Stamina::oConnection connection;
				CStdString uri;
				if (request) {
					connection = request->getConnection();
					uri = request->getUri().a_string();
				} else {
					uri = centralUrl;
					connection = new Stamina::Connection(internet, uri);
				}
				// budujemy nowy request ¿eby wykonaæ GET...
				request = new Stamina::Request( connection, uri, Stamina::Request::typeGet );
				request->sendThrowable("", "");
			} 

			if (pack->force) {
				sdl.progress++;
				ICMessage(IMI_LONGSET , (int) &sdl , DSET_PROGRESS);
			}

			if (request->getStatusCode() != 200 && request->getStatusCode() != 304 /*Not Modified*/) {
				throw eUpdate("Serwer zwróci³ nieprawid³ow¹ odpowiedŸ!", Stamina::inttostr(request->getStatusCode()) + " " + request->getStatusText());
			}

			String response = request->getResponse();

			IMLOG("Otrzyma³em odpowiedŸ [%.200s]" , response.c_str());
			Stamina::SXML XML;
			XML.loadSource(response.a_str());
			XML.prepareNode("info/this");
			lastID = XML.getAttrib("id");
			globalID = XML.getAttrib("global");

			if (globalID.length() > maxIdSize) {
				globalID = "";
			}
			if (lastID.length() > maxIdSize) {
				lastID = "";
			}

			if (XML.getAttrib("title").empty()) throw eUpdate("Nieprawid³owa odpowiedŸ serwera!" , "Brakuje tytu³u centralki.");

			title = XML.getAttrib("title");

			XML.loadSource(XML.getContent("info").c_str());
			// Zbieramy wiadomosci do wyswietlenia
			CStdString content;
			Stamina::SXML XML2;
		    while (!(content = XML.getNode("message")).empty()) {
				XML2.loadSource(content);
				CStdString text = XML2.getText("message");
				XML2.prepareNode("message");
				CStdString type = XML2.getAttrib("type");
				CStdString action = XML2.getAttrib("action");
				/*TODO: onAccepted i onCanceled */
				//CStdString onAccepted = XML2.getAttrib("onAccepted");
				//CStdString onCanceled = XML2.getAttrib("onCanceled");
				IMLOG("- Got message type=%s action=%s" , type.c_str() , action.c_str());
				count ++;
				if (action == "URL") {
					cMessage m;
					CStdString msgUrl = kUpdate::GetFullUrl(XML2.getAttrib("url"), centralUrl);
					if (!text.empty()) text+="\r\n";
					text += msgUrl;
					m.body = (char*)text.c_str();
					m.flag = MF_HANDLEDBYUI;
					m.net = NET_UPDATE;
					m.type = MT_URL;
					CStdString ext = SetExtParam("" , "Title" , string(title + " - ") + XML2.getAttrib("title"));
					ext = SetExtParam(ext , "Width" , XML2.getAttrib("Width"));
					ext = SetExtParam(ext , "Height" , XML2.getAttrib("Height"));
					ext = SetExtParam(ext , "UPDType" , type);
/*					if (!onAccepted.empty())
						ext = SetExtParam(ext , "UPDonAccepted" , onAccepted);
					if (!onCanceled.empty())
						ext = SetExtParam(ext , "UPDonCanceled" , onCanceled);
						*/
					m.ext = (char*)ext.c_str();
					m.time = Time64(true);
					ICMessage(IMC_NEWMESSAGE , (int)&m);
				} else if (action=="MBOX") { // Message Box
					CStdString msgUrl = kUpdate::GetFullUrl(XML2.getAttrib("url"), centralUrl);
					int flag = 0;
					CStdString style = XML2.getAttrib("style");
					if (strstr(style , "WARN"))
						flag |= MB_ICONEXCLAMATION;
					else if (strstr(style , "ERROR"))
						flag |= MB_ICONERROR;
					else flag |=MB_ICONINFORMATION;
					if (!msgUrl.empty()) {
						text += "\r\n------------------------------\r\nOtworzyæ stronê?";
						flag |= MB_OKCANCEL;
					}
					int r = MessageBox((HWND)ICMessage(IMI_GROUP_GETHANDLE , (int)&sUIAction(0 , IMIG_MAINWND)) 
					, text , (string(title + " - ") + XML2.getAttrib("title")).c_str(), flag);
					if (r==IDOK && !msgUrl.empty()) 
						ShellExecute(0 , "open" , msgUrl , "" , "" , SW_SHOW);
				}
				XML.next();
			} // message
			MirrorList mirrorList;
			while (!(content = XML.getNode("mirror")).empty()) {
				XML2.loadSource(content);
				XML2.prepareNode("mirror");
				Mirror m(XML2.getAttrib("url"), atoi(XML2.getAttrib("priority").c_str()));
				CStdString id = XML2.getAttrib("id");
				mirrorList.addMirror(id, m);
				IMLOG("- Got mirror \"%s\" => \"%s\"  [%d]", id.c_str(), m.url.c_str(), m.priority);
				XML.next();
			}
			while (!(content = XML.getNode("update")).empty()) {
				XML2.loadSource(content);
				XML2.prepareNode("update");
				cUpdate::cUrlItem ui;
				ui.central = title;
	
				Stamina::tStringVector onoption;
				Stamina::split(XML2.getAttrib("onoption"), ",", onoption);
				bool skip = false;
				for (Stamina::tStringVector::iterator it = onoption.begin(); it != onoption.end(); ++it) {
                    if (it->empty()) continue;
					bool negate = ((*it)[0] == '!');
					if (negate) {
						it->erase(0, 1);
					}
					bool found = (RegEx::doMatch(("/[?&]"+*it+"=[^0]/").c_str(), centralUrl) > 0);
					if (!found || (negate && found)) {
                        skip = true;
						break;
					}
				}
				if (skip) {
					XML.next();
					continue;
				}

				ui.title = XML2.getAttrib("title");
				ui.url = XML2.getAttrib("url");
				ui.external = XML2.getAttrib("extern") == "1";
				ui.checkTime = XML2.getAttrib("checktime") == "1";

				// sprawdzamy czy ju¿ takiego cuda nie mamy...
				if (!ui.external || std::find(updates.begin(), updates.end(), ui) == updates.end()) {
					mirrorList.getMirrors(ui.url, ui.mirrors, XML2.getAttrib("mirror"), centralUrl);
					updates.push_back(ui);
					IMLOG("- Got update \"%s\" => \"%s\" %d mirrors", ui.title.c_str(), ui.url.c_str(), ui.mirrors.size());
				} else {
					IMLOG("- Got duplicate update (%s)", ui.url.c_str());
				}
		//        CStdString  = XML2.getText("update");
				count ++;
				XML.next();
			}

			{
				HKEY hKey=HKEY_CURRENT_USER;
				if (!RegCreateKeyEx(hKey,"Software\\Stamina\\Konnekt\\UpdateGlobals",0,"",0,KEY_ALL_ACCESS,0,&hKey,0))
				{
					regSetString(hKey, central->first , globalID);
					RegCloseKey(hKey);
				}
			}

			SETSTR(CFG::lastID , SetExtParam(GETSTR(CFG::lastID) , central->first , lastID).c_str());
		} catch (ExceptionInternet& e) {
			IMLOG("!Wyst¹pi³ b³¹d sieci (%s) - %s - %s!" , title.c_str() , e.getReason().c_str());
			netFailed++;
			if (pack->force)
				if (Ctrl->IMessage(&sIMessage_msgBox(IMI_ERROR , ("Wyst¹pi³ b³¹d sieci!\r\n" + e.getReason() + "\r\n\r\nJe¿eli b³¹d bêdzie siê powtarza³, wy³¹cz t¹ centralkê w ustawieniach! ").c_str(), "KUpdate - centralka - " + title , MB_OKCANCEL , sdl.handle)) == IDCANCEL)
					break; // przerywamy przegl¹danie...

		} catch (eUpdate& e)
		{
			IMLOG("!Wyst¹pi³ b³¹d (%s) - %s - %s!" , title.c_str() , e.getReason().a_str() , e.getExtended().a_str());
			failed++;
			if (pack->force)
				if (Ctrl->IMessage(&sIMessage_msgBox(IMI_ERROR , (e.toString() + "\r\n\r\nJe¿eli b³¹d bêdzie siê powtarza³, wy³¹cz t¹ centralkê w ustawieniach! ").a_str() , "KUpdate - centralka - " + title , MB_OKCANCEL , sdl.handle)) == IDCANCEL)
					break; // przerywamy przegl¹danie...
		} 
		if (pack->force) {
			sdl.progress++;
			ICMessage(IMI_LONGSET , (int) &sdl , DSET_PROGRESS);
		}

	} // przegl¹danie wszystkich paczek 

	if (pack->force) {
		ICMessage(IMI_LONGEND , (int)&sdl);
	}
	if (netFailed == centrals.size()) { /* Nic siê nie uda³o! (_tylko_ problemy z ³¹czeniem! */
		if (!pack->force)
			ICMessage(IMC_SETCONNECT , kUpdate::retryTime);
	} else {
		if (pack->force) {
			if (!count) ICMessage(IMI_INFORM , (int)"Nie ma dla Ciebie ¿adnych informacji!");
		}
		SETSTR(CFG::lastCheck , getCheck().c_str());
		if (updates.size())
			new cUpdate(pack->force, updates, (pack->request.empty() || pack->request == "CHECK"));
		sMESSAGESELECT ms(NET_UPDATE);
		ms.wflag = MF_HANDLEDBYUI;
		ICMessage(IMC_MESSAGEQUEUE , (int)&ms);
		Ctrl->DTsetInt64(DTCFG , 0 , kUpdate::CFG::lastCheckTime , _time64(0));
		ICMessage(IMC_SAVE_CFG);

		/* Od razu ustawiamy w kolejce kolejn¹ aktualizacjê... */
		IMessageDirect(IM_CONNECT, Ctrl->ID());
	}
	delete pack;
	_endthreadex(0);
	return 0;
}

void CALLBACK TimerCheckerAPC(ConnectThreadPack * ctp , DWORD l , DWORD h) {
	CloseHandle(Ctrl->BeginThread("Update::ConnectThreadProc", 0 , 0 , (unsigned ( __stdcall *)( void * ))ConnectThreadProc , ctp , 0 , 0));
}

void checkUpdate(bool force = false , CStdString request = "", bool fetchFromSettings = false) {
	__int64 diff = 0; // ró¿nica w SEKUNDACH
    if (!force) {
		/* Sprawdzamy, czy automat powinien siê odpaliæ w ogóle... */
		if (!GETINT(kUpdate::CFG::checkInterval)/* || (GETINT(kUpdate::CFG::checkAtStart) && getCheck() == GETSTR(CFG::lastCheck))*/)
			return;
		/* Ile czasu up³ynê³o? */
		diff = _time64(0) - Ctrl->DTgetInt64(DTCFG , 0 , kUpdate::CFG::lastCheckTime);
		/* Ile czasu zosta³o? (sprawdzamy nie rzadziej ni¿ 6 godzin */
		int checkInterval = max(6*60, GETINT(kUpdate::CFG::checkInterval) );
		diff = checkInterval * 60 - diff;
    }
    ConnectThreadPack * ctp = new ConnectThreadPack;
    ctp->force = force;
    ctp->request = request;
	ctp->fetchFromSettings = fetchFromSettings;
	if (diff <=0) /* Ju¿ dawno powinno wypaliæ! */
		CloseHandle(Ctrl->BeginThread("Update::ConnectThreadProc", 0 , 0 , (unsigned ( __stdcall *)( void * ))ConnectThreadProc , ctp , 0 , 0));
	else {
		if (GETINT(kUpdate::CFG::checkAtStart)) {
			IMLOG("- Not this time (checkAtStart==true)");
			return;
		}
		IMLOG("- Next check in %d s" , diff);
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = - diff * 1000 * 10000;
		SetWaitableTimer(timerChecker , &dueTime , 0 , (PTIMERAPCROUTINE)TimerCheckerAPC , ctp , false);
	}

}


// --------------------------------------------------------

int Init() {
	kUpdate::runUnfinishedUpdates();
  timerChecker = CreateWaitableTimer(0 , true , 0);
  return 1;
}
void DeInit() {
	CloseHandle(timerChecker);
}

int IStart() {

  return 1;
}
int IEnd() {
    return 1;
}
int ISetCols() {

	SetColumn(DTCFG  , CFG::lastID , DT_CT_PCHAR , "" , "kUpdate/lastID");
	SetColumn(DTCFG  , CFG::lastCheck , DT_CT_PCHAR , "" , "kUpdate/lastCheck");
	SetColumn(DTCFG  , CFG::centrals , DT_CT_PCHAR , "" , "kUpdate/centrals");
	SetColumn(DTCFG  , CFG::userCentrals , DT_CT_PCHAR , "" , "kUpdate/userCentrals");
	SetColumn(DTCFG  , CFG::lastCheckTime , DT_CT_64 , 0 , "kUpdate/lastCheckTime");
	SetColumn(DTCFG  , CFG::checkInterval , DT_CT_INT , 60 * 24 * 2 , "kUpdate/checkInterval");
	SetColumn(DTCFG  , CFG::checkAtStart , DT_CT_INT , 1 , "kUpdate/checkAtStart");

	dtDefs = Tables::registerTable(Ctrl, "KUpdate/definitions");	
	dtDefs->setOpt(optBroadcastEvents, false);
	dtDefs->setOpt(optUseCurrentPassword, false);
	dtDefs->setOpt(optUsePassword, false);
	dtDefs->setOpt(optAutoLoad, true);
	dtDefs->setOpt(optAutoSave, true);
	dtDefs->setFilename("update_defs.dtb");
	dtDefs->setDirectory();
	dtDefs->setColumn(DTDefs::file, ctypeString, "file");
	dtDefs->setColumn(DTDefs::lastModTime, ctypeInt64, "modTime");
	dtDefs->setColumn(DTDefs::lastRevision, ctypeInt64, "revision");
	dtDefs->setColumn(DTDefs::lastOldestFile, ctypeInt64, "oldestFile");

	return 1;
}
int IPrepare() {
    IconRegister(IML_16 , IDI_UPD , Ctrl->hDll() , IDI_UPD);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD , 0) , Ctrl->hDll() , IDI_TYPE_MOTD);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0) , Ctrl->hDll() , IDI_TYPE_UPDATE);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO , 0) , Ctrl->hDll() , IDI_TYPE_INFO);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG , 0) , Ctrl->hDll() , IDI_TYPE_MSG);
	UIActionAdd(IMIG_MAIN , 0 , ACTT_SEPARATOR , "" );
	UIGroupAdd(IMIG_MAIN , IMIA::gUpdate , 0 , "Aktualizacje" , IDI_UPD);
	UIActionAdd(IMIA::gUpdate , IMIA::check , 0 , "SprawdŸ wszystkie nowoœci");
	UIActionAdd(IMIA::gUpdate , IMIA::runUnfinished , ACTR_INIT , "Dokoñcz/usuñ zaleg³¹ aktualizacjê" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0));
	UIActionAdd(IMIA::gUpdate , 0 , ACTT_SEPARATOR , "SprawdŸ wszystkie nowoœci");
	UIActionAdd(IMIA::gUpdate , IMIA::getUpdate , 0 , "Aktualizacje" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0));

/*
	UIActionAdd(IMIA::gUpdate , IMIA::getMOTD , 0 , "Ostatni MOTD" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD , 0));
	UIActionAdd(IMIA::gUpdate , IMIA::getInfo , 0 , "Info o wersji" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO , 0));
	UIActionAdd(IMIA::gUpdate , IMIA::getMsg , 0 , "Ostatnia wiadomoœæ" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG , 0));
*/
	UIGroupAdd(IMIG_CFG_SETTINGS , IMIA::gCfg , 0 , "Aktualizacje" , IDI_UPD); {
		UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_GROUP , ""); {
			if (ShowBits::checkBits(ShowBits::showInfoNormal)) {
				UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_TIPBUTTON | ACTSC_INLINE , AP_TIP "" AP_TIPIMAGEURL "file://%KonnektData%\\img\\kupdate_help.gif" AP_ICONSIZE "32"
					, 0, 50, 50);
				UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_HTMLINFO | ACTSC_FULLWIDTH , 
					"Konnekt co jakiœ czas sprawdza, czy s¹ dostêpne dodatki lub uaktualnienia. "
					"Gdy uka¿e siê coœ nowego, w <i>zasobniku systemowym</i> pojawi siê migaj¹ca zielona strza³ka, któr¹ nale¿y klikn¹æ, aby otworzyæ okno aktualizacji [<b>?</b>]."
					, 0, 300, -4);
				//UIActionAdd(kUpdate::IMIA::gCfg , kUpdate::IMIA::showHelp , ACTT_BUTTON | ACTSC_FULLWIDTH , "Krótka instrukcja obs³ugi" , 0 , 0 , 25);
				UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_SEPARATOR);
			}
			CStdString txt;
			txt = "SprawdŸ teraz" AP_ICONSIZE "32" AP_TIP "Sprawdza, czy jest coœ nowego";
			txt = SetActParam(txt, AP_IMGURL, "res://dll/4000.ico");
			UIActionAdd(kUpdate::IMIA::gCfg , kUpdate::IMIA::check , ACTT_BUTTON | ACTSC_INLINE , txt
				, 0, 145, 42);
			txt = "Przejrzyj aktualizacje" AP_ICONSIZE "16" AP_TIP "Pozwala przejrzeæ wszystkie aktualnie dostêpne uaktualnienia.";
			txt = SetActParam(txt, AP_ICO, inttostr(UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0)).c_str());
			UIActionAdd(kUpdate::IMIA::gCfg , kUpdate::IMIA::getUpdate , ACTT_BUTTON/* | ACTSC_INLINE*/ , txt
				, 0, 150, 42);
			txt = "Dokoñcz poprzedni¹ aktualizacjê" AP_ICONSIZE "16" AP_TIP "Na dysku znajduje siê pobrana aktualizacja. Mo¿esz j¹ zainstalowaæ, lub usun¹æ.";
			txt = SetActParam(txt, AP_ICO, inttostr(UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0)).c_str());
			UIActionAdd(kUpdate::IMIA::gCfg , kUpdate::IMIA::runUnfinished , ACTT_BUTTON | ACTR_INIT , txt
				, 0, 300, 35);

		} UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_GROUPEND , "");

		CentralList::loadList();
//		CentralList::unserialize(Ctrl->DTgetStr(DTCFG, 0 , CFG::centrals));

		if (ShowBits::checkLevel(ShowBits::levelNormal)) {
			UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_GROUP , "Czêstotliwoœæ sprawdzania"); {
				if (ShowBits::checkLevel(ShowBits::levelAdvanced))
					UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_CHECK , "Sprawdzaj tylko po uruchomieniu" , kUpdate::CFG::checkAtStart);
				UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_COMBO | ACTSCOMBO_NOICON | ACTSCOMBO_LIST /*|  ACTSC_INLINE*/ , 
					"Nigdy" CFGVALUE  "0\n"  
					"Co 12 godzin"  CFGVALUE "720\n"
					"Raz dziennie" CFGVALUE "1440\n"
					"Raz na dwa dni" CFGVALUE "2880\n"
					"Raz na tydzieñ" CFGVALUE "10080" 
					AP_PARAMS AP_TIP "Czêstotliwoœæ sprawdzania nowych aktualizacji na serwerze." , kUpdate::CFG::checkInterval);
				//UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_COMMENT , "Co ile sprawdzaæ nowoœci?");
			} UIActionAdd(kUpdate::IMIA::gCfg , 0 , ACTT_GROUPEND , "");
		}

		if (ShowBits::checkLevel(ShowBits::levelAdvanced)) {
			UIGroupAdd(IMIA::gCfg, IMIA::gCfgCentrals , 0 , "Dodatkowe centralki" , IDI_UPD); {

				UIActionAdd(kUpdate::IMIA::gCfgCentrals , 0 , ACTT_GROUP , "Dodatkowe adresy centralek"); {
					UIActionAdd(kUpdate::IMIA::gCfgCentrals , 0 , ACTT_HTMLINFO , "Poni¿ej mo¿esz podaæ dodatkowe adresy <i>centralek</i>, które bêd¹ ka¿dorazowo sprawdzane.<br/>"
						"<div class='warn' align='center'><b>Uwaga!</b> Niew³aœciwie edytuj¹c to pole mo¿esz pozbawiæ siê aktualizacji programu! </div>"
						"<div class='note'>Ka¿dy adres w osobnej linijce, adresy mo¿na tymczasowo wy³¹czaæ umieszczaj¹c na pocz¹tku znaki '#' lub '!'.</div>"
						, 0 , 0 , 75);
					UIActionAdd(kUpdate::IMIA::gCfgCentrals , IMIB_CFG , ACTT_TEXT , "" , kUpdate::CFG::userCentrals , 0 , 100);
					//UIActionAdd(kUpdate::IMIA::gCfg , kUpdate::IMIA::defCentral , ACTT_BUTTON | ACTSC_FULLWIDTH , "Przywróæ domyœln¹ listê" , 0 , 0 , 25);
				} UIActionAdd(kUpdate::IMIA::gCfgCentrals , 0 , ACTT_GROUPEND , "");
			}
		}
	}


    return 1;
}

ActionProc(sUIActionNotify_base * anBase) {
	if ((anBase->act.id & IMIB_) == CentralList::actionBranch) {
		return CentralList::actionProc(anBase);
	}
  sUIActionNotify_2params * an = static_cast<sUIActionNotify_2params*>(anBase);
  switch (an->act.id) {
	  case IMIA::check:
		  ACTIONONLY(anBase);
		  checkUpdate(true , "CHECK", anBase->act.parent == kUpdate::IMIA::gCfg);
          return 0;
	  case IMIA::getInfo:
		  ACTIONONLY(anBase);
          checkUpdate(true , "VERSIONINFO");
          return 0;
	  case IMIA::getMOTD:
		  ACTIONONLY(anBase);
          checkUpdate(true , "MOTD");
          return 0;
	  case IMIA::getMsg:
		  ACTIONONLY(anBase);
          checkUpdate(true , "MSG");
          return 0;
	  case IMIA::getUpdate:
//		  new cUpdate(true , "http://test.konnekt.info/update/test.xml");
		  ACTIONONLY(anBase);
          checkUpdate(true , "UPDATE", anBase->act.parent == kUpdate::IMIA::gCfg);
          return 0;
	  case IMIA::showHelp:
		  ACTIONONLY(anBase);
		  ShellExecute(0 , "open" , "doc\\kupdate.html" , "" , "" , SW_SHOW);
		  return 0;
/*	  case IMIA::defCentral:
		  ACTIONONLY(anBase);
		  UIActionCfgSetValue(sUIAction(IMIA::gCfg , IMIB_CFG | CFG::centrals) , defaultCentral);
		  return 0;
		  */
	  case IMIA::runUnfinished:
		  if (anBase->code == ACTN_CREATE) {
			  UIActionSetStatus(anBase->act, (kUpdate::checkUnfinishedUpdates()=="" ? -1 : 0) , ACTS_HIDDEN);
		  } else if (anBase->code == ACTN_ACTION) {
			  kUpdate::runUnfinishedUpdates(true);
		  }
		  return 0;

  }
  return 1;
}

};

using namespace kUpdate;

int __stdcall IMessageProc(sIMessage_base * msgBase) {
 sIMessage_2params * msg = static_cast<sIMessage_2params*>(msgBase); 
 sDIALOG sd;
 cMessage * m;
 switch (msg->id) {
    // INITIALIZATION
    case IM_PLUG_NET:        return NET_UPDATE;
    case IM_PLUG_TYPE:       return IMT_MESSAGE | IMT_CONFIG | IMT_UI | IMT_MSGUI | IMT_PROTOCOL;
    case IM_PLUG_VERSION:    return (int)"";
    case IM_PLUG_SDKVERSION: return KONNEKT_SDK_V;  // Ta linijka jest wymagana!
    case IM_PLUG_SIG:        return (int)"UPDATE";
    case IM_PLUG_CORE_V:     return (int)"W98";
    case IM_PLUG_UI_V:       return 0;
    case IM_PLUG_NAME:       return (int)"Auto Update";
    case IM_PLUG_NETNAME:    return (int)"";
    case IM_PLUG_INIT:       Ctrl=(cCtrl*)msg->p1;Plug_Init(msg->p1,msg->p2);return Init();
    case IM_PLUG_DEINIT:     Plug_Deinit(msg->p1,msg->p2);DeInit();return 1;
    case IM_PLUG_PRIORITY:   return PLUGP_LOWEST;

    case IM_SETCOLS:     return ISetCols();

    case IM_UI_PREPARE:      return IPrepare();
    case IM_START:           return IStart();
    case IM_END:             return IEnd();

    case IM_UIACTION:        return ActionProc((sUIActionNotify_base*)msg->p1);

    case IM_NEEDCONNECTION: ICMessage(IMC_SETCONNECT , 1);break;
	case IM_CONNECT :
		IMESSAGE_TS();
        checkUpdate();
	    return 1;


    case IM_MSG_RCV:
            { m = (cMessage*)msg->p1;
			  if (m->net != NET_UPDATE) return 0;
              switch (m->type) {
                case MT_URL: {
                    if (m->net != NET_UPDATE) return 0;
                    CStdString type = GetExtParam(m->ext , "UPDType");
                    if (type == "MOTD") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD,0);}
                    else if (type == "UPDATE") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE,0);}
                    else if (type == "VERSIONINFO") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO,0);}
                    else if (type == "MSG") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG,0);}
                    return IM_MSG_ok;}
				case MT_EVENT: return IM_MSG_ok;
              }
              return 0;
            }
	case IM_MSG_OPEN: {
		cMessage * m = (cMessage *)msg->p1;
		if (m->net != NET_UPDATE || m->type != MT_EVENT) return 0;
		HANDLE event = (HANDLE) Stamina::chtoint(GetExtParam(m->ext , "WaitObject").c_str() , 16);
		IMLOG("Opening waiting object %x" , event);
		if (event)
			SetEvent(event);
		return IM_MSG_delete;}
	case kUpdate::IM::startInterface:case kUpdate::IM::destroyInterface: {
		if (!msg->p1 || msg->sender != Ctrl->ID()) break;
		IMESSAGE_TS();
		return ((cUpdate*)msg->p1)->MessageProc(msg);
		}

	case IM_CFG_CHANGED:
//		if (UIActionHandle(sUIAction(CentralList::actionGroup, CentralList::actionHolder))) {
		if (CentralList::actionsInstantiated) {
			Ctrl->DTsetStr(DTCFG, 0, kUpdate::CFG::centrals, CentralList::serialize());
		}
		return 0;


    default:
        if (Ctrl) Ctrl->setError(IMERROR_NORESULT);
        return 0;

 }
 if (Ctrl) Ctrl->setError(IMERROR_NORESULT);
 return 0;
}
 
