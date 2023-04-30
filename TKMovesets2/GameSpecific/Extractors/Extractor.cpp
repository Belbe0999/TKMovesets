#include <format>
#include <fstream>
#include <cstdlib>
#include <windows.h>
#include <filesystem>
#include <lz4.h>

#include "Helpers.hpp"
#include "Extractor.hpp"
#include "Animations.hpp"

#include "MovesetStructs.h"
#include "GameTypes.h"

using namespace ByteswapHelpers;

// -- Helpers -- //

namespace ExtractorUtils
{

	void WriteFileData(std::ofstream &file, const std::vector<std::pair<Byte*, uint64_t>>& blocks, uint8_t&progress, uint8_t progress_max)
	{
		uint8_t remainingProgress = progress_max - progress;
		uint8_t step = (uint8_t)((size_t)remainingProgress / blocks.size());

		for (const std::pair<Byte*, uint64_t>& block : blocks) {
			char* blockData = (char*)block.first;
			uint64_t blockSize = block.second;

			file.write(blockData, blockSize);
			Helpers::align8Bytes(file);

			progress += step;
		}
	}

	bool CompressFile(int32_t moveset_data_start, const std::wstring& dest_filename, const std::wstring& src_filename)
	{
		std::ifstream orig_file(src_filename, std::ios::binary);
		std::ofstream new_file(dest_filename, std::ios::binary);

		// Copy up to moveset_data_start, get size of moveset data
		int32_t moveset_data_size;
		{
			// Read up to moveset data start
			char* buf = new char[moveset_data_start];
			orig_file.read(buf, moveset_data_start);

			// Calculate moveset data size
			orig_file.seekg(0, std::ios::end);
			moveset_data_size = (int32_t)orig_file.tellg() - moveset_data_start;

			// Mark moveset as compressed
			((TKMovesetHeader*)buf)->moveset_data_size = moveset_data_size;

			// Write header
			new_file.write(buf, moveset_data_start);
			delete[] buf;

			// Move cursor back to the start of the moveset data
			orig_file.seekg(moveset_data_start, std::ios::beg);
		}

		// Compress moveset data
		char* inbuf = new char[moveset_data_size];
		char* outbuf = new char[moveset_data_size];
		int32_t compressed_size;

		orig_file.read(inbuf, moveset_data_size);
		orig_file.close();

		std::filesystem::remove(src_filename);

		compressed_size = LZ4_compress_default(inbuf, outbuf, moveset_data_size, moveset_data_size);
		DEBUG_LOG("Compression: Old size was %d, compressed size is %d\n", moveset_data_size, compressed_size);

		if (compressed_size <= 0)
		{
			new_file.close();
			std::filesystem::remove(dest_filename);
			DEBUG_LOG("Compression failure\n");
		}
		else {
			new_file.write(outbuf, compressed_size);
		}

		delete[] inbuf;
		delete[] outbuf;
		return compressed_size >= 0;
	}
};

// Private methods //

Byte* Extractor::allocateAndReadBlock(gameAddr blockStart, gameAddr blockEnd, uint64_t& size_out)
{
	size_t blockSize = blockEnd - blockStart;

	if (blockSize >= (MOVESET_MAX_BLOCK_SIZE_MB * 1000000)) { // 50 MB
		DEBUG_LOG("Block size too big to be valid, returning nullptr\n");
		// Arbitrary max block size in case something bad happened and we get a very invalid start & end
		return nullptr;
	}

	Byte* block = (Byte*)malloc(blockSize);
	if (block == nullptr) {
		size_out = 0;
		return nullptr;
	}

	m_process->readBytes(blockStart, block, blockSize);
	size_out = blockSize;

	return block;
}

std::wstring Extractor::GenerateFilename(const char* characterName, const char* gameIdentifierString, const char* extension, unsigned int suffixId)
{
	if (suffixId <= 1) {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ Helpers::string_to_wstring(extension);
		return outputFilename;
	}
	else {
		std::wstring outputFilename = Helpers::string_to_wstring(cm_extractionDir) + L"/" +
			Helpers::string_to_wstring(gameIdentifierString) + Helpers::string_to_wstring(characterName)
			+ L" (" + std::to_wstring(suffixId) + L")" + Helpers::string_to_wstring(extension);
		return outputFilename;
	}
}

void Extractor::GetFilepath(const char* characterName, std::wstring& out, std::wstring& out_tmp, bool overwriteSameFilename)
{
	CreateDirectory(cm_extractionDir, NULL);
	// Make sure the directory for extraction exists 

	const char* gameIdentifierstring = m_gameInfo.movesetNamePrefix;

	std::wstring filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, 0);

	if (!overwriteSameFilename && Helpers::fileExists(filePath.c_str())) {
		// File exists: try to add numbered suffix, loop until we find a free one
		unsigned int counter = 2;
		filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);

		while (Helpers::fileExists(filePath.c_str())) {
			filePath = GenerateFilename(characterName, gameIdentifierstring, MOVESET_FILENAME_EXTENSION, counter);
			counter++;
		}

		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION, counter);
	}
	else {
		// File doesnt exist or we are allowed to overwrite, no need to suffix
		out = filePath;
		out_tmp = GenerateFilename(characterName, gameIdentifierstring, MOVESET_TMPFILENAME_EXTENSION);
	}
}

uint64_t Extractor::GetAnimationSize(gameAddr anim)
{
	uint16_t animType = m_process->readInt16(anim);
	if ((animType & 0xFF) == 0) {
		animType = BYTESWAP_INT16(animType);
	}

	if (animType == 0xC8) {
		return TAnimUtils::FromProcess::getC8AnimSize(m_process, anim);
	}
	else if (animType == 0x64) {
		return TAnimUtils::FromProcess::get64AnimSize(m_process, anim);
	}
	// Invalid animation type

	DEBUG_LOG("Animation address %llx does not have a valid animation type. First four bytes: %x\n", anim, m_process->readInt32(anim));

	throw;
	return 0;
}