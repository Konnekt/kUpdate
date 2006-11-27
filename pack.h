namespace kUpdate {

namespace AutoSelection {
	enum nType {
		toUpdate, 
		allNew, 
		setAll,
		setYes,setNo,setInherit
	};
};

// podstawa
class cPackBase {
protected:
	bool selected; // Czy jest "w³¹czony" jako zaznaczony
public:
	enum nSelection {
		no ,  // nie w³¹czony
		yes , // w³¹czony (ale wcale nie oznacza to, ¿e zostanie zainstalowany!
		all , // coœ w rodzaju REPAIR - pobiera wszystkie pliki jakie siê da
		inherit ,
	};
	enum nType {
		base , root = 1 , section = 2 , branch = 3 , pack = 0x10
	};

	class cSection * parent;
	CStdString name;
	CStdString title;
	CStdString info;
	CStdString dlUrl;
	CStdString path;
	nSelection selection;
	unsigned char dependent; // Ile jest w³¹czonych zale¿nych od niego...
	CStdString url;
	HTREEITEM  treeItem;
	cPackBase(cSection * parent):parent(parent) {  
		selection = inherit;
		dependent = 0;
		selected = false;
		treeItem = 0;
	}
	virtual nType Type() {return nType::base;}
	virtual bool ReadNode(const CStdString & node) {return false;}
	virtual class cUpdate * Owner();
	virtual cPackBase * ByName(CStdString name);
	CStdString GetPath();
	CStdString GetDlUrl();
	CStdString GetPackPath(bool makeSafe);
	void SetDlUrl(CStdString url);
	nSelection GetSelection();
	virtual void AutoSelect(AutoSelection::nType type , bool recurse);
	virtual int GetSize() {return 0;} // podaje ile do pobrania...
	virtual int GetIcon() {return 0;} // Zwraca numer ikonki
	virtual bool NeedRestart() {return false;}

	// maximize - uznaje za zaznaczone równie¿ te, które wymagaj¹ update'u
	virtual bool IsSelected(bool maximize=false);
	virtual bool HasSelected(bool maximize=false);
	// Sprawdza czy paczka jest dobrze "oznaczona" (selected)
	// Zwraca czy by³y ró¿ne
	virtual bool Check(bool recursive);
	// INTERFACE
	virtual void AddTreeItem(HWND tree);
	virtual void AddingTreeItem(TVINSERTSTRUCT * vi) {}
	virtual void RefreshInfo(); // Ustawia dane w interfejsie
	virtual void SetInfo(CStdString & nfo) {}
	virtual CStdString GetRevLog(__time64_t since = -1) = 0;
	// Instalowanie
	virtual void Download() {}
	virtual void CreateScript(ofstream & f) {}
	virtual CStdString GetTempName();
};

// sekcja
class cSection : public cPackBase {
public:
	typedef deque <class cPackBase *> tData;
	tData data; // dane - paczki i sekcje nale¿¹ce do sekcji...

	nType Type() {return section;}
	bool ReadNode(const CStdString & node);
	cPackBase * ByName(CStdString name);
	bool HasSelected(bool maximize=false);
	cSection(cSection * parent):cPackBase(parent) {}
	bool Check(bool recursive);
	void AddTreeItem(HWND tree);
	int GetSize();
	void AutoSelect(AutoSelection::nType type , bool recurse);
	int GetIcon();
	void Download();
	void CreateScript(ofstream & f);
	virtual CStdString GetSectionName();
	bool NeedRestart();
	CStdString GetRevLog(__time64_t since = -1);
	void SetInfo(CStdString & nfo);

};
class cSectionRoot : public cSection {
public:
	class cUpdate * owner;
	virtual class cUpdate * Owner() {return owner;}
	nType Type() {return nType::root;}
	cSectionRoot():cSection(0) {selection = no;}
	CStdString GetSectionName() {return "";}
	void AddTreeItem(HWND tree);
	void SetInfo(CStdString & nfo);
//	nType Type() {return nType::root;}
};
/* Sekcja wyci¹gniêta jako root z pliku XML */
class cSectionBranch : public cSection {
public:
	CStdString revision;
	CStdString revisionDate;
//	cUpdate::cUrlItem defsUrl;

	nType Type() {return nType::branch;}
	cSectionBranch(cSection * parent/*, cUpdate::cUrlItem url*/):cSection(parent)/*,defsUrl(url)*/ {}
	CStdString GetSectionName() {return "";}

	bool ReadNode(const CStdString & node);
	void CreateScript(ofstream & f);
	
	void SetInfo(CStdString & nfo);
	void AddingTreeItem(TVINSERTSTRUCT * vi);

//	nType Type() {return nType::root;}
};


// plik
class cFile {
public:
	enum enTarget {
		targetAuto = 0, 
		targetNormal = 1,
		targetLibrary = 2,
		targetSystem = 3,
		targetTemp = 4
	};

	CStdString name;
	CStdString dlURL;
	CStdString path;
	unsigned int size;
	time_t date;
	time_t localDate; // czas lokalnego pliku
	unsigned needUpdate : 2;
	enTarget target : 4;
	unsigned int crc32;
	cFile() {needUpdate = 0; size = 0; target = targetAuto;}
	cFile(Stamina::SXML & XML , bool getAll = true);
	CStdString GetTempName();
	enTarget GetTarget(class cPack * pack);
	static enTarget ParseTarget(Stamina::SXML & xml, enTarget def);
};
class cFileEx: public cFile {
public:
	Stamina::Version version;
	cFileEx(Stamina::SXML & XML , bool getAll = true);
};

class cRevItem {
public:
	CStdString info;
	__time64_t time;
	cRevItem(Stamina::SXML & XML);
};

// paczka
class cPack : public cPackBase {
public:
	enum nPackType {
		none , gz , zip , unknown
	};
	nPackType type;
	unsigned hidden : 1;
	unsigned required : 1;
	unsigned needUpdate : 2; // Któryœ z plików trzeba "unowoczeœniæ" 2 - jest nowa wersja 1 - jest nowy plik
	unsigned isNew : 1;
	unsigned needRestart : 1;
	unsigned noinstall : 1; 
	unsigned recommended : 1;
	unsigned additional : 1;
	//unsigned system : 1;  // Czy paczka zawiera biblioteki systemowe
	cFile::enTarget target : 4;
	//bool tempTarget : 1;  // Czy paczka docelowo ma wyl¹dowaæ w Temp'ie
	typedef vector <cPackBase *> tDepends;
	tDepends depends;
	typedef vector <cRevItem> tRevLog;
	tRevLog revLog;
	__time64_t maxTime; // 'czas' najstarszego lokalnego pliku
	__time64_t packTime;
	CStdString searchPath;

	nType Type() {return nType::pack;}
	bool ReadNode(const CStdString & node);
	virtual bool ReadNode(Stamina::SXML & XML) {return true;}
	virtual bool ReadFileNode(Stamina::SXML & XML) {return false;}
	cPack(cSection * parent):cPackBase(parent),  hidden(0),required(0),needUpdate(0),needRestart(0),noinstall(0),maxTime(0),packTime(0), target(cFile::targetAuto) {}
	bool IsSelected(bool maximize=false);
	bool Check(bool recursive);
	void AddTreeItem(HWND tree);
	void AddingTreeItem(TVINSERTSTRUCT * vi);
	void AutoSelect(AutoSelection::nType type , bool recurse);
	int GetIcon();
	CStdString GetType();
	void SetInfo(CStdString & nfo);
	bool NeedRestart() { return IsSelected() && needRestart;}
	CStdString GetRevLog(__time64_t since = -1);
	virtual unsigned int GetCRC32() {return 0;}
    bool IsTempTarget();
	virtual cFile::enTarget GetTarget() {return this->target != cFile::targetAuto ? this->target : cFile::targetNormal;}
};
class cPackSingle : public cPack {
public:
	unsigned size : 24;
	unsigned int crc32;
	bool ReadNode(Stamina::SXML & XML);
	bool ReadFileNode(Stamina::SXML & XML);
	cPackSingle(cSection * parent):cPack(parent){}
	int GetSize();
	CStdString GetTempName();
	void Download();
	void CreateScript(ofstream & f);
	void SetInfo(CStdString & nfo);
	unsigned int GetCRC32() {return crc32;}
};
class cPackFiles : public cPack {
public:
	typedef deque <cFile> tFiles;
	tFiles files;
	bool ReadNode(Stamina::SXML & XML);
	bool ReadFileNode(Stamina::SXML & XML);
	cPackFiles(cSection * parent):cPack(parent){}
	int GetSize();
	void Download();
	void CreateScript(ofstream & f);
	void SetInfo(CStdString & nfo);
};

};