#pragma once
/*
kUpdate - obs³uga listy centralek...

(C)2004 Stamina
Rights reserved
*/


namespace kUpdate {
	/*
	Ustawienia listy przechowywane s¹ w dotychczasowej lokalizacji w postaci:
	!http://adres_centralki?opcje=wartosci
	*/
	namespace CentralList {
		class ItemBase {
		protected:
			bool _default;
			bool _value;
			bool _required;
			CStdString _name;
			ShowBits::enLevel _level;
			int _action;
			bool _main;
		public:
			CStdString getValue();
			void setValue(CStdString value);
		};


		class Option : ItemBase {
		private:
			bool _sendAlways;
		public:
			Option(const CStdString source);
			CStdString serialize();
			void unserialize(const CStdString url);
			int actionProc(sUIActionNotify_base * anBase);
			class Item * getParent();
		};

		class Item : ItemBase {
		private:
			CStdString _url;
			typedef deque <Option> tOptions;
			tOptions _options;
		public:
			Item(const CStdString source);
			__inline CStdString getUrl() {return _url;}
			CStdString serialize();
			void unserialize(const CStdString url);
			int actionProc(sUIActionNotify_base * anBase);
		};
		typedef deque <Item> tItems;
		const int actionBranch = 0x14000000;
		const int actionHolder = actionBranch | 1;

		extern tItems items;
		extern int lastAction;
		extern bool actionsInstantiated;

		const int actionGroup = kUpdate::IMIA::gCfg;

		/** Wczytuje listê.
			Mo¿e byæ wywo³ane tylko gdzies po #IM_UIPREPARE
		*/
		void loadList();
		CStdString serialize();
		void unserialize(const CStdString url);

		//unloadList();
		int actionProc(sUIActionNotify_base * anBase);

	};
};