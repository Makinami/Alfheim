#include "pch.h"
#include "FileUtility.h"
#include <fstream>
#include <zlib.h>

using namespace std;
using namespace Utility;

namespace Utility
{
	ByteArray NullFile = make_shared<ByteArray::element_type>(vector<byte>());
}

ByteArray DecompressZippedFile(wstring& fileName);

ByteArray ReadFileHelper(const wstring& fileName)
{
	struct _stat64 fileStat;
	int fileExists = _wstat64(fileName.c_str(), &fileStat);
	if (fileExists == -1)
		return NullFile;

	ifstream file(fileName, ios::in | ios::binary);
	if (!file)
		return NullFile;

	auto byteArray = make_shared<ByteArray::element_type>(file.seekg(0, ios::end).tellg());
	file.seekg(0, ios::beg).read((char*)byteArray->data(), byteArray->size());
	file.close();

	ASSERT(byteArray->size() == (size_t)fileStat.st_size);

	return byteArray;
}

ByteArray ReadFileHelperEx(shared_ptr<wstring> fileName)
{
	std::wstring zippedFileName = *fileName + L".gz";
	ByteArray firstTry = DecompressZippedFile(zippedFileName);
	if (firstTry != NullFile)
		return firstTry;

	return ReadFileHelper(*fileName);
}

ByteArray Inflate(ByteArray CompressedSource, int& err, uint32_t ChunkSize = 0x100000)
{
	// Create a dynamic buffer to hold compressed blocks
	vector<unique_ptr<Utility::byte>> blocks;

	z_stream strm = {};
	strm.data_type = Z_BINARY;
	strm.total_in = strm.avail_in = (uInt)CompressedSource->size();
	strm.next_in = CompressedSource->data();

	err = inflateInit2(&strm, (15 + 32)); // 15 window bits, and the +32 tells zlib to detect if using gzip or zlib

	while (err == Z_OK || err == Z_BUF_ERROR)
	{
		strm.avail_out = ChunkSize;
		strm.next_out = (uint8_t*)malloc(ChunkSize);
		blocks.emplace_back(strm.next_out);
		err = inflate(&strm, Z_NO_FLUSH);
	}

	if (err != Z_STREAM_END)
	{
		inflateEnd(&strm);
		return NullFile;
	}

	ASSERT(strm.total_out > 0, "Nothing to decompress");

	auto byteArray = make_shared<ByteArray::element_type>(strm.total_out);

	void* curDest = byteArray->data();
	size_t remaining = byteArray->size();

	for (auto& block : blocks)
	{
		ASSERT(remaining > 0);

		size_t CopySize = min(remaining, (size_t)ChunkSize);

		memcpy(curDest, block.get(), CopySize);
		curDest = (uint8_t*)curDest + CopySize;
		remaining -= CopySize;
	}

	inflateEnd(&strm);

	return byteArray;
}

ByteArray DecompressZippedFile(wstring& fileName)
{
	ByteArray CompressedFile = ReadFileHelper(fileName);
	if (CompressedFile == NullFile)
		return NullFile;

	int error;
	auto DecompressedFile = Inflate(CompressedFile, error);
	if (DecompressedFile->size() == 0)
	{
		Utility::Printf(L"Couldn't unpiz file {}: Error = {}\n", fileName.c_str(), error);
		return NullFile;
	}

	return DecompressedFile;
}

ByteArray Utility::ReadFileSync(const wstring& fileName)
{
	return ReadFileHelperEx(make_shared<wstring>(fileName));
}

task<ByteArray> Utility::ReadFileAsync(const wstring& fileName)
{
	auto SharedPtr = make_shared<wstring>(fileName);
	return create_task([=] { return ReadFileHelperEx(SharedPtr); });
}
