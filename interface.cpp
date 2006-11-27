// obs³uguje interfejs

#include "stdafx.h"

#include "konnekt/plug.h"
#include "konnekt/ui.h"
#include "konnekt/plug_export.h"
#include "konnekt/plug_func.h"

#include "konnekt/kUpdate.h"
#include "update.h"
#include "interface.h"
#include "resrc1.h"

 //#define DEBUGVIEW

namespace kUpdate {

void cUpdate::UICreate(bool threadSafe) {
	if (!threadSafe) {
		Ctrl->IMessageDirect(Ctrl->ID() , &sIMessage_2params(IM::startInterface , (int)this , 0));
		return;
	}
	iml = ImageList_Create(16 , 16 , ILC_COLOR24 | ILC_MASK , 10 , 2);
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_BSTART) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_BADVANCED) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_BSELECTALL) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_HELP) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_INONE) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_IYES) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_INO) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_IALL) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_IGRAYED) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_IREQUIRED) , 16));
	ImageList_AddIcon(iml , loadIconEx(Ctrl->hDll() , MAKEINTRESOURCE(IDI_IDEPENDENT) , 16));



	this->loop = false;
	this->UILoop(); // inicjalizujemy kolejkê wiadomoœci dla w¹tku...
	this->hwnd = CreateDialogParam(Ctrl->hDll() , MAKEINTRESOURCE(IDD_UPDATE) , (HWND)UIGroupHandle(sUIAction(0,IMIG_MAINWND)) , UpdateDialogProc , (LPARAM)this);

	this->treeWnd = GetDlgItem(hwnd , IDC_TREE);
	this->infoWnd = GetDlgItem(hwnd , IDC_INFO);
	RichEditHtml::init(this->infoWnd);
	TreeView_SetImageList(treeWnd , iml , TVSIL_NORMAL);
	//EnableWindow(hwnd , false);
	EnableWindow(treeWnd , false);
	EnableWindow(tbWnd , false);
	ShowWindow(this->hwnd , SW_SHOW);
}
void cUpdate::UIDestroy() {
	if (Ctrl->Is_TUS(0)) {
		Ctrl->IMessageDirect(Ctrl->ID() , &sIMessage_2params(IM::destroyInterface , (int)this , 0));
		return;
	}
	if (this->hwnd) {
		HWND wnd = this->hwnd;
		this->hwnd = 0; // Gdy HWND nie jest 0, procedura okna uzna to za anulowanie i wyrzuci b³¹d!
		DestroyWindow(wnd);
	}
}
void cUpdate::UILoop() {
//	MSG msg;
	while(this->loop/* && GetMessage( &msg, NULL, 0, 0 )*/) { 
//		TranslateMessage(&msg); 
//		DispatchMessage(&msg); 
		SleepEx(1 , true);
    } 
}
void cUpdate::UIStart() {
	this->loop = true;
	this->sections.AutoSelect(AutoSelection::toUpdate , true);
	// Uzupe³niamy drzewko
	this->sections.AddTreeItem(this->treeWnd);
	UIRefresh();
	EnableWindow(treeWnd , true);
	EnableWindow(tbWnd , true);
}
void cUpdate::UIStatus(const StringRef& status , int position) {
	if (!this->hwnd || !this->statusWnd) return;
	SendMessage(this->statusWnd , SB_SETTEXT , position , (LPARAM)status.a_str());
}
void cUpdate::UIRefresh() {
	sections.Check(true);
	sections.Check(true); // Tylko ¿eby ustawiæ ikonki...
	CStdString sz;

	sz.Format("Razem %.2f KB" , float(sections.GetSize()) / 1024);
	UIStatus(sz , 2);

	SendMessage(tbWnd , TB_SETSTATE , buttons::start , sections.GetSize()>0?TBSTATE_ENABLED:0);
	repaintWindow(treeWnd);

}
void cUpdate::UIAbort(bool inform) {
	if (thread) {
		QueueUserAPC(APCCancel , thread , inform);
	}
}


