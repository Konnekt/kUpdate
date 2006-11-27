//---------------------------------------------------------------------------

#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <process.h>
#include <Wininet.h>
#include <stdstring.h>
#pragma hdrstop

using namespace std;

#include "plug_shared.h"
#include "ui_shared.h"
#include "plug_export.h"
#include "plug_func.h"
#include "include\time64.h"
#include "include\func.h"
#include "include\simxml.h"
#include "include\win_registry.h"
#include "res\upd\resource.h"
#pragma link "wininet.lib"

#define CFG_UPD_LASTID 4000      // ID ostatniej przesylki
#define CFG_UPD_LASTCHECK 4001 // Kiedy ostatnio cos pobieral

#define IMIG_MAIN_UPDATE 4000
#define IMIA_UPD_CHECK   4001
#define IMIA_UPD_GET_MOTD 4002
#define IMIA_UPD_GET_INFO 4003
#define IMIA_UPD_GET_MSG 4004
#define IMIA_UPD_GET_UPDATE 4005

/*
#define UPD_HOST "jego.stamina.eu.org"
#define UPD_URL "~stamina/konnekt/k_msg.php"
*/

#define UPD_HOST "www.stamina.eu.org"
#define UPD_URL "konnekt/k_msg.php"


CStdString globalID;


int WINAPI DllEntryPoint(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
        return 1;
}
//---------------------------------------------------------------------------
CStdString getCheck() {
    cTime64 tim(true);
    return tim.strftime("%d-%m-%Y") + "|" + string(inttoch(ICMessage(IMC_VERSION),16)) + "|" + string((char*)ICMessage(IMC_GETBETALOGIN));
}

void saveData() {
    SETSTR(CFG_UPD_LASTCHECK , getCheck().c_str());
    ICMessage(IMC_SAVE_CFG);

    HKEY hKey=HKEY_CURRENT_USER;
    string str;
    if (!RegCreateKeyEx(hKey,"Software\\Stamina\\Konnekt\\Update",0,"",0,KEY_ALL_ACCESS,0,&hKey,0))
    {
        RegSetSZ(hKey,"lastGlobalID" , globalID);
    }
}


CStdString getVersions() {
    CStdString ver = "";
    int c = ICMessage(IMC_PLUG_COUNT);
    for (int i=-1; i<c; i++) {
        ver += (i==-1)?"CORE"
            :(char*)IMessageDirect(IM_PLUG_SIG , ICMessage(IMC_PLUG_ID , i));
        ver += "=" + string(inttoch(ICMessage(IMC_PLUG_VERSION , i)));
        ver+="\n";
    }
    return ver;
}

// ----------------------------------------------------------

