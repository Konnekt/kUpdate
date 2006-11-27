/*
#define UPD_HOST "jego.stamina.eu.org"
#define UPD_URL "~stamina/konnekt/k_msg.php"
*/

namespace kUpdate {

// wyj¹tek
class eUpdate: public Stamina::ExceptionString {
public:
	eUpdate(const StringRef& msg="" , const StringRef& ext=""):ExceptionString(msg),_ext(ext) {
	}

	virtual String toString(iStringFormatter* format=0) const {
		String info =  this->getReason();
		if (this->_ext.empty() == false)
			info += "\r\n\r\n" + this->_ext;
		return info;
	}

	const String& getExtended() {
		return _ext;
	}

protected:
	String _ext;
};
class eUpdateCancel:public eUpdate {
public:
	bool inform;
	eUpdateCancel(bool inform=false):inform(inform){} 
};


}

#include "pack.h"

using namespace Tables;


namespace kUpdate {

	const int maxIdSize = 1024;
	const int retryTime = 10 * 60 * 1000; // w razie braku po³¹czenia...

	extern Tables::oTable dtDefs;


	namespace DTDefs {
		const tColId file = 1;
		const tColId lastModTime = 2;
		const tColId lastRevision = 3;
		const tColId lastOldestFile = 4;
	};

	CStdString getTempPath();
	void runUpdateScript(CStdString script);
	CStdString checkUnfinishedUpdates();
	void runUnfinishedUpdates(bool giveMessage = false);
	void removeUnfinishedUpdate(CStdString script);
	bool scriptNeedsRestart(CStdString script);
	CStdString getUnfinishedUpdateDataDirectory(CStdString script);
	CStdString GetFullPath(CStdString path);
	CStdString GetFullUrl(const CStdString & url, const CStdString & parent);
	int CALLBACK chooseOptionDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

	CStdString postDataToServer(CStdString & url, HINTERNET hSession, const CStdString hdrs, const CStdString frmData);

	CStdString makeNameSafe(const CStdString& name);


// -----------------------------------------------------
// -----------------------------------------------------


class cUpdate {
	friend class cUpdatePrepare;
	friend class cPack;
	friend class cPackSingle;
	friend class cPackFiles;
	friend class cPackBase;
	friend class cSection;
	friend class cUpdateDownload;

public:
	class cUrlItem {
	public:
		cUrlItem() {
			external = false; 
			_row = Tables::rowNotFound;
			checkTime = false;
		}
		CStdString url;
		CStdString central;
		CStdString title;

		bool external;
		bool checkTime;
		Stamina::tStringVector mirrors;

		CStdString definitionId() {
			if (this->external) {
				return url;
			} else {
				return this->central + " :: " + this->url;
			}
		}

		tRowId getDefRow() {
			if (_row != Tables::rowNotFound) {
				return _row;
			}
			_row = dtDefs->findRow(0, Konnekt::Tables::Find::EqStr(dtDefs->getColumn(DTDefs::file), this->definitionId()));
			if (_row == rowNotFound) {
				_row = dtDefs->addRow();
				dtDefs->setString(_row, DTDefs::file, this->definitionId().c_str());
			}
			return _row;
		}

		operator == (const cUrlItem& b) {
			return this->url == b.url;
		}

		

	private:
		tRowId _row;
	};
	typedef vector<cUrlItem> tUrlList;



private:
	bool force;
	bool onlyNew;
	tUrlList urlList; // url do pobrania a póŸniej komenda do wywo³ania
	cSectionRoot sections;
	class cUpdatePrepare * updatePrepare; // obiekt trzymaj¹cy rzeczy potrzebne podczas zbierania danych
	class cUpdateDownload * updateDownload;
	time_t date;
	HANDLE thread;
	CStdString version;
	CRC32 CRC;
	CStdString updateId;

	static unsigned int __stdcall ThreadProc(cUpdate * upd);
	// Zaczyna wykonywanie
	void Main();
	// Pobiera XML'a, przegl¹da go i wype³nia drzewko
	void DoXMLStuff();
	// Na podstawie drzewka sprawdza pliki i decyduje czy
	// cokolwiek jest do sprawdzenia (przy force jest coœ zawsze!)
	bool CheckForUpdates();
	// Generuje skrypt, koñczy Konnekta i odpala UPD...
	void ApplyUpdates();

	void CleanUp();

	// Interfejs --------------------------------------
	bool advanced; // Tryb "zaawansowany"

	HWND hwnd; // uchwyt okna...
	HWND statusWnd;
	HWND treeWnd , infoWnd , tbWnd;
	HIMAGELIST iml;
	bool loop;
	cPackBase * selected;

	struct buttons {
		 const static int start=2001;
		 const static int advanced=2002;
		 const static int selectAll=2003;
		 const static int selectNone=2004;
		 const static int help=2005;
	};
	struct icons {
		 const static int start=0;
		 const static int advanced=1;
		 const static int selectAll=2;
		 const static int help=3;
		// itemsy
		 const static int i_none=	4; // bez zaznaczenia, pusty kwadrat
		 const static int i_yes=	5; // zaznaczony
		 const static int i_no=		6; // bez zaznaczenia
		 const static int i_all=	7; // do "naprawy"
		 const static int i_grayed=	8; // zaznaczone dzieci
		 const static int i_required=9; // wymagane
		 const static int i_dependent=10; // wymagane przez paczki

	};

	void UIDestroy();
	// Zaczyna "interakcjê"
	void UIStart();
	// Wykonuje standardow¹ pêtlê windowsow¹ dopóki loop==true
	void UILoop();
	void UIAbort(bool inform = false);
	// Ustawia status
	void UIStatus(const StringRef& status , int position=0);

	void UIRefresh();

	static int CALLBACK UpdateDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
//	static int CALLBACK ProgressDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
//	static int CALLBACK NewerDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);

	static VOID CALLBACK APCCancel(ULONG_PTR dwParam);

	void DownloadFile(cPack * pack , cFile * file=0);

public:

	void UICreate(bool threadSafe=false);
	int MessageProc(sIMessage_2params * msg);

	cUpdate(bool force , tUrlList & url, bool onlyNew);

	static int FileNeedUpdate(cPack * pack , cFileEx & file , const char * altPath=0); // XML z wybranym "file"
//	static int FileNeedUpdate(CStdString file , time_t date , cVersion version);

	const CStdString getUpdateId() {return this->updateId;}

	const static int File_uptodate = 0;
	const static int File_notfound = 1;
	const static int File_toupdate = 2;
};

class cUpdateDownload {
public:
	cUpdate * owner;
	sDIALOG_long dl;
	int size;
	int downloaded;
	Stamina::oInternet session;
	Stamina::oConnection connection;	
	//CStdString host;
	time_t start;
	CStdString path;
	CStdString pathData;
	cPackBase * last;
	int packDownloaded;
	cUpdateDownload(cUpdate * owner);
	~cUpdateDownload();
};

class cUpdatePrepare {
private:
	cUpdate * owner;
	typedef multimap<CStdString , cPack *> tDependants;
	typedef pair<CStdString , cPack *> tDepPair;
	tDependants dependants;
public:
	cUpdatePrepare(cUpdate * owner):owner(owner) {}
	~cUpdatePrepare();
	void AddDependant(cPack * item , CStdString name);
	void AddDependants(cPack * item , CStdString name);
	void AssignDependants();
};

};