// ----------------------------------------------------------------
void cPackBase::AddTreeItem(HWND tree){
	TVINSERTSTRUCT tis;
	memset(&tis , 0 , sizeof(tis));
	tis.hParent = parent? parent->treeItem : 0;
	tis.hInsertAfter = TVI_SORT;
	tis.itemex.mask = TVIF_PARAM | TVIF_TEXT | TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tis.itemex.state = tis.itemex.stateMask = 0;
	tis.itemex.lParam = (LPARAM)this;
	tis.itemex.pszText = this->title.empty()? (char*)this->name.c_str() : (char*)this->title.c_str();
	tis.itemex.iImage = tis.itemex.iSelectedImage = GetIcon();
	AddingTreeItem(&tis);
	this->treeItem = TreeView_InsertItem(tree , &tis);
	RefreshInfo();
}
void cSection::AddTreeItem(HWND tree){
	if (!Owner()->advanced && !HasSelected(true)) return;
	cPackBase::AddTreeItem(tree);
	if (!this->treeItem) return;
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		(*i)->AddTreeItem(tree);
}
void cSectionRoot::AddTreeItem(HWND tree){
	for (tData::iterator i = data.begin(); i!=data.end(); i++)
		(*i)->AddTreeItem(tree);
}

void cPack::AddTreeItem(HWND tree){
	if (this->hidden || (!Owner()->advanced && this->needUpdate==0)) return;
	cPackBase::AddTreeItem(tree);
}
void cSectionBranch::AddingTreeItem(TVINSERTSTRUCT * tis) {
	tis->hInsertAfter = TVI_LAST;
	tis->itemex.stateMask |= TVIS_EXPANDED;
	tis->itemex.state |= TVIS_EXPANDED;
}
void cPack::AddingTreeItem(TVINSERTSTRUCT * vi) {
	if (this->needUpdate) {
		vi->itemex.stateMask |= TVIS_BOLD;
		vi->itemex.state |= TVIS_BOLD;
	}
}

/*
 title
 info
 inform - wiêksze ni¿ info
 warn
 var / value
revisions
rev_s
rev_p
rev
 note - krótka notka...
*/