struct ConnectThreadPack {
    bool force;
    CStdString request;
};

 unsigned int __stdcall ConnectThreadProc(ConnectThreadPack * pack) {

    sDIALOG_long sdl;
    if (pack->force) {
        sdl.flag = DLONG_AINET | DLONG_CANCEL;
        sdl.info = "Pobieram informacje z serwera";
        sdl.title = "Aktualizacje";
        ICMessage(IMI_LONGSTART , (int)&sdl);
    }
    int count = 0;
	IMLOG("* Próbujê po³¹czyæ...");
    HINTERNET hSession=0 , hConnect=0 , hRequest=0;
    try {
    hSession = InternetOpen("Konnekt (update plugin)", INTERNET_OPEN_TYPE_PROXY, 0, 0, 0);
    if (!hSession) throw "InternetOpen failed!";

    hConnect = InternetConnect(hSession, _T(UPD_HOST),
         INTERNET_DEFAULT_HTTP_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (!hConnect) throw "Connect failed!";

    hRequest = HttpOpenRequest(hConnect, "POST",
        _T(UPD_URL), NULL, NULL, 0, 0, 1);
    if (!hRequest) throw "OpenRequest failed!";

    CStdString hdrs = "Content-Type: application/x-www-form-urlencoded";
    CStdString frmData;
    frmData =  "last="+urlEncode(GETSTR(CFG_UPD_LASTID))
            + "&global=" + urlEncode(globalID)
            + "&request=" + urlEncode(pack->request)
            + "&version=" + urlEncode(getVersions())
            + "&betaLogin=" + urlEncode((char*)ICMessage(IMC_GETBETALOGIN))
            + "&betaPass=" + urlEncode((char*)ICMessage(IMC_GETBETAPASSMD5))
        ;
    IMLOG("* Wysy³am... [%s]" , frmData.c_str());
    HttpSendRequest(hRequest, hdrs, hdrs.length(), (void*)frmData.c_str(), frmData.length());
    CStdString response;
    DWORD read;
    if (!InternetReadFile(hRequest , response.GetBuffer(50000) , 50000 , &read)) throw "Read failed!";
    response.ReleaseBuffer();
    IMLOG("%s" , response.c_str());
    cXML XML;
    XML.loadSource(response);
    XML.prepareNode("info/this");
    CStdString lastID = XML.getAttrib("id");
    if (lastID.empty()) throw "Bad Response!";
    SETSTR(CFG_UPD_LASTID , lastID.c_str());
    globalID = XML.getAttrib("global");
    XML.loadSource(XML.getContent("info").c_str());
    // Zbieramy wiadomosci do wyswietlenia
    CStdString content;
    cXML XML2;
    while (!(content = XML.getNode("message")).empty()) {
        XML2.loadSource(content);
        CStdString text = XML2.getText("message");
        XML2.prepareNode("message");
        CStdString type = XML2.getAttrib("type");
        CStdString action = XML2.getAttrib("action");
        if (action == "URL") {
            cMessage m;
            CStdString url = XML2.getAttrib("url");
            if (!text.empty()) text+="\r\n";
            text += url;
			m.body = (char*)text.c_str();
            m.flag = MF_HANDLEDBYUI;
            m.net = NET_UPDATE;
            m.type = MT_URL;
            CStdString ext = SetExtParam("" , "Title" , XML2.getAttrib("title"));
            ext = SetExtParam(ext , "Width" , XML2.getAttrib("Width"));
            ext = SetExtParam(ext , "Height" , XML2.getAttrib("Height"));
            ext = SetExtParam(ext , "UPDType" , type);
            m.ext = (char*)ext.c_str();
            m.time = cTime64(true);
            count ++;
            ICMessage(IMC_NEWMESSAGE , (int)&m);
        } else if (action=="MBOX") { // Message Box
            CStdString URL = XML2.getAttrib("url");
            int flag = 0;
            CStdString style = XML2.getAttrib("style");
            if (strstr(style , "WARN"))
                flag |= MB_ICONEXCLAMATION;
            else if (strstr(style , "ERROR"))
                flag |= MB_ICONERROR;
            else flag |=MB_ICONINFORMATION;
            if (!URL.empty()) {
                text += "\r\n------------------------------\r\nOtworzyæ stronê?";
                flag |= MB_OKCANCEL;
            }
            int r = MessageBox((HWND)ICMessage(IMI_GROUP_GETHANDLE , (int)&sUIAction(0 , IMIG_MAINWND)) 
            , text , ("Update: " + XML2.getAttrib("title")).c_str(), flag);
            if (r==IDOK && !URL.empty()) 
                ShellExecute(0 , "open" , URL , "" , "" , SW_SHOW);
        }
        XML.next();
    }

    sMESSAGESELECT ms(NET_UPDATE);
    ICMessage(IMC_MESSAGEQUEUE , (int)&ms);
    saveData();
  } catch (char * c)
  {
    IMLOG("!Coœ siê nie powiod³o [%s]!" , c);
    if (!pack->force)
        ICMessage(IMC_SETCONNECT , 1);
  } 
  if (hSession) InternetCloseHandle(hSession);
  if (hConnect) InternetCloseHandle(hConnect);
  if (hRequest) InternetCloseHandle(hRequest);

  if (pack->force) {
      ICMessage(IMI_LONGEND , (int)&sdl);
      if (!count) ICMessage(IMI_NOTIFY , (int)"Nie ma dla Ciebie ¿adnych informacji na serwerze!");

  }
  delete pack;
  _endthreadex(0);
  return 0;
}

void checkUpdate(bool force = false , CStdString request = "") {

    if (!force) {
        if (getCheck() == GETSTR(CFG_UPD_LASTCHECK)) {
            IMLOG("No need to check anything...");
            return;
        }
    }
    ConnectThreadPack * ctp = new ConnectThreadPack;
    ctp->force = force;
    ctp->request = request;
	Ctrl->BeginThread(0 , 0 , (unsigned ( __stdcall *)( void * ))ConnectThreadProc , ctp , 0 , 0);

}


// --------------------------------------------------------

int Init() {
  HKEY hKey=HKEY_CURRENT_USER;
  string str;
  if (!RegOpenKeyEx(hKey,"Software\\Stamina\\Konnekt\\Update",0,KEY_READ,&hKey))
  {
        globalID=RegQuerySZ(hKey,"lastGlobalID");
  }
  return 1;
}

int IStart() {

  return 1;
}
int IEnd() {
    return 1;
}
#define CFGSETCOL(i,t,d) {sSETCOL sc;sc.id=(i);sc.type=(t);sc.def=(int)(d);ICMessage(IMC_CFG_SETCOL,(int)&sc);}
int ISetCols() {
    CFGSETCOL(CFG_UPD_LASTID , DT_CT_PCHAR , "");
    CFGSETCOL(CFG_UPD_LASTCHECK , DT_CT_PCHAR , "");
    return 1;
}
int IPrepare() {
    IconRegister(IML_16 , IDI_UPD , Ctrl->hDll() , IDI_UPD);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD , 0) , Ctrl->hDll() , IDI_TYPE_MOTD);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE , 0) , Ctrl->hDll() , IDI_TYPE_UPDATE);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO , 0) , Ctrl->hDll() , IDI_TYPE_INFO);
    IconRegister(IML_16 , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG , 0) , Ctrl->hDll() , IDI_TYPE_MSG);
    ICMessage(IMC_SETCONNECT , 1);
    UIGroupAdd(IMIG_MAIN , IMIG_MAIN_UPDATE , 0 , "Aktualizacje" , IDI_UPD);
    UIActionAdd(IMIG_MAIN_UPDATE , IMIA_UPD_CHECK , 0 , "SprawdŸ");
    UIActionAdd(IMIG_MAIN_UPDATE , IMIA_UPD_GET_MOTD , 0 , "Ostatni MOTD" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD , 0));
    UIActionAdd(IMIG_MAIN_UPDATE , IMIA_UPD_GET_INFO , 0 , "Info o wersji" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO , 0));
    UIActionAdd(IMIG_MAIN_UPDATE , IMIA_UPD_GET_MSG , 0 , "Ostatnia wiadomoœæ" , UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG , 0));
    return 1;
}

