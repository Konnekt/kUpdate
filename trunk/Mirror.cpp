#include "stdafx.h"

#include "konnekt/kupdate.h"
#include "update.h"
#include "Mirror.h"

namespace kUpdate {

	using Stamina::tStringVector;

	void MirrorList::getMirrors(const CStdString& url, Stamina::tStringVector& result, const CStdString& allow, const CStdString& fallback) {

		Stamina::tStringVector allowed;
		Stamina::split(allow, ",", allowed, false);

		// ustalamy max priorytet...
		int priority = 0;
		for (tList::iterator it = _list.begin(); it != _list.end(); ++it) {
			if (allowed.size() > 0 && std::find(allowed.begin(), allowed.end(), it->first) == allowed.end()) {
				continue;
			}
			if (it->second.priority > priority) priority = it->second.priority;
		}

		int lotto = Stamina::random(1, priority+1);

		for (tList::iterator it = _list.begin(); it != _list.end(); ++it) {
			if (allowed.size() > 0 && std::find(allowed.begin(), allowed.end(), it->first) == allowed.end()) {
				continue;
			}
			if (it->second.priority >= lotto) {
				result.push_front( kUpdate::GetFullUrl(url, it->second.url) );
			} else {
				result.push_back( kUpdate::GetFullUrl(url, it->second.url) );
			}
		}


		if (result.empty()) 
			result.push_back( kUpdate::GetFullUrl(url, fallback) );

	}


}