void kUpdate::InfoWindowStyleCB(const CStdString & token , const CStdString & style , RichEditFormat::SetStyle & ss) {
	if (style == "title") {
		ss.cf.dwMask |= CFM_BOLD | CFM_SIZE | CFM_COLOR;
		ss.cf.yHeight = pixelsToFontHeight(13);
		ss.cf.dwEffects |= CFE_BOLD;
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.cf.crTextColor = 0x800000;
		ss.pf.dwMask |= PFM_ALIGNMENT | PFM_SPACEAFTER;
		ss.pf.wAlignment = PFA_CENTER;
		ss.pf.dySpaceAfter = 20;
	} else if (style == "note") {
		ss.cf.dwMask |= CFM_SIZE | CFM_COLOR;
		ss.cf.yHeight = pixelsToFontHeight(9);
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.cf.crTextColor |= GetSysColor(COLOR_BTNSHADOW);
		ss.pf.dwMask |= PFM_SPACEBEFORE;
		ss.pf.dySpaceBefore = 20;
	} else if (style == "warn") {
		ss.cf.dwMask |= CFM_SIZE | CFM_COLOR;
		ss.cf.yHeight = pixelsToFontHeight(11);
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.cf.crTextColor = 0x0000FF;
	} else if (style == "info") {
		ss.cf.dwMask |= CFM_SIZE;
		ss.cf.yHeight = pixelsToFontHeight(11);
		ss.pf.dwMask |= PFM_ALIGNMENT | PFM_SPACEAFTER | PFM_SPACEBEFORE;
		ss.pf.wAlignment = PFA_JUSTIFY;
		ss.pf.dySpaceAfter = 10;
		ss.pf.dySpaceBefore = 10;
	} else if (style == "inform") {
		ss.cf.dwMask |= CFM_SIZE;
		ss.cf.yHeight = pixelsToFontHeight(12);
		ss.pf.dwMask |= PFM_ALIGNMENT | PFM_SPACEBEFORE;
		ss.pf.wAlignment = PFA_JUSTIFY;
		ss.pf.dySpaceBefore = 20;

	} else if (style == "var") {
		ss.pf.dwMask |= PFM_TABSTOPS;
		ss.pf.cTabCount = 2;
		ss.pf.rgxTabs[0] = pixelsToFontHeight(80);
		ss.pf.rgxTabs[1] = pixelsToFontHeight(140);
	} else if (style == "value") {
		ss.cf.dwMask |= CFM_BOLD;
		ss.cf.dwEffects |= CFE_BOLD;
	} else if (style == "revisions") {
		ss.cf.dwMask |= CFM_UNDERLINE | CFM_COLOR | CFM_SIZE | CFM_BOLD;
		ss.cf.dwEffects |= CFE_UNDERLINE | CFE_BOLD;
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.cf.crTextColor = 0x0000F0;
		ss.cf.yHeight = pixelsToFontHeight(12);
		ss.pf.dwMask |= PFM_ALIGNMENT | PFM_SPACEBEFORE;
		ss.pf.wAlignment = PFA_CENTER;
		ss.pf.dySpaceBefore = 16;
	} else if (style == "rev_s") {
		ss.cf.dwMask |= CFM_UNDERLINE | CFM_UNDERLINETYPE | CFM_SIZE | CFM_COLOR | CFM_BOLD;
		ss.cf.bUnderlineType = CFU_UNDERLINEDOTTED;
		ss.cf.dwEffects |= CFE_UNDERLINE | CFM_BOLD;
		ss.cf.yHeight = pixelsToFontHeight(11);
		ss.cf.crTextColor = 0x0000A0;
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.pf.dwMask |= PFM_SPACEBEFORE;
		ss.pf.dySpaceBefore = 5;
	} else if (style == "rev_p") {
		ss.cf.dwMask |= CFM_SIZE | CFM_BOLD | CFM_COLOR;
		ss.cf.dwEffects |= CFM_BOLD;
		ss.cf.yHeight = pixelsToFontHeight(11);
		ss.cf.crTextColor = 0x000080;
		ss.cf.dwEffects &= ~CFE_AUTOCOLOR;
		ss.pf.dwMask |= PFM_SPACEBEFORE;
		ss.pf.dySpaceBefore = 5;
	} else if (style == "rev") {
		ss.cf.dwMask |= CFM_SIZE | CFM_COLOR;
		ss.cf.dwEffects |= CFE_AUTOCOLOR;
		ss.cf.yHeight = pixelsToFontHeight(10);
		ss.pf.dwMask |= PFM_SPACEBEFORE | PFM_STARTINDENT | PFM_OFFSET;
		ss.pf.dySpaceBefore = 0;
		ss.pf.dxStartIndent = ss.pf.dxOffset = 15;
	}
}
CStdString kUpdate::InfoValue(const CStdString & name , const CStdString & value) {
	return "<div class='var'>" + name + "\t<span class='value'>" + value + "</span></div>";
}
CStdString kUpdate::RNtoBR(CStdString txt) {
	txt.Replace("\n" , "<br/>");
	return txt;
}