ActionProc(sUIActionNotify_base * anBase) {
  sUIActionNotify_2params * an = static_cast<sUIActionNotify_2params*>(anBase);
  switch (an->act.id) {
      case IMIA_UPD_CHECK:
          checkUpdate(true , "CHECK");
          return 0;
      case IMIA_UPD_GET_INFO:
          checkUpdate(true , "VERSIONINFO");
          return 0;
      case IMIA_UPD_GET_MOTD:
          checkUpdate(true , "MOTD");
          return 0;
      case IMIA_UPD_GET_MSG:
          checkUpdate(true , "MSG");
          return 0;
      case IMIA_UPD_GET_UPDATE:
          checkUpdate(true , "UPDATE");
          return 0;
  }
  return 1;
}


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
    case IM_PLUG_DEINIT:     Plug_Deinit(msg->p1,msg->p2);return 1;
    case IM_PLUG_PRIORITY:   return PLUGP_LOWEST;

    case IM_SETCOLS:     return ISetCols();

    case IM_UI_PREPARE:      return IPrepare();
    case IM_START:           return IStart();
    case IM_END:             return IEnd();

    case IM_UIACTION:        return ActionProc((sUIActionNotify_base*)msg->p1);

    case IM_NEEDCONNECTION: ICMessage(IMC_SETCONNECT , 1);break;
    case IM_CONNECT :
      checkUpdate();
    return 1;


    case IM_MSG_RCV:
            { m = (cMessage*)msg->p1;
              switch (m->type) {
                case MT_URL: {
                    if (m->net != NET_UPDATE) return 0;
                    CStdString type = GetExtParam(m->ext , "UPDType");
                    if (type == "MOTD") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MOTD,0);}
                    else if (type == "UPDATE") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE,0);}
                    else if (type == "VERSIONINFO") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_INFO,0);}
                    else if (type == "MSG") {m->notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_MSG,0);}
                    return IM_MSG_ok;}
              }
              return 0;
            }


    default:
        if (Ctrl) Ctrl->setError(IMERROR_NORESULT);
        return 0;

 }
 if (Ctrl) Ctrl->setError(IMERROR_NORESULT);
 return 0;
}
 
