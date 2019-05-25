#include <game/StdAfx.h>
#include <game/Save.h>
#include "Analysis.h"
#include "AnalysisCore.h"
#include "Utils.h"
#include "Macros.h"

#include <psapi.h>
#include <stdio.h>
#include <unordered_set>

using namespace std::literals;

namespace aiModInternal {
	static PEModule& getReassemblyModule() {
		static PEModule module = PEModule(GetModuleHandle(NULL));
		return module;
	}

#pragma region Notifier::instance, Notifier::notify
	struct NotifierOffsets {
		Notifier& (*Notifier_instance)() = NULL;
		void(__thiscall *Notifier_notify)(Notifier*, const Notification&) = NULL;
	};
	static std::optional<NotifierOffsets> findNotifierOffsets() {
		auto mod = getReassemblyModule();
		auto rdata = mod.getSegment(".rdata");
		auto text = mod.getSegment(".text");

		DPRINT_LOW("Searching for Notifier offsets...");
		NotifierOffsets offsets;

		// Find part of an assert string in Notifier::notifier
		auto notifyStringOffset = rdata.findOnlyString("Notifier::notify\0"s);
		ANALYSIS_TRY(notifyStringOffset);
		ANALYSIS_DBG("0x%p", *notifyStringOffset);
		// Find potential function entry points containing a references to that string
		auto notifyPotentialEntries = text.findPotentialFunctionEntriesByOffset(*notifyStringOffset, 0x200);
		ANALYSIS_DBG_LIST("0x%p", notifyPotentialEntries);
		// Find a call to one of those potential entries in Block::addResource
		auto notifyFunction = mod.parseFunctionByName("?addResource@Block@@QAEMMU?$tvec2@M$0A@@glm@@@Z");
		ANALYSIS_TRY(notifyFunction);
		auto notifyCallOffsets = notifyFunction->getCallOffsets();
			
		for (auto offset : notifyPotentialEntries) {
			auto begin = notifyCallOffsets.begin();
			auto end = notifyCallOffsets.end();
			auto firstOccurance = std::find(begin, end, offset);
			if (firstOccurance == begin) continue;
			if (firstOccurance == end) continue;
			offsets.Notifier_instance = (Notifier& (*)()) *(firstOccurance - 1);
			offsets.Notifier_notify = (void(__thiscall *)(Notifier*, const Notification&)) *firstOccurance;
			ANALYSIS_DBG("0x%p", offsets.Notifier_instance);
			ANALYSIS_DBG("0x%p", offsets.Notifier_notify);
			return offsets;
		}

		DPRINT_LOW("  Could not find call to any Notify::notify possibilities.");
		return std::nullopt;
	}
	static std::optional<NotifierOffsets>& getNotifierOffsets() {
		static auto offsets = findNotifierOffsets();
		return offsets;
	}
	bool notifierOffsetsLoaded() {
		return getNotifierOffsets().has_value();
	}
	Notifier& Notifier_instance() {
		auto offsets = getNotifierOffsets();
		if (!offsets.has_value()) reportFatalError("Failed to find Notifier::instance offset.");
		return offsets->Notifier_instance();
	}
	void Notifier_notify(Notifier* notifier, const Notification& notification) {
		auto offsets = getNotifierOffsets();
		if (!offsets.has_value()) reportFatalError("Failed to find Notifier::notify offset.");
		offsets->Notifier_notify(notifier, notification);
	}
#pragma endregion
#pragma region globals
	static std::optional<Globals*> findGlobals() {
		auto mod = getReassemblyModule();

		DPRINT_LOW("Searching for globals offset...");

		auto func = mod.parseFunctionByName("?launchUpdate@Block@@AAE_NI@Z");
		ANALYSIS_TRY(func);

		// Find a `cmp globals.player, 0` instruction in Block::launchUpdate
		auto playerOffset = func->findOnlyInstruction<Offset>([](const DecodedInstruction& instr) -> std::optional<Offset> {
			if (instr.opcode == ZYDIS_MNEMONIC_CMP) {
				auto decoded = instr.decode();
				if (decoded.operandCount != 3) return std::nullopt;
				if (decoded.operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE) return std::nullopt;
				if (decoded.operands[1].imm.value.u != 0) return std::nullopt;
				ZydisU64 offset;
				auto status = ZydisCalcAbsoluteAddress(&decoded, &decoded.operands[0], &offset);
				if (!ZYDIS_SUCCESS(status)) return std::nullopt;
				return (Offset) offset;
			}
			return std::nullopt;
		}, "`CMP mem32, 0` instruction");
		if (!playerOffset.has_value()) {
			DPRINT_LOW_REPORT("Warning: Failed to find globals offset.");
			return std::nullopt;
		}
		ANALYSIS_DBG("0x%p", *playerOffset);
		auto globalsOffset = *playerOffset - offsetof(Globals, player);
		ANALYSIS_DBG("0x%p", globalsOffset);
		return (Globals*) globalsOffset;
	}
	std::optional<Globals*> tryGetGlobals() {
		static auto globals = findGlobals();
		return globals;
	}
	Globals& getGlobals() {
		auto globals = tryGetGlobals();
		if (!globals.has_value()) reportFatalError("Failed to find globals offset.");
		return (Globals&) *globals;
	}
#pragma endregion
#pragma region Player::setMessage
	typedef void (__thiscall *fn_Player_setMessage)(Player*, string msg);
	static std::optional<fn_Player_setMessage> findPlayerSetMessage() {
		auto mod = getReassemblyModule();
		auto rdata = mod.getSegment(".rdata");
		auto text = mod.getSegment(".text");

		DPRINT_LOW("Searching for Player::setMessage offset...");

		// Find the "Unlocked Faction" text.
		auto unlockedFactionStringOffset = rdata.findOnlyString("Unlocked Faction\0"s);
		ANALYSIS_TRY(unlockedFactionStringOffset);
		ANALYSIS_DBG("0x%p", *unlockedFactionStringOffset);
		// Find all references to the "Unlocked Faction" text.
		auto unlockedFactionOffsets = text.findOffset(*unlockedFactionStringOffset);
		ANALYSIS_DBG_LIST("0x%p", unlockedFactionOffsets);
		auto getTextOffset = mod.getFunctionByName("?gettext_@@YAPBDPBD@Z");
		for (auto offset : unlockedFactionOffsets) {
			auto func = mod.parseFunctionByOffset(offset - 1);
			if (!func.has_value()) continue;
			auto calls = func->getCallOffsets(offset - 1);
			ANALYSIS_DBG_LIST("0x%p", calls);
			if (calls.size() < 3) continue;
			// Calls in order should be `gettext_`, `string::string`, `Player::setMessage`
			if (calls[0] != getTextOffset) continue;
			ANALYSIS_DBG("0x%p", calls[2]);
			return (fn_Player_setMessage) calls[2];
		}

		DPRINT_LOW("  Could not find call to Player::setMessage.");
		return std::nullopt;
	}
	static std::optional<fn_Player_setMessage> getPlayer_setMessage() {
		static auto func = findPlayerSetMessage();
		return func;
	}
	bool playerSetMessageLoaded() {
		return getPlayer_setMessage().has_value();
	}
	void Player_setMessage(Player* player, string msg) {
		auto func = getPlayer_setMessage();
		if (!func.has_value()) reportFatalError("Failed to find Player::setMessage offset.");
		(*func)(player, msg);
	}
#pragma endregion
#pragma region CVarBase::index
	typedef std::map<lstring, CVarBase*>& (*fn_CVarBase_index)();
	static std::optional<fn_CVarBase_index> findCvarBaseIndex() {
		auto mod = getReassemblyModule();
		auto data = mod.getSegment(".data");
		auto rdata = mod.getSegment(".rdata");
		auto text = mod.getSegment(".text");

		DPRINT_LOW("Searching for CVarBase::index offset...");

		// Find offset of the CVarBase RTTI type descriptor.
		auto rttiNameOffset = data.findOnlyString(".?AUCVarBase@@\0"s);
		ANALYSIS_TRY(rttiNameOffset);
		ANALYSIS_DBG("0x%p", *rttiNameOffset);
		auto rttiTypeDescriptorOffset = *rttiNameOffset - offsetof(_TypeDescriptor, name);
		ANALYSIS_DBG("0x%p", rttiTypeDescriptorOffset);
		// Find offset of the CVarBase Complete Object Locator.
		auto rttiColDescriptorOffsets = rdata.findOffset(rttiTypeDescriptorOffset);
		ANALYSIS_DBG_LIST("0x%p", rttiColDescriptorOffsets);
		Offset rttiColOffset = NULL;
		for (auto potentialDescriptorOffset : rttiColDescriptorOffsets) {
			auto potentialOffset = potentialDescriptorOffset  - offsetof(_s__RTTICompleteObjectLocator, pTypeDescriptor);
			auto potentialRefs = rdata.findOffset(potentialOffset);
			ANALYSIS_DBG_LIST("0x%p", potentialRefs);
			if (potentialRefs.size() == 1) {
				rttiColOffset = potentialOffset;
				break;
			}
		}
		if (!rttiColOffset) {
			DPRINT_LOW("  Failed to find CVarBase _s__RTTICompleteObjectLocator.");
			return std::nullopt;
		}
		// Find CVarBase vftable
		auto vftableHeaderOffset = rdata.findOnlyOffset(rttiColOffset);
		ANALYSIS_TRY(vftableHeaderOffset);
		ANALYSIS_DBG("0x%p", *vftableHeaderOffset);
		auto vftableOffset = *vftableHeaderOffset + 4;
		ANALYSIS_DBG("0x%p", vftableOffset);
		// Find potential entry points for the CVarBase constructor
		auto cVarBasePotentialConstructors = text.findPotentialFunctionEntriesByOffset(vftableOffset, 0x200);
		ANALYSIS_DBG_LIST("0x%p", cVarBasePotentialConstructors);

		// Find offset of "kBlockOverlap" string
		auto kBlockOverlapOffset = rdata.findOnlyString("kBlockOverlap\0"s);
		ANALYSIS_TRY(kBlockOverlapOffset);
		ANALYSIS_DBG("0x%p", *kBlockOverlapOffset);
		// Find a function containing the offset of "kBlockOverlap"
		auto kBlockOverlapSetup = text.findOnlyOffset(*kBlockOverlapOffset);
		ANALYSIS_TRY(kBlockOverlapSetup);
		ANALYSIS_DBG("0x%p", *kBlockOverlapSetup);
		// Determine the actual entry point of CVarBase constructor
		auto kBlockOverlapSetupFunc = mod.parseFunctionByOffset(*kBlockOverlapSetup + 4);
		ANALYSIS_TRY(kBlockOverlapSetupFunc);
		auto cvarBaseOffsets = kBlockOverlapSetupFunc->getCallOffsets(*kBlockOverlapSetup + 4);
		ANALYSIS_DBG_LIST("0x%p", cvarBaseOffsets);
		std::unordered_set<Offset> cvarBaseSet(cvarBaseOffsets.begin(), cvarBaseOffsets.end());
		Offset cVarBaseConstructor = NULL;
		for (auto potentialEntry : cVarBasePotentialConstructors) {
			if (cvarBaseSet.find(potentialEntry) != cvarBaseSet.end()) {
				cVarBaseConstructor = potentialEntry;
				break;
			}
		}
		if (!cVarBaseConstructor) {
			DPRINT_LOW("  Failed to find CVarBase::CVarBase");
			return std::nullopt;
		}
		ANALYSIS_DBG("0x%p", cVarBaseConstructor);

		// Assume the second call in the CVarBase constructor is CVarBase::index
		auto cVarBaseFunc = mod.parseFunctionByOffset(cVarBaseConstructor);
		ANALYSIS_TRY(cVarBaseFunc);
		auto constructorCallOffsets = cVarBaseFunc->getCallOffsets();
		ANALYSIS_DBG_LIST("0x%p", constructorCallOffsets);
		if (constructorCallOffsets.size() != 3) {
			DPRINT_LOW("  Wrong number of calls in CVarBase::CvarBase constructor. (expected 3, found %d)", constructorCallOffsets.size());
			return std::nullopt;
		}
		ANALYSIS_DBG("0x%p", constructorCallOffsets[1]);
		return (fn_CVarBase_index) constructorCallOffsets[1];
	}
	static std::optional<fn_CVarBase_index> getCvarBaseIndex() {
		static auto offset = findCvarBaseIndex();
		return offset;
	}
	bool cvarIndexLoaded() {
		return getCvarBaseIndex().has_value();
	}
	std::map<lstring, CVarBase*>& CVarBase_index() {
		auto func = getCvarBaseIndex();
		if (!func.has_value()) reportFatalError("Failed to find CVarBase::index offset.");
		return (*func)();
	}
#pragma endregion
}