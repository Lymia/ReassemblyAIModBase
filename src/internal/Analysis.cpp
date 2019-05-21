#include <game/StdAfx.h>
#include "Analysis.h"
#include "Utils.h"
#include "Macros.h"

#include <psapi.h>
#include <stdio.h>

namespace aiModInternal {
#pragma region PE header parsing
	static IMAGE_DOS_HEADER* getReassemblyMzHeader() {
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &moduleInfo, sizeof(MODULEINFO));
		return (IMAGE_DOS_HEADER*)moduleInfo.lpBaseOfDll;
	}
	static IMAGE_NT_HEADERS32* getPeHeader(IMAGE_DOS_HEADER* header) {
		if (header->e_magic != IMAGE_DOS_SIGNATURE)
			reportFatalError("Invalid MZ header.");
		auto peOffset = header->e_lfanew;
		if (peOffset > 1024)
			reportFatalError("MZ header contains excessive e_lfanew.");
		auto peHeader = (IMAGE_NT_HEADERS32*)((char*)header + peOffset);
		if (peHeader->Signature != IMAGE_NT_SIGNATURE)
			reportFatalError("Invalid PE header.");
		return peHeader;
	}
	static IMAGE_SECTION_HEADER* getSectionTable(IMAGE_NT_HEADERS32* headers) {
		return (IMAGE_SECTION_HEADER*)((char*)headers + 4 + sizeof(IMAGE_FILE_HEADER) + headers->FileHeader.SizeOfOptionalHeader);
	}

	struct SegmentInfo {
		bool exists = false;
		char* data;
		size_t size;
	};
	struct TargetSegments {
		SegmentInfo text;
		SegmentInfo rdata;
	};

	static TargetSegments findSegments() {
		auto mzHeader = getReassemblyMzHeader();
		auto peHeader = getPeHeader(mzHeader);
		auto segments = getSectionTable(peHeader);

		TargetSegments targetSegments;

		for (int i = 0; i < peHeader->FileHeader.NumberOfSections; i++) {
			auto segment = &segments[i];

			SegmentInfo info;
			info.exists = true;
			info.data = (char*)mzHeader + segment->VirtualAddress;
			info.size = segment->SizeOfRawData;

			auto name = (char*)segment->Name;

			if (!strcmp(name, ".text")) targetSegments.text = info;
			if (!strcmp(name, ".rdata")) targetSegments.rdata = info;
		}

		if (!targetSegments.text.exists) reportFatalError("Could not locate .text segment");
		if (!targetSegments.rdata.exists) reportFatalError("Could not locate .rdata segment");

		return targetSegments;
	}
	static TargetSegments& getSegments() {
		static auto segments = findSegments();
		return segments;
	}
#pragma endregion
#pragma region Analysis helper functions
	static const char* getFunctionPtr(const char* function) {
		auto ptr = (const char*)GetProcAddress(GetModuleHandle(NULL), function);
		if (ptr == NULL) reportFatalError("Could not find address of %s", function);
		return ptr;
	}
	static std::vector<const char*> findAll(SegmentInfo info, std::string needle) {
		std::vector<const char*> foundList;
		auto seg_begin = info.data;
		auto seg_end = info.data + info.size;

		while (true) {
			auto found = std::search(seg_begin, seg_end, needle.begin(), needle.end());
			if (found != seg_end) {
				foundList.push_back(found);
				seg_begin = found + needle.size();
			}
			else return foundList;
		}
	}
	static const char* findOnly(SegmentInfo info, std::string needle) {
		auto all = findAll(info, needle);
		if (all.size() != 1) {
			return NULL;
		}
		return all[0];
	}

	static std::vector<const char*> findCallOffsets(const char* function, size_t max_check_size) {
		std::vector<const char*> callOffsets;
		for (int i = 0; i < max_check_size; i++, function++) {
			if (*function != (char)0xE8) continue; // CALL rel32
			callOffsets.push_back(function + 5 + (*(ptrdiff_t*)(function + 1)));
		}
		return callOffsets;
	}
	static std::vector<const char*> findOffsetRefs(SegmentInfo info, const char* offset) {
		return findAll(info, std::string((const char*)&offset, 4));
	}
	static std::vector<const char*> findPotentialFunctionEntries(SegmentInfo info, const char* offset, size_t max_check_size) {
		std::vector<const char*> potentialEntries;
		for (auto offset : findOffsetRefs(info, offset)) {
			auto start_offset = (size_t)offset & ~0xF - max_check_size; // align 10h
			for (int i = 0; i <= max_check_size; i += 0x10, start_offset += 0x10)
				potentialEntries.push_back((const char*)start_offset);
		}
		return potentialEntries;
	}
	static const char* findCmpMemImmInstruction(const char* function, size_t max_check_size) {
		std::vector<const char*> cmpTargets;
		for (int i = 0; i < max_check_size; i++, function++) {
			// CMP r/mem16/32 imm8
			if (function[0] != (char)0x83) continue;
			if (function[1] != (char)0x3D) continue;
			if (function[6] != (char)0x00) continue;
			cmpTargets.push_back(*(const char**)(function + 2));
		}
		if (cmpTargets.size() != 1) {
			DPRINT_LOW("Wrong number of CMP found. (got %d)", cmpTargets.size());
			return NULL;
		}
		return cmpTargets[0];
	}