void cPackBase::RefreshInfo() {
	cUpdate * owner = Owner();
	if (this->treeItem) {
	//	TreeView_SetCheckState(Owner()->treeWnd , this->treeItem , selected);
		TVITEMEX tie;
		tie.hItem = this->treeItem;
		tie.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tie.iImage = tie.iSelectedImage = GetIcon();
		SendMessage(owner->treeWnd , TVM_SETITEM , 0 , (LPARAM)&tie);
	}
	if (owner->infoWnd && owner->selected == this) {
		// Wrzuca info
		CStdString txt;
		if (this->Type() != nType::root) { /* Root i tak ma to gdzieœ... */
			CStdString nfo;
	#ifdef DEBUGVIEW
			txt = this->title + "<br/>";
			txt+= "Path: " + this->GetPath() + "<br/>";
			txt+= "dlURL: " + this->dlUrl + "<br/>";
			txt+= "URL: " + this->url + "<br/>";
			txt+= this->info + "<br/>";
			nfo.Format("selection: %d\r\ngetSelection: %d\r\nsize: %d\r\nicon: %d\r\nselected: %d\r\n" , selection ,  GetSelection() , GetSize() , GetIcon() , selected);
			txt+= nfo;
	#endif
			txt+= "<div class='title'>" + RNtoBR(this->title) + "</div>";
			if (!this->info.empty()) 
				txt+= "<div class='info'>" + RNtoBR(this->info) + "</div>";
			if (!this->url.empty()) 
				txt+= InfoValue("URL" ,  "<span class='url'>" + this->url + "</span>");
//			nfo.Format("\r\nDo pobrania: %.2fKB\r\n" , float(GetSize())/1024);
//			txt+= nfo;
			nfo = this->GetRevLog();
			if (!nfo.empty())
				txt += "<br/><div class='revisions'>Zmiany</div>" + nfo;

			nfo.Format("%.2f KB" , float(this->GetSize()) / 1024);
			owner->UIStatus(nfo , 1);

		}
		SetInfo(txt);
		/*HACK: na dolegliwoœci richHTML*/
		RichEditHtml::setHTML(owner->infoWnd , "<div align='left'>" + txt + "</div>" , InfoWindowStyleCB);
//		SetWindowText(owner->infoWnd , txt);
	}
}
void cSectionRoot::SetInfo(CStdString & nfo){
	nfo="<div class='title'>kUpdate</div>";
	nfo+="<div class='inform'>";
	if (!HasSelected(true)) {
		nfo+="<u>Nie ma ¿adnych nowoœci.</u>";
	} else {
		nfo+="Zaznacz wszystkie paczki, które chcesz zainstalowaæ i naciœnij '<b>START</b>'.<br/><br/>";
		nfo+="Czerwony wykrzyknik przy pozycji oznacza, ¿e aktualizacja jest dostêpna.<br/>";
		nfo+="Niektóre paczki s¹ ukryte i zaznaczane s¹ automatycznie.";
	}
	nfo+="</div><div class='info'>";
//	nfo+="Aby zobaczyæ wszystkie dostêpne paczki, naciœnij [Wiêcej opcji].";
//	nfo+="<br/>Je¿eli chcesz wymusiæ pobranie ca³ej paczki, kliknij kwadrat przy jej nazwie prawym myszki. Je¿eli klikniesz tak ca³y katalog paczek, zostan¹ 'naprawione' równie¿ ewentualne ukryte paczki...";
	nfo+="W razie problemów/pytañ - naciœnij przycisk '<b>POMOC</b>'.";
	nfo+="<br/><br/>Wszystkie pliki pobierane s¹ do katalogu \""+kUpdate::GetFullPath("%konnektTemp%\\kUpdate")+"\" ";
	nfo+="i po udanej aktualizacji mo¿esz ten katalog skasowaæ. Najlepiej jednak pozostawiæ w nim plik <b>upd.exe</b> - nie trzeba bêdzie go pobieraæ nastêpnym razem...";
	nfo+="</div>";
}

void cSectionBranch::SetInfo(CStdString & nfo) {
	cSection::SetInfo(nfo);
}

void cSection::SetInfo(CStdString & nfo) {
	if (!HasSelected(true)) /* nie ma NIC do zaktualizowania... */
		nfo += "<div class='note'>Sekcja nie zawiera niczego nowego...</div>";
}

void cPack::SetInfo(CStdString & nfo) {
		CStdString add;
#ifdef DEBUGVIEW
		add.Format("needUpdate: %d<br/>required: %d<br/>dependent: %d<br/>type: %d<br/>" , needUpdate , required , dependent , type);
		nfo+= add;
		nfo+="Dependents:  ";
		for (tDepends::iterator i = depends.begin(); i!=depends.end(); i++)
			nfo+= (*i)->name + ",";
		nfo+="<br/>";
#endif
		add = "";
		if (required) add+="Ta paczka jest wymagana.<br/>";
		else
		if (dependent) add+="Ta paczka jest wymagana przez inn¹ paczkê.<br/>";
		if (this->GetTarget() == cFile::targetSystem) add+="Ta paczka jest bibliotek¹ systemow¹.<br/>";
		if (!needUpdate) add+="Paczka nie zawiera niczego nowego...<br/>";
		if (GetSelection() == all) add+="Zostan¹ pobrane wszystkie pliki z paczki.<br/>";
		if (!add.empty())
			nfo += "<div class='note'>" + add + "</div>";
}

