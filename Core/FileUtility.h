#pragma once

#include "pch.h"
#include <vector>
#include <string>
#include <ppl.h>

namespace Utility
{
	using namespace std;
	using namespace concurrency;

	typedef unsigned char byte;
	typedef shared_ptr<vector<byte>> ByteArray;
	extern ByteArray NullFile;

	//? can't we change to path and get rid of shared_ptr?

	// Reads the entire contents of a binary file.  If the file with the same name except with an additional
	// ".gz" suffix exists, it will be loaded and decompressed instead.
	// This operation blocks until the entire file is read.
	ByteArray ReadFileSync(const wstring& fileName);

	// Same as previous except that it does not block but instead returns a task
	task<ByteArray> ReadFileAsync(const wstring& fileName);
}