#pragma endregion

#pragma region Notifier::instance, Notifier::notify
	struct NotifierOffsets {
		bool offsetsLoaded = false;
		Notifier& (*Notifier_instance)() = NULL;
		void(__thiscall *Notifier_notify)(Notifier*, const Notification&) = NULL;
	};
	static NotifierOffsets findNotifierOffsets() {
		auto segments = getSegments();
		NotifierOffsets offsets;

		// Find part of an assert string in Notifier::notifier
		auto notifyStringOffset = findOnly(segments.rdata, "Notifier::notify");
		if (!notifyStringOffset) return offsets;
		// Find potential function entry points containing a references to that string
		auto notifyPotentialEntries = findPotentialFunctionEntries(segments.text, notifyStringOffset, 0x200);
		// Find a call to one of those potential entries in Block::addResource
		auto notifyCallOffsets = findCallOffsets(getFunctionPtr("?addResource@Block@@QAEMMU?$tvec2@M$0A@@glm@@@Z"), 0x280); 
		for (auto offset : notifyPotentialEntries) {
			auto begin = notifyCallOffsets.begin();
			auto end = notifyCallOffsets.end();
			auto firstOccurance = std::find(begin, end, offset);
			if (firstOccurance == begin) continue;
			if (firstOccurance == end) continue;
			offsets.offsetsLoaded = true;
			offsets.Notifier_instance = (Notifier& (*)()) *(firstOccurance - 1);
			offsets.Notifier_notify = (void(__thiscall *)(Notifier*, const Notification&)) *firstOccurance;
			break;
		}

		return offsets;
	}
	static NotifierOffsets& getNotifierOffsets() {
		static auto offsets = findNotifierOffsets();
		return offsets;
	}

	bool notifierOffsetsLoaded() {
		return getNotifierOffsets().offsetsLoaded;
	}
	Notifier& Notifier_instance() {
		auto offsets = getNotifierOffsets();
		if (!offsets.offsetsLoaded) reportFatalError("Failed to find Notifier::instance offset.");
		return offsets.Notifier_instance();
	}
	void Notifier_notify(Notifier* notifier, const Notification& notification) {
		auto offsets = getNotifierOffsets();
		if (!offsets.offsetsLoaded) reportFatalError("Failed to find Notifier::notify offset.");
		offsets.Notifier_notify(notifier, notification);
	}
#pragma endregion
#pragma region globals
	static Globals* findGlobals() {
		auto segments = getSegments();
		// Find a `cmp globals.player, 0` instruction in Block::launchUpdate
		auto playerOffset = findCmpMemImmInstruction(getFunctionPtr("?launchUpdate@Block@@AAE_NI@Z"), 0x380);
		if (!playerOffset) {
			DPRINT_LOW("Warning: Failed to find globals offset.");
			return NULL;
		}
		auto start = playerOffset - offsetof(Globals, player);
		return (Globals*) start;
	}
	Globals* tryGetGlobals() {
		static auto globals = findGlobals();
		return globals;
	}
	Globals& getGlobals() {
		auto globals = tryGetGlobals();
		if (!globals) reportFatalError("Failed to find globals offset.");
		return (Globals&) globals;
	}
#pragma endregion
#pragma region Player::setMessage
	typedef void (__thiscall *fn_Player_setMessage)(Player*, string msg);
	static fn_Player_setMessage findPlayerSetMessage() {
		auto segments = getSegments();
		auto getTextOffset = getFunctionPtr("?gettext_@@YAPBDPBD@Z");
		// Find the "Unlocked Faction" text.
		auto unlockedFactionStringOffset = findOnly(segments.rdata, "Unlocked Faction");
		if (!unlockedFactionStringOffset) return NULL;
		// Find all references to the "Unlocked Faction" text.
		auto unlockedFactionOffsets = findOffsetRefs(segments.text, unlockedFactionStringOffset);
		for (auto offset : unlockedFactionOffsets) {
			auto calls = findCallOffsets(offset, 0x40);
			if (calls.size() < 3) continue;
			// Calls in order should be `gettext_`, `String::String`, `Player::setMessage`
			if (calls[0] != getTextOffset) continue;
			return (fn_Player_setMessage) calls[2];
		}
		return NULL;
	}
	static fn_Player_setMessage getPlayer_setMessage() {
		static auto func = findPlayerSetMessage();
		return func;
	}
	bool playerSetMessageLoaded() {
		return getPlayer_setMessage() != NULL;
	}
	void Player_setMessage(Player* player, string msg) {
		auto func = getPlayer_setMessage();
		if (!func) reportFatalError("Failed to find Player::setMessage offset.");
		func(player, msg);
	}
#pragma endregion
}