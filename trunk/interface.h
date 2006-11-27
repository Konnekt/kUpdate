#pragma once
namespace kUpdate {

	void InfoWindowStyleCB(const CStdString & token , const CStdString & styleClass , RichEditFormat::SetStyle & ss);
	CStdString InfoValue(const CStdString & name , const CStdString & value);
	CStdString RNtoBR(CStdString txt);




};