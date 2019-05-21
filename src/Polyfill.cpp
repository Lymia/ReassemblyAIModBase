#include <game/StdAfx.h>
#include <game/Player.h>

#include "internal/Analysis.h"
#include "internal/Utils.h"
#include "internal/Macros.h"

#include <algorithm>

Globals& globals = aiModInternal::getGlobals();

#pragma region Player
void Player::setMessage(string msg) {
	aiModInternal::Player_setMessage(this, msg);
}
#pragma endregion

#pragma region Notification
Notifier& Notifier::instance() {
	return aiModInternal::Notifier_instance();
}
void Notifier::notify(const Notification& notification) {
	return aiModInternal::Notifier_notify(this, notification);
}
void Notifier::registerHandler(uint64 types, INotifyHandler* handler) {
	int idx = 0, removed = 0, added = 0;
	for (; idx < m_handlers.size(); idx++, types >>= 1) {
		auto& list = m_handlers[idx];
		auto bitSet = types & 1;
		auto contains = std::find(list.begin(), list.end(), handler) != list.end();
		if (contains && !bitSet) {
			list.erase(std::remove(list.begin(), list.end(), handler), list.end());
			removed++;
		}
		else if (!contains && bitSet) {
			list.push_back(handler);
			added++;
		}
	}
	if (types) DPRINT(NOTIFICATION, ("Attempt to register unknown notification types!"));
	DPRINT(NOTIFICATION, ("Added %d and removed %d notification handlers for handler at 0x%p.", added, removed, handler));
}
Notification::Notification(ENotification t, const char* msg, ...) __printflike(3, 4) : Notification(t) {
	va_list vl;
	va_start(vl, msg);
	std::string s = str_vformat(msg, vl);
	va_end(vl);
	message = s;
}
ENUM_TO_STR_FN(eNotificationName, ENotification, NOTIFICATION_TYPE);
string Notification::toString() const {
	// TODO: Implement this properly.
	return str_format("{type=%s, ...}", eNotificationName(type));
}
#pragma endregion