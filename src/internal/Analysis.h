#pragma once

namespace aiModInternal {
	bool notifierOffsetsLoaded();
	Notifier& Notifier_instance();
    void Notifier_notify(Notifier*, const Notification&);

	Globals* tryGetGlobals();
	Globals& getGlobals();

	bool playerSetMessageLoaded();
	void Player_setMessage(Player* player, string msg);
}