void cPackSingle::SetInfo(CStdString & nfo) {
	cPack::SetInfo(nfo);
//	nfo+="\r\ns";
}
void cPackFiles::SetInfo(CStdString & nfo) {
	cPack::SetInfo(nfo);
#ifdef DEBUGVIEW
	nfo+="Files:  ";
	CStdString add;
	for (tFiles::iterator i = files.begin(); i!=files.end(); i++) {
		add.Format("<br/>%d - %s - %d" , i->needUpdate , i->name.c_str() , i->size);
		nfo+= add;
	}
	nfo+="<br/>";
#endif

}


int cSection::GetIcon() {
	if (GetSelection() == yes) return cUpdate::icons::i_yes;
	if (HasSelected()) return cUpdate::icons::i_grayed;
	if (HasSelected(true)) return cUpdate::icons::i_no;
	return cUpdate::icons::i_none;
}
int cPack::GetIcon() {
	if (GetSelection() == all) return cUpdate::icons::i_all;
	if (needUpdate==cUpdate::File_uptodate)
		return cUpdate::icons::i_none;
	if (GetSelection() == yes) return cUpdate::icons::i_yes;
	if (required) return cUpdate::icons::i_required;
	if (dependent > 0) return cUpdate::icons::i_dependent;
	return cUpdate::icons::i_no;
}
// 0000000000000000000000000000000000000000000000000000000000000000
// oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
// ''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

