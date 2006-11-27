#include "stdafx.h"

#include <list>

#include <konnekt/lib.h>
#include "konnekt/kupdate.h"
#include "update.h"

namespace kUpdate {

	std::list<DWORD> updatesInProgress;
	CriticalSection updatesInProgressCS;

void kUpdate::runUpdateScript(CStdString script) {
	if (script.empty()) return;
	const char *args[3];

	// nie ma upd.exe, spróbujemy odzyskaæ go z w³asnych zasobów...
	CStdString backupUpd = expandEnvironmentStrings("%KonnektData%\\kupdate\\upd.exe");
	if (_access(kUpdate::getTempPath() + "upd.exe", 0)) {
		if (_access(backupUpd, 0)) {
			// Nie ma nawet backupa! przerywamy...
			Ctrl->IMessage(&sIMessage_msgBox(IMI_ERROR, "Niestety nie mog³em znaleŸæ programu instalacyjnego!\r\n\r\n"
				"Upewnij siê w ustawieniach czy masz zaznaczon¹ g³ówn¹ centralkê\r\ni wybierz pe³n¹ aktualizacjê (menu Konnekt/Aktualizacje/Aktualizacje)", "kUpdate"));
			ICMessage(IMI_CONFIG, kUpdate::IMIA::gCfg); 
			return;
		} else {
		CopyFile(backupUpd, kUpdate::getTempPath() + "upd.exe", false);
		}
	} else {
	// jest upd.exe, skopiujmy go na przysz³oœæ...
		CopyFile(kUpdate::getTempPath() + "upd.exe", backupUpd, false);
	}

	_chdir(kUpdate::getTempPath());
	args[0] = "upd.exe";
	args[1] = script.c_str();
	args[2] = 0;
	_spawnv(_P_NOWAIT , args[0] , args);
	ICMessage(IMC_RESTORECURDIR);
	if (scriptNeedsRestart(script))
		ICMessage(IMC_SHUTDOWN, 1);

}

CStdString kUpdate::checkUnfinishedUpdates() {
	WIN32_FIND_DATA fd;
	HANDLE hFile;
	CStdString script = "";
	if ((hFile = FindFirstFile(kUpdate::getTempPath() + "*.script",&fd))!=INVALID_HANDLE_VALUE) {
/*		do {
			if (*fd.cFileName != '.' && fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				profile = "?";
				break;
			}
		} while (FindNextFile(hFile , &fd));*/
		script = kUpdate::getTempPath() + fd.cFileName;
		FindClose(hFile);
	}
	return script;
}
void kUpdate::runUnfinishedUpdates(bool giveMessage) {
	CStdString script = checkUnfinishedUpdates();
	if (script.empty()) {
		if (giveMessage)
			IMessage(&sIMessage_msgBox(IMI_INFORM, "Nie ma wiêcej aktualizacji do dokoñczenia.", "kUpdate"));
		return;
	}
	int r = DialogBoxParam(Ctrl->hDll(), MAKEINTRESOURCE(IDD_FOUNDEXISTING), 0, kUpdate::chooseOptionDialogProc, (LPARAM)script.c_str());
	if (r == IDABORT) {
		kUpdate::removeUnfinishedUpdate(script);
		return;
	} else if (r == IDNO || r == IDCANCEL) {
		return;
	}
	kUpdate::runUpdateScript(script);
}


CStdString kUpdate::getTempPath() {
	return kUpdate::GetFullPath("%konnektTemp%\\kUpdate\\");
}
bool kUpdate::scriptNeedsRestart(CStdString script) {
	return script.find("_restart.script") != CStdString::npos;
}
CStdString kUpdate::getUnfinishedUpdateDataDirectory(CStdString script) {
	return RegEx::doReplace("/(_restart)?\\.script/", "", script);
}
void kUpdate::removeUnfinishedUpdate(CStdString script) {
	if (!script || _access(script, 04)) return;
	unlink(script);
	CStdString data = getUnfinishedUpdateDataDirectory(script);
	if (!data || _access(data, 00)) return;
	removeDirTree(data);
}



CStdString kUpdate::GetFullPath(CStdString path) {

	// Zamieniamy %...%
	CStdString out;
	ExpandEnvironmentStrings(path , out.GetBuffer(1024) , 1024);
	out.ReleaseBuffer();
	if (out.size() < 3 || out[1]!=':') 
		out = ((char*)ICMessage(IMC_RESTORECURDIR)) + out;
	out.Replace("/", "\\");
	return out;
}

CStdString kUpdate::GetFullUrl(const CStdString & url, const CStdString & parent) {
	RegEx preg;
	if (url.empty() || preg.match("#^\\w+://#", url)) {
		return url;
	}
	// wyci¹gamy poszczególne elementy URLa
	if (!preg.match("#^(\\w+://[^/]+/)([^\\?]+/)?([^\\?/]*)(\\?.*)?$#", parent))
		return url;
	if (url[0] == '.' && (url.length() < 2 || url[1] != '.')) {
		// (http://..../) + (katalog/) + url bez kropki
		return preg[1] + preg[2] + url.substr(1);
	} else if (url[0] == '/') {
		// (http://..../) + url bez kreki
		return preg[1] + url.substr(1);
	} else {
		// (http://..../) + (katalog/) + url
		return preg[1] + preg[2] + url;
	}
}


// kUpdate...


cUpdate::cUpdate(bool force , tUrlList & url, bool onlyNew) {
	this->force = force;
	this->urlList = url;
	this->onlyNew = onlyNew;
	this->hwnd = this->statusWnd = this->treeWnd = 0;
	this->selected = &sections;
	this->advanced = false;
	this->updateId = Time64(true).strftime("%H%M.%y-%m-%d");
	ICMessage(IMC_PLUG_VERSION , ICMessage(IMC_PLUGID_POS , Ctrl->ID()), (int) version.GetBuffer(16));
	version.ReleaseBuffer();
	version = "kUpdate v" + version;

	thread = (HANDLE)Ctrl->BeginThread("cUpdate::ThreadProc", 0 , 0 , (unsigned ( __stdcall *)( void * ))ThreadProc , this , 0 , 0);
}

unsigned int __stdcall cUpdate::ThreadProc(cUpdate * upd) {
	upd->Main();
	CloseHandle(upd->thread);
	delete upd; // ca³oœæ ¿yje tylko na czas w¹tku...
	return 0;
}

// G³ówny "w¹tek"
void cUpdate::Main() {


	try {
		class UpdateContext {
		public:
			UpdateContext() {
				LockerCS l(updatesInProgressCS);
				updatesInProgress.push_back( GetCurrentThreadId() );
			}
			~UpdateContext() {
				LockerCS l(updatesInProgressCS);
				updatesInProgress.remove( GetCurrentThreadId() );
			}
		} ctx;
		if (force) {
			UICreate();
			//SetClassLong(this->hwnd , GCL_HCURSOR , (LONG)LoadCursor(0 , IDC_WAIT));
		}
		// £aduje XMLa
		DoXMLStuff();
		// Sprawdza czy s¹ jakieœ update'y
		if (CheckForUpdates() || force) {
			// Skoro s¹ to ju¿ trzeba pokazaæ UI
			if (!force) {
				HANDLE event = CreateEvent(0 , false , false , 0);
				cMessage m;
				m.action = sUIAction(IMIG_EVENT , IMIA_EVENT_EVENT);
				m.notify = UIIcon(IT_MESSAGE , NET_UPDATE , IDI_TYPE_UPDATE,0);
				m.type = MT_EVENT;
				m.net = NET_UPDATE;
				m.body = "Nowa aktualizacja!";
				m.flag = MF_NOSAVE;
	            CStdString ext = SetExtParam("" , "Title" , "Nowa aktualizacja!");
				ext = SetExtParam(ext , "WaitObject" , Stamina::inttostr((int)event , 16));
				m.ext = (char*)ext.c_str();
				IMLOG("- Update waits for - %x" , event);
				sMESSAGESELECT ms;
				ms.id = Ctrl->IMessage(&sIMessage_2params(IMC_NEWMESSAGE , (int)&m,0));
				Ctrl->IMessage(&sIMessage_2params(IMC_MESSAGEQUEUE , (int)&ms,1));
					
				WaitForSingleObject(event , INFINITE);
				CloseHandle(event);
				UICreate();
			}
			else {
				//SetClassLong(this->hwnd , GCL_HCURSOR , (LONG)LoadCursor(0 , IDC_ARROW));
			}
			UIStart();
			UIStatus("Przejrzyj listê paczek do pobrania...");
			UILoop();
			ApplyUpdates();
			SleepEx(0 , TRUE);
		} else {
			IMLOG("- Nothing to update.");
		}
		SleepEx(0 , TRUE);
	} 
	catch (eUpdateCancel& e) {
		// nic siê nie dzieje... zwyk³e anulowanie
		if (e.inform)
			MessageBox(this->hwnd , "Operacja przerwana przez u¿ytkownika" , "kUpdate" , MB_ICONINFORMATION);
		CleanUp();
	}
	catch (Stamina::Exception& e) {
		if (force || this->hwnd) {
			String nfo = "Wyst¹pi³ b³¹d!\r\n\r\n" + e.toString();
			IMLOG(nfo.a_str());
			MessageBox(this->hwnd , nfo.c_str() , "kUpdate" , MB_ICONERROR);
		}
		CleanUp();
	}
	UIDestroy();
}

VOID CALLBACK cUpdate::APCCancel(ULONG_PTR dwParam) {
	LockerCS lock(updatesInProgressCS);
	if (std::find(updatesInProgress.begin(), updatesInProgress.end(), GetCurrentThreadId() ) != updatesInProgress.end()) {
		throw eUpdateCancel(dwParam!=0);
	}
}

class ExceptionOldfile: public Stamina::Exception {
	virtual String getReason() const {return "";}
};

void cUpdate::DoXMLStuff() {
//    HINTERNET hSession=0 , hConnect=0 , hRequest=0;
	tUrlList::iterator url;
//	hSession = ;
	Stamina::oInternet internet = new Stamina::Internet((HINTERNET) ICMessage(IMC_HINTERNET_OPEN , (int)version.c_str()));

	this->updatePrepare = new cUpdatePrepare(this);
	this->sections.owner = this;
	this->sections.parent = 0;
	int failed = 0;

	this->sections.name = "kUpdate";
	bool cancel = false;

	// przegl¹damy kolejne url'e
	for (url = urlList.begin(); url != urlList.end(); url++) {
		try { /*eUpdate*/ try { /*Internet*/
			CStdString status;
			status.Format("[%d/%d] %s - " , url - urlList.begin() + 1 , urlList.size() , url->title.c_str());
			IMLOG("* Update - %s %s" , status.c_str() , url->url.c_str());

			Stamina::oRequest request;
			CStdString fileUrl;
			// Próbujemy kolejne mirrory a¿ nam ³adnie odpowiedz¹...
			// Je¿eli trafi siê powa¿ny b³¹d to dostaniemy throw
			
			bool checkModTime = (this->onlyNew == true && url->checkTime);
			for (Stamina::tStringVector::iterator mirror = url->mirrors.begin(); mirror != url->mirrors.end(); mirror++) {
	
				IMLOG("* Trying mirror - %s" , mirror->c_str());

				this->UIStatus(string(status + "£¹czê z ") + Stamina::Internet::getUrlHost(*mirror));
				Stamina::oConnection connection = new Stamina::Connection(internet, *mirror);
				processMessages(0);
				fileUrl = *mirror;
				request = new Stamina::Request(connection, *mirror, checkModTime ? Stamina::Request::typeHead : Stamina::Request::typeGet, 0, 0, 0, INTERNET_FLAG_RELOAD);
				if (request->send()) {
					int code = request->getStatusCode();
					IMLOG("* Got code = %d", code);
					if (code == 200 /*OK*/ || code == 304 /*Not Modified*/) {
						break; // przerywamy szukanie, skoro nam siê ju¿ uda³o!
					}
				}
				IMLOG("! Mirror failed!");
				// nie mogliœmy siê po³¹czyæ, próbujemy inny...
				request = 0;
				
			}

			if (!request) {
				throw( eUpdate("Nie mog³em pobraæ pliku definicji!", "¯aden z serwerów nie odpowiedzia³ prawid³owo.") );
			}

			if (checkModTime) { // trzeba sprawdziæ czas i ponowiæ zapytanie GETem
				Date64 modTime = request->getLastModificationTime();
				if (!modTime.empty() && modTime.getTime64() <= dtDefs->getInt64(url->getDefRow(), DTDefs::lastModTime)) { // t¹ wersjê ju¿ znamy... nie ma co jej pobieraæ...
					throw ExceptionOldfile();
				}
				// Zapisujemy modtime na przysz³oœæ...
				dtDefs->setInt64(url->getDefRow(), DTDefs::lastModTime, modTime.getTime64());
				request = new Request( request->getConnection(), request->getUri(), Stamina::Request::typeGet);
				// je¿eli tutaj wywali b³¹d, to nawet bardzo s³usznie...
				request->sendThrowable();
			}

			// na pewno mamy jak¹œ odpowiedŸ, trzeba jeszcze po raz ostatni siê upewniæ...
			if (request->getStatusCode() != 200) {
				throw Stamina::ExceptionInternet("Z³a odpowiedŸ serwera: " + request->getStatusText());
			}

			this->UIStatus(status + "Wczytujê dane");

			std::string response = request->getResponse();

			std::string content_type = request->getInfoString(HTTP_QUERY_CONTENT_TYPE);
			if (response.empty() || content_type.find("xml")==-1) {
				IMLOG("Content_type: %s" , content_type.c_str());
				IMLOG("Size: %d" , response.length());
				IMLOG("Serwer zwróci³: %.300s" , response.c_str());
				ofstream f;
				f.open(GetFullPath("%konnektTemp%\\kUpdate\\update "+ RegEx::doReplace("/[^a-z0-9_-]+/", "_", url->title) +" data error.html") , ios_base::out);
				f << response;
				f.close();
				throw eUpdate("Z³a odpowiedŸ serwera");
			}

			this->UIStatus(status + "Analizujê dane");

			cSectionBranch * branch = new cSectionBranch(&this->sections/*, *url*/);
			bool branchFailed = !branch->ReadNode(response);
			if (this->force == false) {
				__int64 revision = _atoi64(branch->revision.c_str());
				if (revision > 0 && revision <= dtDefs->getInt64(url->getDefRow(), DTDefs::lastRevision)) {
					branchFailed = true;
				} else {
					dtDefs->setInt64(url->getDefRow(), DTDefs::lastRevision, revision);
				}
			}
			if (branchFailed) {
				delete branch;
				// dyskretnie olewamy...
				throw ExceptionOldfile();
			}

			this->sections.data.push_back(branch);

			// uzupe³nia  DlUrl je¿eli jest taka potrzeba...
			branch->SetDlUrl(kUpdate::GetFullUrl(branch->dlUrl, fileUrl));

			// Porz¹dkujemy pozyskane dane...
		} catch (Stamina::ExceptionInternet& e) {
			throw eUpdate("Wyst¹pi³ problem z sieci¹!", e.getReason());
		}
		} catch (ExceptionOldfile& e) {
			// ani failed, ani cancel... po prostu olewamy...
		} catch (eUpdateCancel& e) {
			cancel = true;
		} catch (eUpdate& e) {

			IMLOG("!Wyst¹pi³ b³¹d podczas pobierania danych %s - %s!" , url->central.c_str() , url->title.c_str() , e.getReason().a_str() , e.getExtended().a_str());
			failed++;
			if (this->force)
				if (Ctrl->IMessage(&sIMessage_msgBox(IMI_ERROR , ("B³¹d podczas pobierania '" + String(url->title) + "'\r\n" + e.toString() + "\r\n\r\nJe¿eli problem bêdzie siê powtarza³ mo¿esz wy³¹czyæ centralkê w ustawieniach.").a_str() , "KUpdate - centralka - " + url->central , MB_OKCANCEL , 0)) == IDCANCEL)
					cancel = true;
		}
		if (cancel)
			throw eUpdateCancel(false); // przerywamy przegl¹danie...
	}
	delete this->updatePrepare;
	this->updatePrepare = 0;
	dtDefs->save();
	this->sections.Check(true);

}

bool cUpdate::CheckForUpdates() {
	return this->force || sections.HasSelected(true);	
}

inline CStdString makeNameSafe(const CStdString& name) {
	return RegEx::doReplace("/[^a-z0-9_.]/i", "_", name);
}

void cUpdate::DownloadFile(cPack * pack , cFile * file) {
	static int recurency = 0;
	int done = 0; // ile zosta³o z danego pliku pobrane...

	try {
		CRC.reset();
		DWORD read;
		CStdString action = pack->GetDlUrl();
		//RegEx preg;

		action.Replace("%sectionpath%" , urlEncode(pack->parent->GetSectionName()).c_str());
		action.Replace("%sectionname%" , urlEncode(pack->parent->name).c_str());
		action.Replace("%pack%" , urlEncode(pack->name).c_str());
		action.Replace("%file%" , urlEncode(file?file->name:"").c_str());
		action.Replace("%type%" , urlEncode(pack->GetType()).c_str());

		action.Replace("%_sectionname%" , makeNameSafe(pack->parent->name).c_str());
		action.Replace("%_sectionpath%" , makeNameSafe(pack->parent->GetSectionName()).c_str());
		action.Replace("%_pack%" , makeNameSafe(pack->name).c_str());
		action.Replace("%_file%" , makeNameSafe(file?file->name:"").c_str());

		action.Replace("%fullpath%" , pack->parent->GetPackPath(true).c_str());
		action.Replace("%_fullpath%" , urlEncode(pack->parent->GetPackPath(true)).c_str());

		Stamina::Time64 packTime = *_gmtime64(&pack->packTime);
		__time64_t _fileTime = file ? file->date : pack->packTime;
		Stamina::Time64 fileTime = *_gmtime64(&_fileTime);
		action.Replace("%filetime%" , fileTime.strftime("%y%m%d%H%M%S").c_str());
		action.Replace("%packtime%" , packTime.strftime("%y%m%d%H%M%S").c_str());

		IMDEBUG(DBG_DEBUG, "* downloading %d time - %s", recurency + 1, action.c_str());

		//preg.match("#http\\://(.+?)/(.+)#i" , action);
		CStdString info , packinfo;
		if (pack != updateDownload->last) {
			updateDownload->packDownloaded = 0;
			updateDownload->last = pack;
		}

		if (!updateDownload->connection || !updateDownload->connection->isCompatible(action)) {
			//updateDownload->host = preg[1];
			if (!updateDownload->session)
				updateDownload->session = new Stamina::Internet ( (HINTERNET) ICMessage(IMC_HINTERNET_OPEN , (int)version.c_str()) );

			updateDownload->connection = new Stamina::Connection(updateDownload->session, action);

			info = ("£¹czê z " + updateDownload->connection->getHost()).a_string();
			IMLOG(info);
			updateDownload->dl.info = (char*)info.c_str();
			Ctrl->IMessage(&sIMessage_2params(IMI_LONGSET , (int)&updateDownload->dl , DSET_INFO));

		}
		packinfo = "Pobieram: " + pack->parent->title + "/" + pack->title;
		if (file) packinfo+="/"+file->name;
		IMLOG(packinfo);
		packinfo +="\r\n";
		Stamina::oRequest request = new Stamina::Request(updateDownload->
			connection, action, Stamina::Request::typeGet, 0, 0, 0,  recurency > 0 ? INTERNET_FLAG_RELOAD : 0);
		request->sendThrowable();
		if (request->getStatusCode() != 200) {
			throw eUpdate("Nieprawid³owa odpowiedŸ serwera!", request->getStatusText());
		}
		ofstream f;
		CStdString fileName;
		if (pack->IsTempTarget() && file) {
			fileName = updateDownload->path + file->name;
		} else {
			fileName = updateDownload->pathData + pack->GetTempName() + (file?file->GetTempName():"") + "." + pack->GetType();
		}
		f.open(fileName , ios_base::out|ios_base::binary);
		char buff [4001];
		if (updateDownload->start==0) updateDownload->start = time(0);
		do {
			if (updateDownload->dl.cancel) {
				throw eUpdateCancel(false);
			}
			info = packinfo;
			if (file) info = info + stringf("Plik: [%.2f/%.2f] " , float(done/1024) , float(file->size/1024));
				info = info + stringf("Paczka: [%.2f/%.2f] \r\nCa³oœæ: [%.2f/%.2f]KB %.2fKB/s" 
				, float(updateDownload->packDownloaded/1024) , float(pack->GetSize()/1024)
				, float(updateDownload->downloaded/1024) , float(updateDownload->size/1024)
				, float((updateDownload->downloaded/(max(1,time(0)-updateDownload->start)))/1024));
			updateDownload->dl.info = (char*)info.c_str();
			updateDownload->dl.progress = (int)ceil((float(updateDownload->downloaded) / max(1,updateDownload->size))*100);
			Ctrl->IMessage(&sIMessage_2params(IMI_LONGSET , (int)&updateDownload->dl , DSET_INFO|DSET_PROGRESS));

			read = request->readResponse(buff, 4000);
			if (read == -1)
				throw eUpdate("Problem podczas pobierania "+packinfo);
			buff[read] = 0;
			updateDownload->packDownloaded+=read;
			updateDownload->downloaded+=read;
			done += read;
			CRC.add(buff , read);
			f.write(buff , read);
		} while (read);
		f.close();
		if (!done) {
			_unlink(fileName);
			throw eUpdate("Z³a odpowiedŸ serwera - "+packinfo);
		}
		unsigned int fileCRC32 = pack->GetCRC32() ? pack->GetCRC32() : file ? file->crc32 : 0;
		if (fileCRC32 != CRC.getState()) {
			_unlink(fileName);
			throw eUpdate("Z³a suma kontrolna pliku!\r\n"+packinfo);
		}
		if (file && file->date) { // Dla GZipów trzeba ustawiæ datê
			_utimbuf tb;
			tb.actime = file->date;
			tb.modtime = file->date;
			_utime(fileName , &tb);
		}
		recurency = 0;
	} catch (eUpdateCancel& e) {
		throw e;
	} catch (Stamina::Exception& e) {
		CStdString msg = "Wyst¹pi³ b³¹d podczas pobierania "+(pack ? pack->name : "")+" "+(file ? file->name : "")+":\r\n"+e.toString().a_string();
		// raz ponawiamy bez pytania usera...
		if (recurency < 51 && (recurency == 0 ||MessageBox((HWND)updateDownload->dl.handle , msg , "kUpdate - pobieranie" , MB_ICONERROR | MB_RETRYCANCEL)==IDRETRY)) {
			recurency++;
			/* Trzeba siê cofn¹æ! */
			updateDownload->packDownloaded-=done;
			updateDownload->downloaded-=done;

			DownloadFile(pack , file);
		} else {
			recurency = 0;
			throw eUpdateCancel(false); // wywalamy go dalej...
		}

	}
}


void cUpdate::ApplyUpdates() {
	EnableWindow(treeWnd , false);
	EnableWindow(tbWnd , false);
	// Pobieramy kolejne rzeczy...
	cUpdateDownload ud(this);
	updateDownload = &ud;
	bool needRestart = this->sections.NeedRestart();
	try {
		UIStatus("Pobieram potrzebne pliki...");
		sections.Download();
		UIStatus("Generujê skrypt...");
		ofstream f;
		CStdString pathScript = ud.path + "update_" + this->getUpdateId();
		if (needRestart) {
			pathScript += "_restart";
		}
		pathScript += ".script";
		f.open(pathScript , ios_base::out);
		f << "autobackup 1" << endl << "datadir \"" << ud.pathData << "\"" << endl;
		f << "onsuccess del \"%ScriptFile%\"" << endl;
		if (needRestart) {
			f << "onsuccess spawn \""<< GetFullPath("konnekt.exe") <<"\"" << endl;
			f << "onfailure spawn \""<< GetFullPath("konnekt.exe") <<"\"" << endl;
		}
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "echo \"Aktualizacja rozpocznie siê, gdy naciœniesz START\"" << endl;
		if (needRestart) {
			f << "echo \"Upewnij siê, ¿e KONNEKT jest CA£KOWICIE WY£¥CZONY!\"" << endl;
		}
		//f << "echo \""<< this->sections.name <<"\"" << endl;
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "interact" << endl;
		sections.CreateScript(f);
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "echo \"-------------------------------------------------\"" << endl;
		f << "echo \"Aktualizacja zosta³a zakoñczona pomyœlnie.\"" << endl;
		f << "echo \"Mo¿esz j¹ jeszcze anulowaæ zamykaj¹c to okno.\"" << endl;
		f << "echo \"Aby potwierdziæ aktualizacjê naciœnij 'Zakoñcz'.\"" << endl;
		f << "wait \"Zakoñcz\"" << endl;
		f << "echo \"Usuwam pliki instalacyjne...\"" << endl;
		f << "rmdir %DataDir%" << endl;


		f.close();
		int r = DialogBoxParam(Ctrl->hDll(), MAKEINTRESOURCE(IDD_ASKFORINSTALL), this->hwnd, kUpdate::chooseOptionDialogProc, (LPARAM)pathScript.c_str());
		if (r == IDABORT) {
			kUpdate::removeUnfinishedUpdate(pathScript);
			throw eUpdateCancel(false);
		} else if (r == IDNO || r == IDCANCEL) {
			return;
		}
/*		int r;
		if ((r = MessageBox(hwnd , 
			needRestart
				?"Za chwilê Konnekt zostanie zamkniêty i uruchomi siê specjalny program, który przeprowadzi aktualizacjê.\r\n?" 
				:"Za chwilê uruchomi siê specjalny program, który przeprowadzi aktualizacjê.\r\nKontynuowaæ?"
				, "kUpdate" , MB_ICONINFORMATION | MB_YESNOCANCEL)==IDCANCEL)) {
			throw eUpdateCancel(false);
		} else if (r==IDNO) {
		}
		*/
		kUpdate::runUpdateScript(pathScript);
	} catch (eUpdateCancel& e) {
		throw e;
	} catch (eUpdate& e) {
		throw e;
	}
}
void cUpdate::CleanUp() {
}

int cUpdate::FileNeedUpdate(cPack * pack , cFileEx & file , const char * altPath) {
	CStdString fileName = file.name;
	if (fileName.find_last_of("/\\") != fileName.npos) {
		fileName.erase(0, fileName.find_last_of("/\\") + 1);
	}
	CStdString filePath;
	if (altPath) {
		filePath = GetFullPath(altPath) + fileName;
	} else {
		filePath =  GetFullPath(pack->GetPath()) + file.name;
	}

//	int r = FileNeedUpdate(filename , file.date , file.version);

	int r = 0;
	if (_access(filePath,0)) {
		cFile::enTarget target = file.GetTarget(pack);
		/* sprawdzamy ew. katalog system, skoro w domyœlnej lokacji go nie ma... */
		if (!altPath && (target == cFile::targetLibrary || target == cFile::targetSystem || !pack->searchPath.empty())/* && filePath.Right(4).ToLower()==".dll"*/) {
			CStdString path;
			//CStdString searchPath = (target == cFile::targetLibrary) ? ExpandEnvironmentStrings("%KonnektPath%;%KonnektData%\\dll") : "";
			char * lookFor = (target == cFile::targetLibrary) ? "library" : "system";
			char * filePart;
			char * pathBuffer = path.GetBuffer(255);
			bool found = false;
			if ((target == cFile::targetSystem) && SearchPath(0 , fileName , 0 , 255 , pathBuffer , &filePart))
				found = true;
			else if (target == cFile::targetLibrary) {
				if (SearchPath(expandEnvironmentStrings("%KonnektPath%").c_str() , fileName , 0 , 255 , pathBuffer , &filePart)
					|| SearchPath(expandEnvironmentStrings("%KonnektData%\\dll").c_str() , fileName , 0 , 255 , pathBuffer , &filePart))
					found = true;
			} else if (!pack->searchPath.empty()) {
				found = SearchPath(expandEnvironmentStrings(pack->searchPath).c_str() , fileName , 0 , 255 , pathBuffer , &filePart) != 0;
			}
			if (found) {
				*filePart = 0;
				path.ReleaseBuffer();
				r = FileNeedUpdate(pack , file , path);
				IMLOG("- %s - %s file found in %s - %d (local-remote = %d)" , file.name.c_str() , lookFor, path.c_str() , r , file.localDate - file.date);
				return r;
			} else
				IMLOG("- %s - %s file not found..." , file.name.c_str(), lookFor);
		} // altpath
		IMDEBUG(DBG_DEBUG, "FileNeedUpdate(%s) not found" , file.name.c_str());
		return File_notfound;
	}

	struct _stat st;
	_stat(filePath , &st);
	file.localDate = st.st_mtime;

	if (filePath.size()>4 && (filePath.Right(4).ToLower() == ".exe" || filePath.Right(4).ToLower()==".dll")) {
		FileVersion fvi (filePath);
		if (fvi.empty())
			if (file.version > fvi.getFileVersion()) {
				IMDEBUG(DBG_DEBUG, "FileNeedUpdate(%s) to update (version remote %s > local %s)" , file.name.c_str(), file.version.getString().c_str(), fvi.getFileVersion().getString().c_str());
				r = File_toupdate;
			}
	}
	if (r == 0) {
		if (file.localDate < file.date && abs(3600 - abs(file.localDate - file.date)) > 2 ) {
			IMDEBUG(DBG_DEBUG, "FileNeedUpdate(%s) to update (date remote %s > local %s)" , file.name.c_str(), Time64(file.date).strftime("%d-%m-%Y %H:%M:%S").c_str(), Time64(file.localDate).strftime("%d-%m-%Y %H:%M:%S").c_str());
			r = File_toupdate;
		} else {
			r = File_uptodate;
		}
	}

	if (r != File_notfound && _access(filePath , 02)) // je¿eli nie mo¿na pisaæ po pliku, to restart...
		pack->needRestart = 1;
	return r;
} 
/*
int cUpdate::FileNeedUpdate(CStdString file , time_t date , cVersion version) {
	if (_access(file,0)) 
		return File_notfound;
	if (file.size()>4 && (file.Right(4).ToLower() == ".exe" || file.Right(4).ToLower()==".dll")) {
		FILEVERSIONINFO fvi;
		if (FileVersionInfo(file , 0 , &fvi))
			if ((int)version > (int)cVersion(fvi)) return File_toupdate;
	}
	struct _stat st;
	_stat(file , &st);
	if (st.st_mtime < date && abs(st.st_mtime - date) != 3600) 
		return File_toupdate;
	return File_uptodate;
}
*/



int cUpdate::MessageProc(sIMessage_2params * msg) {
	switch (msg->id) {
		case IM::startInterface: UICreate(true);break;
		case IM::destroyInterface: UIDestroy();break;
	}
	return 0;
}




// **************************************************
// UpdatePREPARE
cUpdatePrepare::~cUpdatePrepare() {
	this->AssignDependants();
}
void cUpdatePrepare::AddDependant(cPack * item , CStdString name) {
	this->dependants.insert(tDepPair(name , item));
}
void cUpdatePrepare::AddDependants(cPack * item , CStdString name) {
	size_t start = 0;
	size_t end;
	name.Replace(" " , "");
	do {
		end = name.find_first_of(";," , start);
		AddDependant(item , name.substr(start , end==-1?end : end-start));
		start = end + 1;
	} while (start != 0);

}
void cUpdatePrepare::AssignDependants() {
	CStdString last = " ";
	cPackBase * lastItem = 0;
	for (tDependants::iterator i = dependants.begin(); i != dependants.end(); i++) {
		if (last != i->first) {
			lastItem = owner->sections.ByName(i->first);
			last = i->first;
		}
		if (lastItem)
			i->second->depends.push_back(lastItem);
	}
	dependants.clear();
}

// 00000000000000000000000000000000000000000000
// updateDownload

cUpdateDownload::cUpdateDownload(cUpdate * owner){
	start=0;
	owner=owner;
	size=0;
	downloaded=0;
	dl.flag = DLONG_ARECV | DLONG_CANCEL;
	dl.handle = owner->hwnd;
	dl.info = "";
	dl.progress = 0;
	dl.title = "kUpdate - pobieranie paczek";
	Ctrl->IMessage(&sIMessage_2params(IMI_LONGSTART , (int)&dl,0));
	size = owner->sections.GetSize();
	path = kUpdate::getTempPath();
	pathData = path + "update_"+owner->getUpdateId()+"\\";
	createDirectories(pathData);
	packDownloaded = 0;
	last = 0;
}
cUpdateDownload::~cUpdateDownload(){
	Ctrl->IMessage(&sIMessage_2params(IMI_LONGEND , (int)&dl,0));
}



}