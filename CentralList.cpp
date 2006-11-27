/*
kUpdate - obs³uga listy centralek...

(C)2004 Stamina
Rights reserved
*/

#include "stdafx.h"
#include "update.h"
#include "CentralList.h"

using namespace kUpdate;
using namespace kUpdate::CentralList;

tItems CentralList::items;
int CentralList::lastAction = 2;
bool CentralList::actionsInstantiated=false;

void CentralList::loadList() {
	/* tworzymy akcje */
	UIActionAdd(CentralList::actionGroup , 0 , ACTT_GROUP , "Wybierz centralki z których chcesz pobieraæ aktualizacje");

	Stamina::SXML xml;
	xml.loadFile(kUpdate::GetFullPath("%KonnektData%\\kupdate\\centrals.xml"));
	xml.loadSource(xml.getContent("setting/kupdate/central").c_str());	
	CStdString item;
	while (!(item = xml.getNode("item")).empty()) {
		CentralList::items.push_back(CentralList::Item(item));
		xml.next();
	}

	/* Zamykamy akcje ...*/
	UIActionAdd(CentralList::actionGroup , CentralList::actionHolder , ACTT_HOLDER | ACTR_INIT , "");
	UIActionAdd(CentralList::actionGroup , 0 , ACTT_GROUPEND , "");

	// po za³adowaniu trzeba odœwie¿yæ ustawienie...
	unserialize(GETSTR(kUpdate::CFG::centrals));
	SETSTR(kUpdate::CFG::centrals, serialize());


}

CStdString CentralList::serialize() {
	CStdString value = "";
	tItems::iterator it;
	for (it = items.begin(); it != items.end(); ++it) {
		value += it->serialize() + "\n";
	}
	return value;
}
void CentralList::unserialize(const CStdString value) {
	RegEx preg;
	tItems::iterator it;
	for (it = items.begin(); it != items.end(); ++it) {
		CStdString urlEscaped = preg.replace("/[^\\w\\d_-]/", "\\\\0", it->getUrl());
		if (preg.match("/^\\!?"+urlEscaped+".*$/m", value)>0) {
			it->unserialize(preg[0]);
		} else {
			it->unserialize("");
		}
	}
}
int CentralList::actionProc(sUIActionNotify_base * anBase) {
	if (anBase->act.id == actionHolder) {
		if (anBase->code == ACTN_CREATE) {
			// wczytujemy dane zaraz po tym, gdy nasze "elementy" zosta³y stworzone...
			CentralList::unserialize(Ctrl->DTgetStr(DTCFG, 0 , CFG::centrals));
			CentralList::actionsInstantiated = true;
		} else if (anBase->code == ACTN_DESTROY) {
			CentralList::actionsInstantiated = false;
		}
		return 0;
	} else {
		// wysylamy do wszystkich...
		tItems::iterator it;
		for (it = items.begin(); it != items.end(); ++it) {
			it->actionProc(anBase);
		}
	}
	return 0;
}

// ItemBase -----------------------------------------------------

CStdString CentralList::ItemBase::getValue() {
	if (this->_required) return "1";
	sUIAction act(actionGroup, this->_action);
	if (this->_action != 0 && UIActionHandle(act) != 0) {
		return UIActionCfgGetValue(act, 0, 0);
	} else {
		return this->_value ? "1" : "0";
	}
}
void CentralList::ItemBase::setValue(CStdString value) {
	if (this->_required) value = "1";
	if (value.empty()) value = this->_default ? "1" : "0";
	sUIAction act (actionGroup, this->_action);
	if (this->_action != 0 && UIActionHandle(act) != 0) {
		UIActionCfgSetValue(act, value);
	}
	this->_value = (value == "1");
}