int CALLBACK cUpdate::UpdateDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
	cUpdate * upd = (cUpdate*)GetProp(hwnd , "cUpdate*");
	switch (message)
	{
		case WM_INITDIALOG: {
			cUpdate * upd = (cUpdate*)lParam;
			SetProp(hwnd , "cUpdate*" , (HANDLE)upd);
			Rect rc;
			// Tworzy okno statusu...
			upd->statusWnd = CreateStatusWindow(WS_CHILD | WS_VISIBLE,
				"Gotowy...",hwnd,0);
			int sbw [3]={200 , 300 , -1};
			SendMessage(upd->statusWnd , SB_SETPARTS , 3 , (LPARAM)sbw);
			// tworzy bar'a w miejsce IDC_BAR
			rc = getChildRect(GetDlgItem(hwnd , IDC_BAR));
			DestroyWindow(GetDlgItem(hwnd , IDC_BAR));
			upd->tbWnd = CreateWindowEx(0, TOOLBARCLASSNAME, (LPSTR) NULL,
					WS_CHILD | WS_CLIPCHILDREN |WS_CLIPSIBLINGS | WS_VISIBLE
			//		|TBSTYLE_TRANSPARENT
			//		|CCS_NODIVIDER
					| TBSTYLE_FLAT
			        | TBSTYLE_LIST
			        | CCS_NOPARENTALIGN
			        | CCS_NORESIZE
					| TBSTYLE_TOOLTIPS
					, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hwnd,
					(HMENU)IDC_BAR, 0, 0);
//			SendMessage(upd->tbWnd , TB_SETEXTENDEDSTYLE , 0 ,
							/*TBSTYLE_EX_MIXEDBUTTONS*/
							/*| TBSTYLE_EX_HIDECLIPPEDBUTTONS*/
//							);
			SendMessage(upd->tbWnd, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
			SendMessage(upd->tbWnd, TB_SETIMAGELIST , 0 , (LPARAM)upd->iml);

			TBBUTTON bb;
			bb.iBitmap=icons::start;
			bb.idCommand=buttons::start;
			bb.fsState=TBSTATE_ENABLED;
			bb.fsStyle=BTNS_AUTOSIZE | BTNS_BUTTON;
			bb.iString=(int)"Start";
			bb.dwData=0;
			SendMessage(upd->tbWnd, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb);
			bb.iBitmap=icons::selectAll;
			bb.idCommand=buttons::selectAll;
			bb.iString=(int)"Zaznacz wszystkie";
			SendMessage(upd->tbWnd, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb);
			bb.iBitmap=icons::i_required;
			bb.idCommand=buttons::selectNone;
			bb.iString=(int)"Tylko wymagane";
			SendMessage(upd->tbWnd, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb);
			bb.iBitmap=icons::advanced;
			bb.idCommand=buttons::advanced;
			bb.fsStyle=BTNS_AUTOSIZE | BTNS_CHECK;
			bb.iString=(int)"Wiêcej opcji";
			SendMessage(upd->tbWnd, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb);
			bb.iBitmap=icons::help;
			bb.idCommand=buttons::help;
			bb.fsStyle=BTNS_AUTOSIZE;
			bb.iString=(int)"Pomoc";
			SendMessage(upd->tbWnd, TB_INSERTBUTTON, 0x3FFF , (LPARAM)&bb);
			// IDC_TREE
			break;}
		case WM_CLOSE: {
			if (upd->hwnd) {
				upd->UIAbort();
			}
			//throw eUpdateCancel();
			break;
			}
		case WM_NOTIFY: {
            NMHDR * nm;nm=(NMHDR*)lParam;
			if (!upd || nm->hwndFrom != upd->treeWnd) return 0;
            switch (nm->code) {
//				case TVN_: {
//					break;}
				case TVN_SELCHANGED: {
					NMTREEVIEW * nmt = (NMTREEVIEW*)lParam;
					upd->selected = (cPackBase*) nmt->itemNew.lParam;
					if (upd->selected) {
						upd->selected->RefreshInfo();
					}
					
					break;}
				case NM_CLICK: case NM_RCLICK: {
					TVHITTESTINFO hti;
					GetCursorPos(&hti.pt);
					ScreenToClient(upd->treeWnd , &hti.pt);
					TreeView_HitTest(upd->treeWnd,&hti);
					if (!hti.hItem || !(hti.flags & TVHT_ONITEMICON)) return 0;
					TVITEMEX ie;
					ie.mask = TVIF_PARAM;
					ie.hItem = hti.hItem;
					TreeView_GetItem(upd->treeWnd , &ie);
					cPackBase * item = (cPackBase*) ie.lParam;
					if (nm->code == NM_RCLICK) {
						item->AutoSelect(item->GetSelection() == cPackBase::all ? AutoSelection::setInherit : AutoSelection::setAll , true);
					} else {
						item->AutoSelect(item->GetSelection() == cPackBase::yes ? AutoSelection::setNo : AutoSelection::setYes , true);
					}
					upd->UIRefresh();
					break;}
				case EN_LINK:
					return RichEditHtml::parentWindowProc(hwnd , message , wParam , lParam);

			break;} // WM_NOTIFY
		}
        case WM_COMMAND:
	        switch(HIWORD(wParam)) {
				case BN_CLICKED:
					switch(LOWORD(wParam)) {
						case buttons::start: upd->loop = false; break;
						case buttons::selectAll: upd->sections.AutoSelect(AutoSelection::allNew , true); upd->UIRefresh(); break;
						case buttons::selectNone: upd->sections.AutoSelect(AutoSelection::setNo , true); upd->UIRefresh(); break;
						case buttons::help: ShellExecute(0 , "open" , "doc\\kupdate.html" , "" , "" , SW_SHOW); break;
						case buttons::advanced: 
							upd->advanced = !upd->advanced;
							upd->selected = &upd->sections;
							TreeView_DeleteAllItems(upd->treeWnd);
							upd->sections.AddTreeItem(upd->treeWnd);
							upd->UIRefresh();
							SendMessage(upd->tbWnd , TB_CHECKBUTTON , buttons::advanced , upd->advanced);
							break;
	                }
					break;
             }
             break;
	};
	return 0;
}


int CALLBACK kUpdate::chooseOptionDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		if (lParam) {
			const char * script = (char*)lParam;
			if (kUpdate::scriptNeedsRestart(script)) {
				ShowWindow(GetDlgItem(hwnd, IDC_RESTART), SW_SHOW);
			}
			if (GetDlgItem(hwnd, IDC_INFO)) {
				struct _stat stat;
				if (!_stat(script, &stat)) {
					CStdString info = "Aktualizacja z dnia " + Time64(stat.st_mtime).strftime("%A %d-%m-%y %H:%M");
					SetDlgItemText(hwnd, IDC_INFO, info);
				}
			}

		}
		return 0;
    case WM_COMMAND:
        if (HIWORD(wParam)==BN_CLICKED) {
			EndDialog(hwnd, LOWORD(wParam));
			return 1;
		}
	}
	return 0;
}

}