
#include "stdafx.h"
#include "konnekt/plug.h"
#include "konnekt/kupdate.h"
#include "update.h"

namespace kUpdate {

cPackBase * cPackBase::ByName(CStdString name) {
	if (this->name == name) return this;
	return 0;
}
cUpdate * cPackBase::Owner() {return parent->Owner();}

CStdString cPackBase::GetPackPath(bool makeSafe) {
	CStdString name = makeSafe ? makeNameSafe(this->name) : this->name;
	if (parent && parent->Type() != nType::branch) {
		return parent->GetPackPath(makeSafe) + "/" + name;
	} else {
		return name;
	}
}


CStdString cPackBase::GetPath() {
	if (!this->path.empty())
		return this->path;
	else
		return parent ? parent->GetPath() : "";
}
CStdString cPackBase::GetDlUrl() {
	if (!this->dlUrl.empty())
		return this->dlUrl;
	else
		return this->parent ? this->parent->GetDlUrl() : "";
}
void cPackBase::SetDlUrl(CStdString url) {
	if (url.empty()) return;
	CStdString parentUrl;
	if (this->parent)
		parentUrl = this->parent->GetDlUrl();
	this->dlUrl = kUpdate::GetFullUrl(url, parentUrl);
}


CStdString cPackBase::GetTempName() {
	if (!parent) return "";
	return parent->GetTempName() + this->name+ "-";
}


cPackBase::nSelection cPackBase::GetSelection() {
	if (this->selection != inherit)
		return this->selection;
	else
		return parent ? parent->GetSelection() : inherit;
}

bool cPackBase::IsSelected(bool maximize) {
	nSelection sel = GetSelection();
	return (sel == nSelection::yes || sel == nSelection::all);
}
bool cPackBase::Check(bool recursive) {
	bool sel = IsSelected();
	if (sel!=selected) {
		selected = sel;
		return true;
	}
	RefreshInfo();
	return false;
}

void cPackBase::AutoSelect(AutoSelection::nType type , bool recurse) {
	if (type == AutoSelection::setAll) this->selection=all;
	else if (type == AutoSelection::setYes) this->selection=yes;
	else if (type == AutoSelection::setNo) this->selection=no;
	else if (type == AutoSelection::setInherit) this->selection=inherit;
}



bool cPack::IsSelected(bool maximize) {
	nSelection sel = GetSelection();
	if (sel == nSelection::all) return true;
	if (needUpdate) {
		if (required || dependent || sel == yes) return true;
		if (this->needUpdate == cUpdate::File_notfound && this->additional) {
			return false;
		}
		if (maximize) return true;
	}
	return false;
}

bool cPackBase::HasSelected(bool maximize) {
	return IsSelected(maximize);
}


bool cPack::Check(bool recursive) {
	if (cPackBase::Check(recursive)) {
		for (tDepends::iterator i = depends.begin(); i!=depends.end(); i++) {
			(*i)->dependent+=selected?1:-1;
			(*i)->Check(false);
		}
		return true;
	}
	return false;
}
CStdString cPack::GetType() {
	switch (type) {
		case this->gz: return "gz";
		case this->zip: return "zip";
		default: return "plain";
	}
}



bool cPack::ReadNode(const CStdString & node) {
	Stamina::SXML XML;
	// Wczytujemy info o paczce...
	XML.loadSource(node);
	this->info = XML.getText("pack/info");
	// Szykujemy siê do czytania atrybutów
	XML.prepareNode("pack");
	this->name = XML.getAttrib("name");
	this->url = XML.getAttrib("url");
	this->title = XML.getAttrib("title");
	this->dlUrl = XML.getAttrib("dlURL");
	this->path = XML.getAttrib("path");
	this->hidden = XML.getAttrib("hidden") == "1";
	this->required = XML.getAttrib("required") == "1";
	this->isNew = XML.getAttrib("new") == "1";
	this->needRestart = XML.getAttrib("restart") == "1";
	this->noinstall = XML.getAttrib("noinstall") == "1";
	this->recommended = XML.getAttrib("recommended") == "1";
	this->additional = XML.getAttrib("additional") == "1";
	this->target = cFile::ParseTarget(XML, cFile::targetAuto);
	this->searchPath = XML.getAttrib("searchpath");
//	this->system = XML.getAttrib("system") == "1";
	if (this->target == cFile::targetAuto && ((this->name == "UPD2") || (this->GetPath().find("%konnektTemp%") == 0))) {
		this->target = cFile::targetTemp;
	}

	CStdString type = XML.getAttrib("type");
	this->type = type=="gz" ? nPackType::gz 
		: type=="zip" ? nPackType::zip
		: type=="" ? nPackType::none
		: nPackType::unknown;
	type = XML.getAttrib("depends");
	if (!type.empty())
		Owner()->updatePrepare->AddDependants(this , type);
	if (!this->IsTempTarget()) {
		Owner()->updatePrepare->AddDependant(this , "UPD2");
	}
	// Dalsza czêœæ zale¿y ju¿ od typu kontrolki...
	if (!ReadNode(XML)) return false;
	CStdString item;
	// a teraz paczki
	XML.loadSource(XML.getContent("pack").c_str());
	while (!(item = XML.getNode("file")).empty()) {
		XML.prepareNode("file" , true);
		Stamina::SXML::Position bck = XML.pos;
		if (!this->ReadFileNode(XML)) break;
		XML.pos = bck;
		XML.next();
	}
	/* revLog */
	XML.restart();
	while (!(item = XML.getNode("rev")).empty()) {
		XML.prepareNode("rev" , true);
		revLog.push_back(cRevItem(XML));
		XML.next();
	}

	return true;
}

void cPack::AutoSelect(AutoSelection::nType type , bool recurse) {
	cPackBase::AutoSelect(type , recurse);
	if (type == AutoSelection::toUpdate) this->selection= (needUpdate == cUpdate::File_toupdate) ? yes : no;
	else if (type == AutoSelection::allNew) this->selection= (needUpdate != cUpdate::File_uptodate) ? yes : no;
}

CStdString cPack::GetRevLog(__time64_t since) {
//	if (!maxTime) return "";
	if (since == -1) since = maxTime + 1;
	CStdString rev;
	for (tRevLog::iterator it = revLog.begin(); it != revLog.end(); it++) {
		if (it->time >= since && abs(3600 - abs((int)(it->time - since))) > 2) {
			rev += "<div class='rev'>" + it->info + "</div>";
		} else 
			break; // nastêpne i tak bêd¹ jeszcze starsze...
	}
	rev.Replace("\n" , "<br/>");
	return rev;
}

bool cPack::IsTempTarget() {
	return this->GetTarget() == cFile::targetTemp;
//	return this->GetPath().find("%konnektTemp%") == 0;
//	return (kUpdate::GetFullPath(this->GetPath()).find(kUpdate::GetFullPath("%konnektTemp%")) == 0);
}


// SINGLE ---------------------------------------------------

bool cPackSingle::ReadNode(Stamina::SXML & XML) {
	this->size = atoi(XML.getAttrib("size").c_str());
	this->crc32 = atoi(XML.getAttrib("crc32").c_str());
	return true;
}
bool cPackSingle::ReadFileNode(Stamina::SXML & XML) {
	// Wystarczy sprawdziæ czy któryœ z plików
	// opisuj¹cych nie jest "na czasie"
	cFileEx file(XML , false);
	int r = cUpdate::FileNeedUpdate(this , file);
	if (file.localDate > this->maxTime) this->maxTime = file.localDate;
	if (file.date > this->packTime) this->packTime = file.date;
	if (!(noinstall && r == cUpdate::File_notfound) && r) {
		this->needUpdate=r; //return false; /* ¿eby przelecia³ wszystkie i ustali³ maxTime... */
	}
	return true;
}
int cPackSingle::GetSize() {
	return IsSelected() ? size : 0;
};

CStdString cPackSingle::GetTempName() {
	return cPackBase::GetTempName() + ".zip";
}

void cPackSingle::Download() {
	if (!IsSelected()) return;
	Owner()->DownloadFile(this);
}
void cPackSingle::CreateScript(ofstream &f) {
	if (!IsSelected()) return;
	f << "canignore " << (required?0:1) << endl;
	//f << "echo \"pack - "<<this->name<<"\"" << endl;
	if (type != none)
		f << "unpack";		
	else
		f << "copy";		
	f <<" \""<<GetTempName()<<"."<<GetType()<<"\" \""<<kUpdate::GetFullPath(GetPath())<<"\""<<endl;
}


// FILES -----------------------------------------------------

bool cPackFiles::ReadNode(Stamina::SXML & XML) {
	return true;
}
bool cPackFiles::ReadFileNode(Stamina::SXML & XML) {
	cFileEx file (XML);
	int r = cUpdate::FileNeedUpdate(this , file);
	if (file.localDate > this->maxTime) this->maxTime = file.localDate;
	if (file.date > this->packTime) this->packTime = file.date;
	if (r) {
		file.needUpdate = r;
		if (!(noinstall && r == cUpdate::File_notfound) && this->needUpdate < cUpdate::File_toupdate) this->needUpdate = r;
	}
	files.push_back(file);
	return true;
}

int cPackFiles::GetSize() {
	if (!IsSelected()) return 0;
	int size = 0;
	for (tFiles::iterator i=files.begin(); i!=files.end(); i++)
		if (i->needUpdate>0 || GetSelection() == all) size+=i->size;
	return size;
};

void cPackFiles::Download() {
	if (!IsSelected()) return;
	cUpdate * owner = Owner();
	for (tFiles::iterator i=files.begin(); i!=files.end(); i++) {
		if (i->needUpdate>0 || GetSelection() == all) {
			owner->DownloadFile(this , &(*i));
		}
	}
}
void cPackFiles::CreateScript(ofstream &f) {
	if (!IsSelected() || this->IsTempTarget()) return;
//	f << "echo \"packFiles - "<<this->name<<"\"" << endl;
	f << "canignore " << (required?0:1) << endl;
	for (tFiles::iterator i=files.begin(); i!=files.end(); i++) {
		if (i->needUpdate>0 || GetSelection() == all) {
			if (type != none)
				f << "unpack";		
			else
				f << "copy";		
			f <<" \""<<GetTempName()<<i->GetTempName()<<"."<<GetType()<<"\" \""<<kUpdate::GetFullPath(GetPath())<<i->name<<"\""<<endl;
		}
	}
}


// ---------------------------------
// FILE

CStdString cFile::GetTempName() {
	return RegEx::doReplace("#\\\\|/#" , "_" , this->name);
}


cFile::cFile(Stamina::SXML & XML , bool getAll) {
	this->name = XML.getAttrib("name");
	this->path = XML.getAttrib("path");
    this->date = atoi(XML.getAttrib("date").c_str());
	this->crc32 = atoi(XML.getAttrib("crc32").c_str());
	this->target = ParseTarget(XML, targetAuto);

	this->needUpdate = 0;
	if (getAll) {
		this->dlURL = XML.getAttrib("dlURL");
		this->size = atoi(XML.getAttrib("size").c_str());
	} else {this->size=0;}
}

cFile::enTarget cFile::GetTarget(cPack * pack) {
	if (this->target != targetAuto)
		return this->target;
	else if (pack) 
		return pack->GetTarget();
	else
		return targetNormal;
}
cFile::enTarget cFile::ParseTarget(Stamina::SXML & XML, enTarget def = targetNormal) {
	enTarget target = (enTarget) atoi(XML.getAttrib("target").c_str());
	if (target == targetAuto) {
		if (XML.getAttrib("system") == "1") {
			target = targetSystem;
		} else if (XML.getAttrib("library") == "1") {
			target = targetLibrary;
		}
	}
	return target;
}


cFileEx::cFileEx(Stamina::SXML & XML , bool getAll):cFile(XML , getAll) {
	this->version = Version( atoi(XML.getAttrib("version").c_str()) );
}

// ---------------------------------
// REV

cRevItem::cRevItem(Stamina::SXML & XML) {
	this->time = _atoi64(XML.getAttrib("time").c_str());
	this->info = XML.getText();
}

};