// Item ---------------------------------------------------------
CentralList::Item::Item(const CStdString source) {
	Stamina::SXML xml;
	xml.loadSource(source);
	CStdString info = xml.getText("item/info");
	xml.prepareNode("item", true);
	this->_name = xml.getAttrib("name");
	this->_default = xml.getAttrib("default") == "1";
	this->_level = (ShowBits::enLevel) Stamina::chtoint(xml.getAttrib("level").c_str());
	if (!this->_level) this->_level = ShowBits::levelBeginner;
	this->_required = xml.getAttrib("required") == "1";
	this->_main = xml.getAttrib("main") == "1";
	this->_url = xml.getAttrib("url");
	CStdString title = xml.getAttrib("title");
	if (title.empty())
		title = this->_name;
	/* Tworzymy akcje... */
	if (ShowBits::checkLevel(this->_level)) {
		this->_action = CentralList::actionBranch | (++CentralList::lastAction);

		int status = ACTT_CHECK;
		if (this->_required) status |= ACTS_DISABLED;
		if (this->_main) status |= ACTSC_BOLD;
		CStdString txt = "  "+title;
		if (!info.empty()) {
			txt = SetActParam(txt, AP_TIP, info);
		}
		UIActionAdd(CentralList::actionGroup , this->_action , status , txt);

		CStdString option;
		xml.loadSource(xml.getContent().c_str());
		while (!(option = xml.getNode("option")).empty()) {
			this->_options.push_back(CentralList::Option(option));
			xml.next();
		}
	} else {
		this->_action = 0;
	}

	
}

CStdString CentralList::Item::serialize() {
	CStdString url = this->_url;
	if (url.find('?') == url.npos) {
		url += "?clist=1";
	}
	tOptions::iterator it;
	for (it = this->_options.begin(); it != this->_options.end(); ++it) {
		CStdString value = it->serialize();
		if (value.empty() == false) {
			url += '&';
			url += value;
		}
	}
	if (this->getValue() != "1")
		url = "!" + url;
	return "## " + this->_name + "\n" + url;

}
void CentralList::Item::unserialize(const CStdString url) {
	if (url.empty()) {
		this->setValue(this->_default ? "1" : "0");
	} else {
		this->setValue(url[0]!='!' ? "1" : "0");
	}
	tOptions::iterator it;
	for (it = this->_options.begin(); it != this->_options.end(); ++it) {
		it->unserialize(url);
	}
}
int CentralList::Item::actionProc(sUIActionNotify_base * anBase) {
	/* na razie nawet nic nie potrzebujemy... */
	return 0;
}



// Option ---------------------------------------------------------

CentralList::Option::Option(const CStdString source) {
	Stamina::SXML xml;
	xml.loadSource(source);
	CStdString info = xml.getText("option/info");
	xml.prepareNode("option", true);
	this->_name = xml.getAttrib("name");
	this->_default = xml.getAttrib("default") == "1";
	this->_level = (ShowBits::enLevel) Stamina::chtoint(xml.getAttrib("level").c_str());
	if (!this->_level) this->_level = ShowBits::levelBeginner;
	this->_required = xml.getAttrib("required") == "1";
	this->_sendAlways = xml.getAttrib("sendAlways") == "1";
	this->_main = xml.getAttrib("main") == "1";
	CStdString title = xml.getAttrib("title");
	if (title.empty())
		title = this->_name;
	/* Tworzymy akcje... */
	if (ShowBits::checkLevel(this->_level)) {
		this->_action = CentralList::actionBranch | (++CentralList::lastAction);
		int status = ACTT_CHECK;
		if (this->_required) status |= ACTS_DISABLED;
		if (this->_main) status |= ACTSC_BOLD;
		CStdString txt = "  "+title;
		if (!info.empty()) {
			txt = SetActParam(txt, AP_TIP, info);
		}
		UIActionCfgAdd(CentralList::actionGroup , this->_action , status , txt, 0, 20);

	} else {
		this->_action = 0;
	}
}

CStdString CentralList::Option::serialize() {
	CStdString value = this->getValue();
	if (value != "1" && !this->_sendAlways) return "";
	return this->_name + "=" + urlEncode(value);
}

void CentralList::Option::unserialize(const CStdString url) {
	RegEx preg;
	if (preg.match("/[\\?&]"+this->_name+"=([^&]+)/", url) > 0) {
		this->setValue(preg[1]);
	} else {
		this->setValue("");
	}
}
	


