#include <game/StdAfx.h>
#include <core/Str.h>

#include "AnalysisCore.h"
#include "Macros.h"
#include "Utils.h"

#include <algorithm>
#include <map>
#include <psapi.h>

namespace aiModInternal {
	static ZydisDecoder createDecoder() {
		ZydisDecoder decoder;
		ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);
		return decoder;
	}
	static ZydisDecoder& getDecoder() {
		static auto decoder = createDecoder();
		return decoder;
	}

#pragma region Segment related code
	static std::string internSegmentName(Offset offset) {
		auto offsetStr = (const char*) offset;
		char buffer[9];
		buffer[8] = 0;
		memcpy(buffer, offsetStr, 8);
		return std::string(offsetStr, strlen(offsetStr));
	}

	void Segment::boundsCheck(Offset offset) const {
		if (!containsOffset(offset)) {
			auto str = toString();
			reportFatalError("Invalid access to offset 0x%p in %s", offset, str.c_str());
		}
	}
	std::string Segment::toString() const {
		return str_format("%s (0x%p, 0x%x bytes)", name, base, length);
	}

	static IMAGE_DOS_HEADER* getMzHeader(HMODULE mod) {
		MODULEINFO moduleInfo;
		GetModuleInformation(GetCurrentProcess(), mod, &moduleInfo, sizeof(MODULEINFO));
		return (IMAGE_DOS_HEADER*) moduleInfo.lpBaseOfDll;
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
	static IMAGE_SECTION_HEADER* getSegmentTable(IMAGE_NT_HEADERS32* headers) {
		return (IMAGE_SECTION_HEADER*)((char*)headers + 4 + sizeof(IMAGE_FILE_HEADER) + headers->FileHeader.SizeOfOptionalHeader);
	}
	PEModule::PEModule(HMODULE mod0) {
		mod = mod0;
		modName = getModuleName(mod);

		DPRINT_LOW("Parsing header for module %s...", modName);

		auto mzHeader = getMzHeader(mod);
		auto peHeader = getPeHeader(mzHeader);
		auto segmentTable = getSegmentTable(peHeader);

		for (int i = 0; i < peHeader->FileHeader.NumberOfSections; i++) {
			auto segmentInfo = &segmentTable[i];

			Segment segment;
			segment.name = internSegmentName(segmentInfo->Name);
			segment.base = (Offset) mzHeader + segmentInfo->VirtualAddress;
			segment.definedLength = segmentInfo->SizeOfRawData;
			segment.length = segmentInfo->Misc.VirtualSize;

			if (segment.definedLength > segment.length) segment.definedLength = segment.length;
			if (segments.find(segment.name) != segments.end())
				reportFatalError("Duplicate segment %s in module %s.", segment.name, modName.c_str());

			DPRINT_LOW("  Segment %s @ 0x%p (definedLength = 0x%x, length = 0x%x)",
			           segment.name.c_str(), segment.base, segment.definedLength, segment.length);
			linearSegments.push_back(segment);
			segments[segment.name] = segment;
		}
		std::sort(linearSegments.begin(), linearSegments.end(),
	              [](const Segment& a, const Segment& b) { return a.base < b.base; });
		if (linearSegments.size() > 1) for (int i = 0; i < linearSegments.size() - 1; i++) {
			auto last = linearSegments[i], next = linearSegments[i + 1];
			auto lastEnd = last.base + last.length;
			if (lastEnd > next.base) 
				reportFatalError("Segment %s overlaps segment %s in module %s.", last.name, next.name, modName.c_str());
		}
	}

	bool PEModule::hasSegment(const char* name) const {
		return segments.find(name) != segments.end();
	}
	bool PEModule::containsOffset(Offset offset) const {
		return segmentForOffset(offset).has_value();
	}
	std::optional<Segment> PEModule::tryGetSegment(const char* name) const {
		auto find = segments.find(name);
		if (find == segments.end()) return std::nullopt;
		else return find->second;
	}
	Segment PEModule::getSegment(const char* name) const {
		auto find = tryGetSegment(name);
		if (!find.has_value()) reportFatalError("Could not find segment %s", name);
		else return *find;
	}
	std::optional<Segment> PEModule::segmentForOffset(Offset offset) const {
		auto bound = std::lower_bound(
			linearSegments.begin(), linearSegments.end(), offset,
			[](const Segment& segment, const Offset& offset) { return segment.base < offset; }
		);
		if (bound != linearSegments.begin()) {
			if (bound->containsOffset(offset)) return *bound;
			bound--;
			if (bound->containsOffset(offset)) return *bound;
		}
		return std::nullopt;
	}

	void PEModule::boundsCheck(Offset offset) const {
		if (!containsOffset(offset))
			reportFatalError("Offset 0x%p is not in module %s.", offset, modName.c_str());
	}
#pragma endregion

#pragma region Function parsing related code
	DecodedInstruction::DecodedInstruction(ZydisDecodedInstruction& instr) {
		ASSERT_FATAL(instr.addressWidth == ZYDIS_ADDRESS_WIDTH_32);
		ASSERT_FATAL(instr.instrAddress <= 0xFFFFFFFFL);
		instrAddress = (Offset) instr.instrAddress;
		length = instr.length;
		opcode = instr.mnemonic;
		category = instr.meta.category;
	}
	inline ZydisDecodedInstruction DecodedInstruction::decode() const {
		ZydisDecodedInstruction instr;
		auto status = ZydisDecoderDecodeBuffer(&getDecoder(), (const void*) instrAddress, length, (ZydisU64) instrAddress, &instr);
		ASSERT_FATAL(ZYDIS_SUCCESS(status));
		return instr;
	}

	static std::optional<Offset> parseBranchTarget(ZydisDecodedInstruction& instr) {
		ZydisU64 offset;
		auto status = ZydisCalcAbsoluteAddress(&instr, &instr.operands[0], &offset);
		if (ZYDIS_SUCCESS(status)) return (Offset) offset;
		else return std::nullopt;
	}

	static std::optional<ParsedFunction> findInstructions(
		Segment seg, Offset initialOffset, size_t maxInstructions
	) {
		auto decoder = getDecoder();

		std::map<Offset, DecodedInstruction> instructions;
		std::vector<Offset> uncheckedOffsets;
		uncheckedOffsets.push_back(initialOffset);

		// Iterate through every potential branch starting offset.
		DPRINT_LOW("  Parsing function at 0x%p...", initialOffset);
		ZydisDecodedInstruction instr;
		while (uncheckedOffsets.size() > 0) {
			auto offset = uncheckedOffsets.back();
			uncheckedOffsets.pop_back();

			if (instructions.find(offset) != instructions.end()) continue;
			if (!seg.containsOffset(offset)) {
				DPRINT_LOW("    Function jumps to offset 0x%p outside of segment %s!", offset, seg.name);
				return std::nullopt;
			}
			auto remaining = seg.length - (offset - seg.base);

			// Decode instructions until we hit an unconditional branch or return.
			for (;;) {
				if (instructions.find(offset) != instructions.end()) break;

				auto status = ZydisDecoderDecodeBuffer(&decoder, (const void*) offset, remaining, (ZydisU64) offset, &instr);
				if (!ZYDIS_SUCCESS(status)) {
					DPRINT_LOW("    Failed to parse instruction at offset 0x%p (status code: 0x%x)", offset, status);
					return std::nullopt;
				}
				if (instructions.size() > maxInstructions) {
					DPRINT_LOW("    Found more than %d instructions, assuming analysis failed somewhere.", maxInstructions);
					return std::nullopt;
				}

				auto continuesToNext = true;
				switch (instr.meta.category) {
					case ZYDIS_CATEGORY_UNCOND_BR:
						continuesToNext = false;
						// intentionally falls through
					case ZYDIS_CATEGORY_COND_BR: {
						auto branchTarget = parseBranchTarget(instr);
						if (branchTarget.has_value()) uncheckedOffsets.push_back(*branchTarget);
						else DPRINT_LOW("    Found unparsable branch at 0x%p. Ignoring.", offset);
						break;
					}
					case ZYDIS_CATEGORY_INTERRUPT:
						DPRINT_LOW("    Found interrupt instruction at 0x%p, assuming analysis left the function.", offset);
						// intentionally falls through
					case ZYDIS_CATEGORY_RET:
						continuesToNext = false;
						break;
				}
				instructions[offset] = DecodedInstruction(instr);
				if (!continuesToNext) break;

				remaining -= instr.length;
				offset += instr.length;
			}
		}

		// Check for overlapping instructions, and count some instructions in the meantime.
		ptrdiff_t maxGap = 0;
		size_t gapCount = 0;
		ASSERT_FATAL(instructions.size());
		if (instructions.size() > 1) 
			for (auto last = instructions.begin(), curr = ++instructions.begin(); curr != instructions.end(); last++, curr++) {
				auto lastAddr = last->second.instrAddress + last->second.length;
				auto currAddr = curr->second.instrAddress;
				if (currAddr < lastAddr) {
					DPRINT_LOW("    Instruction at 0x%p overlaps instruction at 0x%p.", last->first, curr->first);
					return std::nullopt;
				}
				if (currAddr != lastAddr) {
					maxGap = max(maxGap, currAddr - lastAddr);
					gapCount++;
				}
			}

		// Create ParsedFunction.
		std::vector<DecodedInstruction> linearInstructions;
		for (auto pair : instructions) linearInstructions.push_back(pair.second);
		auto parsed = ParsedFunction(initialOffset, linearInstructions);
		DPRINT_LOW("    Found %d instructions. (range = [0x%p, 0x%p], gaps = %d, max gap = %d)", 
		           linearInstructions.size(),
		           (Offset) linearInstructions[0].instrAddress, (Offset) linearInstructions.back().instrAddress,
		           gapCount, maxGap);
		return parsed;
	}

	std::optional<Offset> PEModule::getFunctionByName(const char* name) const {
		auto proc = (Offset) GetProcAddress(mod, name);
		boundsCheck(proc);
		return proc;
	}
	std::optional<ParsedFunction> PEModule::parseFunctionByName(const char* name, size_t maxInstructions) const {
		return parseFunctionByOffset((Offset) GetProcAddress(mod, name), maxInstructions);
	}
	std::optional<ParsedFunction> PEModule::parseFunctionByOffset(Offset offset, size_t maxInstructions) const {
		boundsCheck(offset);
		auto segment = *segmentForOffset(offset);
		return findInstructions(segment, offset, maxInstructions);
	}
#pragma endregion

#pragma region Data analysis
	static std::optional<Offset> ifOnly(std::vector<Offset> vec) {
		if (vec.size() == 0) return std::nullopt;
		if (vec.size() > 1) {
			DPRINT_LOW("    Found %d instances of string!", vec.size());
			return std::nullopt;
		}
		return vec[0];
	}

	std::vector<Offset> Segment::findString(string str) const {
		std::vector<Offset> foundList;
		auto seg_begin = (const char*) base;
		auto seg_end = (const char*) (base + length);

		while (true) {
			auto found = std::search(seg_begin, seg_end, str.begin(), str.end());
			if (found == seg_end) return foundList;
			foundList.push_back((Offset) found);
			seg_begin = found + 1;
		}
	}
	std::optional<Offset> Segment::findOnlyString(string str) const {
		return ifOnly(findString(str));
	}

	std::vector<Offset> Segment::findOffset(Offset offset) const {
		return findString(string((const char*) &offset, 4));
	}
	std::optional<Offset> Segment::findOnlyOffset(Offset offset) const {
		return ifOnly(findOffset(offset));
	}

	// Assumes there is padding between all factions (0xCC) and that functions are aligned by 0x10 bytes.
	static void findFunctionEntries(Segment seg, std::vector<Offset>& offsets, Offset offset, size_t maxInstructions) {
		seg.boundsCheck(offset);
		ASSERT_FATAL((maxInstructions & 0xF) == 0);

		offset = (Offset) ((size_t) offset & ~0xF);
		auto min = offset - maxInstructions;
		if (min < seg.base) min = seg.base;
		for (; offset >= min; offset -= 0x10) {
			if (offset != seg.base && *(offset - 1) != 0xCC) continue;
			offsets.push_back(offset);
		}
	}
	std::vector<Offset> Segment::findPotentialFunctionEntries(Offset offset, size_t maxInstructions) const {
		std::vector<Offset> offsets;
		findFunctionEntries(*this, offsets, offset, maxInstructions);
		DPRINT_LOW("  Found %d potential function entry points.", offsets.size());
		return offsets;
	}
	std::vector<Offset> Segment::findPotentialFunctionEntriesByOffset(Offset offset, size_t maxInstructions) const {
		std::vector<Offset> offsets;
		DPRINT_LOW("  Searching segment %s for function entry points containing offset 0x%p.", name.c_str(), offset);
		for (auto occurance : findOffset(offset)) 
			findFunctionEntries(*this, offsets, occurance, maxInstructions);
		DPRINT_LOW("    Found %d potential function entry points.", offsets.size());
		return offsets;
	}
	std::vector<Offset> Segment::findPotentialFunctionEntriesByString(string str, size_t maxInstructions) const {
		std::vector<Offset> offsets;
		DPRINT_LOW("  Searching segment %s for function entry points containing string with length %d.", name.c_str(), str.size());
		for (auto occurance : findString(str))
			findFunctionEntries(*this, offsets, occurance, maxInstructions);
		DPRINT_LOW("    Found %d potential function entry points.", offsets.size());
		return offsets;
	}
#pragma endregion

#pragma region Function analysis
	static bool isInInstruction(const DecodedInstruction& instr, Offset offset) {
		return instr.instrAddress >= offset && offset < (instr.instrAddress + instr.length);
	}
	bool ParsedFunction::isOffsetInFunction(Offset offset) const {
		auto bound = findInstructionsAfter(offset);
		if (bound == instructions.size()) return false;
		if (bound != 0 && isInInstruction(instructions[bound - 1], offset)) return true;
		return isInInstruction(instructions[bound], offset);
	}
	size_t ParsedFunction::findInstructionsAfter(Offset offset) const {
		auto bound = std::lower_bound(
			instructions.begin(), instructions.end(), offset,
			[](DecodedInstruction instr, Offset off) { return instr.instrAddress < off; }
		);
		if (bound == instructions.end()) return instructions.size();
		if (isInInstruction(*bound, offset)) bound++;
		return bound - instructions.begin();
	}

	std::vector<Offset> ParsedFunction::getCallOffsets(Offset after) const {
		DPRINT_LOW("  Finding call offsets starting at 0x%p.", after ? after : functionStart);
		std::vector<Offset> calls;
		auto idx = after ? findInstructionsAfter(after) : 0;
		for (auto it = instructions.begin() + idx; it != instructions.end(); it++) {
			if (it->category == ZYDIS_CATEGORY_CALL) {
				auto decoded = it->decode();
				auto target = parseBranchTarget(decoded);
				if (!target.has_value())
					DPRINT_LOW("    Could not parse call at 0x%p. Skipping.", (Offset) it->instrAddress);
				else calls.push_back(*target);
			}
		}
		return calls;
	}
#pragma endregion
}