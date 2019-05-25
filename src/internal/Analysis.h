#pragma once

#include <game/Save.h>

#include <optional>

namespace aiModInternal {
	bool notifierOffsetsLoaded();
	Notifier& Notifier_instance();
    void Notifier_notify(Notifier*, const Notification&);

	std::optional<Globals*> tryGetGlobals();
	Globals& getGlobals();

	bool playerSetMessageLoaded();
	void Player_setMessage(Player* player, string msg);

	bool cvarIndexLoaded();
	std::map<lstring, CVarBase*>& CVarBase_index();
}