#include "stdafx.h"
#include "konnekt/kupdate.h"
#include "update.h"
#include "interface.h"
namespace kUpdate {

cPackBase * cSection::ByName(CStdString name) {
	if (this->name == name) return this;
	cPackBase * found = 0;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		if ((found = (*i)->ByName(name))) return found;
	return 0;
}

bool cSection::HasSelected(bool maximize) {
	if (cPackBase::HasSelected(maximize)) return true;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		if ((*i)->HasSelected(maximize)) return true;
	return false;
}
bool cSection::NeedRestart() {
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		if ((*i)->NeedRestart()) return true;
	return false;
}


bool cSection::Check(bool recursive) {
	bool sel = cPackBase::Check(recursive);
	if (sel || recursive) {
		for (tData::iterator i = data.begin(); i!=data.end(); i++)
			if (recursive || (*i)->selection==inherit)
				if ((*i)->Check(recursive)) sel=true;
	}
	return sel;
}
int cSection::GetSize() {
	int size = 0;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		size+=(*i)->GetSize();
	return size;
}
void cSection::AutoSelect(AutoSelection::nType type , bool recurse) {
	cPackBase::AutoSelect(type , recurse);
	if (!recurse) return;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		(*i)->AutoSelect(type , recurse);
}

CStdString cSection::GetSectionName() {
	CStdString parentName = parent->GetSectionName();
	if (parentName.empty())
		return this->name;
	else
		return parentName + "/" + this->name;
}

void cSection::Download() {
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		(*i)->Download();
}
void cSection::CreateScript(ofstream &f) {
	//f << "echo \"----- Sekcja "<< this->name <<"\""<< endl;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		(*i)->CreateScript(f);
}

void cSectionBranch::CreateScript(ofstream & f) {
	f << "echo \"--------------------------------\"" << endl;
	f << "echo \"Zestaw - " << this->name << " - '" << this->title << "'\"" << endl;
	f << "echo \"Rewizja - " << this->revision << " / " << this->revisionDate << "\"" << endl;
	f << "echo \"--------------------------------\"" << endl;
	cSection::CreateScript(f);
}


bool cSection::ReadNode(const CStdString & node) {
	Stamina::SXML XML;
	// Wczytujemy info o sekcji...
	XML.loadSource(node);
	this->info = XML.getText("section/info");
	XML.prepareNode("section" , true);
	this->name = XML.getAttrib("name");
	this->url = XML.getAttrib("url");
	this->title = XML.getAttrib("title");
	this->dlUrl = XML.getAttrib("dlURL");
	this->path = XML.getAttrib("path");
	// wczytujemy sekcje...
	CStdString item;
	XML.loadSource(XML.getContent().c_str());
	while (!(item = XML.getNode("section")).empty()) {
		cSection * section = new cSection(this);
		if (!section->ReadNode(item))
			delete section;
		else
			this->data.push_back(section);
		XML.next();
	}
	XML.restart();
	// a teraz paczki
	while (!(item = XML.getNode("pack")).empty()) {
		cPackBase * pack;
		XML.prepareNode("pack",true);
		pack = XML.getAttrib("single") == "1" ? (cPackBase*)new cPackSingle(this) : (cPackBase*)new cPackFiles(this);
		if (!pack->ReadNode(item))
			delete pack;
		else
			this->data.push_back(pack);
		XML.next();
	}
	return true;
}

bool cSectionBranch::ReadNode(const CStdString & node) {
	Stamina::SXML XML;
	XML.loadSource(node);
	XML.prepareNode("update");
	this->revision = XML.getAttrib("revision");
	this->revisionDate = XML.getAttrib("date");

	// normalnie czytamy sekcjê...
	cSection::ReadNode(XML.getNode("update/section"));
	this->info = InfoValue("Aktualizacja" , revision) + InfoValue("Data" , revisionDate) + this->info;
	return true;
}

CStdString cSection::GetRevLog(__time64_t since) {
	CStdString revs;
	/* paczki */
	for (tData::iterator i = data.begin(); i!=data.end(); i++) {
		if ((*i)->Type() < nType::pack) continue;
		CStdString rev = (*i)->GetRevLog(since);
		if (!rev.empty())
			rev = "<div class='rev_p'>" + (*i)->title + "</div>" + rev;
		revs += rev;
	}
	for (tData::iterator i = data.begin(); i!=data.end(); i++) {
		if ((*i)->Type() >= nType::pack) continue;
		CStdString rev = (*i)->GetRevLog(since);
		if (!rev.empty())
			rev = "<div class='rev_s'>" + (*i)->title + "</div>" + rev;
		revs += rev;
	}

	return revs;
}

};