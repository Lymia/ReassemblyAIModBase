#pragma once

#include "Macros.h"
#include "Utils.h"

#include <Windows.h>
#include <zydis/Zydis.h>
#include <vector>
#include <unordered_map>
#include <optional>

namespace aiModInternal {
	typedef uint8_t* Offset;
	struct ParsedFunction;

	struct Segment final {
		std::string name;
		Offset base;
		size_t definedLength;
		size_t length;

		inline bool containsOffset(Offset offset) const {
			return base < offset && offset < (base + length);
		}
		void boundsCheck(Offset offset) const;
		std::string toString() const;

		std::vector<Offset> findString(string str) const;
		std::optional<Offset> findOnlyString(string str) const;
		
		std::vector<Offset> findOffset(Offset offset) const;
		std::optional<Offset> findOnlyOffset(Offset offset) const;

		std::vector<Offset> findPotentialFunctionEntries(Offset offset, size_t maxInstructions) const;
		std::vector<Offset> findPotentialFunctionEntriesByOffset(Offset offset, size_t maxInstructions) const;
		std::vector<Offset> findPotentialFunctionEntriesByString(string str, size_t maxInstructions) const;
	};
	struct PEModule final {
		PEModule(HMODULE mod);

		inline const char* name() const {
			return modName.c_str(); 
		}

		bool hasSegment(const char* name) const;
		bool containsOffset(Offset offset) const;
		std::optional<Segment> tryGetSegment(const char* name) const;
		Segment getSegment(const char* name) const;
		std::optional<Segment> segmentForOffset(Offset offset) const;

		std::optional<Offset> getFunctionByName(const char* name) const;
		std::optional<ParsedFunction> parseFunctionByName(const char* name, size_t maxInstructions = 10000) const;
		std::optional<ParsedFunction> parseFunctionByOffset(Offset offset, size_t maxInstructions = 10000) const;

		void boundsCheck(Offset offset) const;

	private:
		HMODULE mod;
		string modName;
		std::unordered_map<string, Segment> segments;
		vector<Segment> linearSegments;
	};

	struct DecodedInstruction final {
		Offset instrAddress;
		ZydisU8 length;
		ZydisMnemonic opcode;
		ZydisInstructionCategory category;

		DecodedInstruction() { }
		DecodedInstruction(ZydisDecodedInstruction& instr);
		inline ZydisDecodedInstruction decode() const;
	};
	struct ParsedFunction final {
		Offset functionStart;
		std::vector<DecodedInstruction> instructions;
		ParsedFunction(Offset functionStart, std::vector<DecodedInstruction> instructions) :
			functionStart(functionStart), instructions(instructions) { }

		bool isOffsetInFunction(Offset offset) const;
		size_t findInstructionsAfter(Offset offset) const;

		std::vector<Offset> getCallOffsets(Offset after = NULL) const;

		template <typename Ret, typename Fn>
		std::vector<Ret> findInstructions(Fn fn, const char* criteria0 = NULL, Offset after = NULL) const {
			auto criteria = criteria0 ? criteria0 : "instructions matching criteria";
			DPRINT_LOW("  Searching for %s...", criteria);
			std::vector<Ret> values;
			auto idx = after ? findInstructionsAfter(after) : 0;
			for (auto it = instructions.begin() + idx; it != instructions.end(); it++) {
				std::optional<Ret> matches = fn(*it);
				if (matches.has_value()) values.push_back(std::move(*matches));
			}
			return values;
		}
		template <typename Ret, typename Fn>
		std::optional<Ret> findOnlyInstruction(Fn fn, const char* criteria0 = NULL, Offset after = NULL) const {
			auto criteria = criteria0 ? criteria0 : "instructions matching criteria";
			auto list = findInstructions<Ret, Fn>(fn, criteria, after);
			if (list.size() == 0) return std::nullopt;
			if (list.size() != 1) reportFatalError("    Found more than %d %s.", list.size(), criteria);
			return std::move(list[0]);
		}
	};
}

