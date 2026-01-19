// PBG6
// jwilins (and some code reversed by nmlgc)
// Extracts and repacks datfiles in the PBG6 format used in Seihou 3 (Banshiryuu) C74

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <Windows.h>
#include <sys/stat.h>
#include "stdint.h"
#include <string>
#include <vector>
#include "crc32.h"

const unsigned long CP1_SIZE = 0x102;
const unsigned long CP2_SIZE = 0x400;
unsigned long pool1[CP1_SIZE];
unsigned long pool2[CP2_SIZE];

struct PBG6Header {
	uint32_t magic;	// PBG6
	uint32_t tocOffset;
	uint32_t decompressedTOCSize;
	uint32_t decompressedTOCChecksum;
};

struct PBG6FileInfo {
	char* filename;
	uint32_t compressedSize;
	uint32_t decompressedSize;
	uint32_t offset;
	uint32_t decompressedCRCSum;
};

inline unsigned long EndianSwap(const unsigned long& x)
{
	return ((x & 0x000000ff) << 24) |
		((x & 0x0000ff00) << 8) |
		((x & 0x00ff0000) >> 8) |
		((x & 0xff000000) >> 24);
}

void InitCryptPools()
{
	for (unsigned long c = 0; c < CP1_SIZE; c++)	pool1[c] = c;
	for (unsigned long c = 0; c < CP2_SIZE; c++)	pool2[c] = 1;
}

void CryptStep(unsigned long& sym)
{
	static const unsigned long cmp = (CP1_SIZE - 1);

	pool2[sym]++;
	sym++;
	while (sym <= cmp)
	{
		pool1[sym]++;
		sym++;
	}

	if (pool1[cmp] < 0x10000)	return;

	pool1[0] = 0;

	for (unsigned short c = 0; c < cmp; c++)
	{
		pool2[c] = (pool2[c] | 2) >> 1;
		pool1[c + 1] = pool1[c] + pool2[c];
	}

	return;
}

// Range coder decompression for PBG6
char* decrypt(const char* source, const unsigned long& destsize, const unsigned long& sourcesize)
{
	unsigned long ebx = 0, ecx, edi, esi, edx;
	unsigned long cryptval[2] = { 0 };
	unsigned long s = 4, d = 0;	// source and destination bytes
	char* decompressed = new char[destsize];

	InitCryptPools();

	edi = EndianSwap(*(unsigned long*)source);
	esi = 0xFFFFFFFF;

	while (1)
	{
		edx = 0x100;

		cryptval[0] = esi / pool1[0x101];
		cryptval[1] = (edi - ebx) / cryptval[0];

		ecx = 0x80;
		esi = 0;

		while (1)
		{
			while ((ecx != 0x100) && (pool1[ecx] > cryptval[1]))
			{
				ecx--;
				edx = ecx;
				ecx = (esi + ecx) >> 1;
			}

			if (cryptval[1] < pool1[ecx + 1])	break;

			esi = ecx + 1;
			ecx = (esi + edx) >> 1;
		}

		*(decompressed + d) = (char)ecx;	// Write!
		if (++d >= destsize)	return decompressed;

		esi = (long)pool2[ecx] * (long)cryptval[0];	// IMUL

		ebx += pool1[ecx] * cryptval[0];
		CryptStep(ecx);

		ecx = (ebx + esi) ^ ebx;

		while (!(ecx & 0xFF000000))
		{
			ebx <<= 8;
			esi <<= 8;
			edi <<= 8;

			ecx = (ebx + esi) ^ ebx;

			edi += *(source + s) & 0x000000FF;
			s++;
			// if(++s >= sourcesize)	return true;
		}

		while (esi < 0x10000)
		{
			esi = 0x10000 - (ebx & 0x0000FFFF);

			ebx <<= 8;
			esi <<= 8;
			edi <<= 8;

			edi += *(source + s) & 0x000000FF;
			s++;
			// if(++s >= sourcesize)	return true;
		}
	}
}

// PBG6 compressor (carryless range coder)
// Uses two frequency tables (pool1 for cumulative, pool2 for symbol)
std::vector<char> encrypt(const char* source, const unsigned long& sourcesize)
{
	const unsigned long TOP = 0x01000000;
	const unsigned long BOT = 0x00010000;

	std::vector<char> dest;

	// Initialize model
	InitCryptPools();

	unsigned long low = 0;
	unsigned long range = 0xFFFFFFFF;

	// For the number of bytes in the file (plus one for EOF)
	for (unsigned long byteIndex = 0; byteIndex <= sourcesize; ++byteIndex) {
		// Store symbol to encode (with extra 256 for EOF)
		unsigned long sym = (byteIndex != sourcesize) ? (unsigned char)source[byteIndex] : 256;

		// Store total frequency
		unsigned long total = pool1[CP1_SIZE - 1];  // pool1[0x101]

		// Scale range by total
		unsigned long rangeByTotal = range / total;

		// Update arithmetic coding
		low += pool1[sym] * rangeByTotal;
		range = pool2[sym] * rangeByTotal;

		// Ensure high bytes of low and low + range differ
		while (1) {
			unsigned long lowRangeDiff = (low + range) ^ low;
			if (lowRangeDiff & 0xFF000000) break;

			// Emit the stable top byte
			dest.push_back((char)((low >> 24) & 0xFF));

			low <<= 8;
			range <<= 8;
		}

		// Ensure range >= BOT
		while (range < BOT) {
			range = BOT - (low & 0xFFFF);

			dest.push_back((char)((low >> 24) & 0xFF));

			low <<= 8;
			range <<= 8;
		}

		// If not EOF
		if (sym != 256) {
			// Update model
			CryptStep(sym);
		}
	}

	// Push 4 more bytes of low
	for (uint32_t iter = 0; iter < 4; ++iter) {
		dest.push_back((char)((low >> 24) & 0xFF));
		low <<= 8;
	}

	return dest;
}

// Extract a PBG6 packfile
int pbg6Extract(wchar_t inDatName[], wchar_t outFolderName[])
{
	// Open input packfile
	FILE* inDat = _wfopen(inDatName, L"rb");
	if (!inDat) {
		printf("Error opening packfile!\n");
		return -1;
	}

	// Read in packfile header and check magic
	PBG6Header curr6Header = { 0 };
	fread(&curr6Header, sizeof(PBG6Header), 1, inDat);
	if (curr6Header.magic != '6GBP') {	// PBG6
		printf("Not a valid packfile!\n");
		return -2;
	}

	// Create directory provided by user if nonexistent
	if (CreateDirectoryW(outFolderName, NULL) == 0 
		&& GetLastError() != ERROR_ALREADY_EXISTS) {
		printf("Unable to create given directory!\n");
		return -5;
	}

	// Get table of contents size, using the difference between
	// packfile size and TOC offset
	struct _stat s;
	_wstat(inDatName, &s);
	size_t compressedTOCSize = s.st_size - curr6Header.tocOffset;

	// Read in and decompress table of contents
	char* compressedTOC = new char[compressedTOCSize];
	fseek(inDat, curr6Header.tocOffset, SEEK_SET);
	fread(compressedTOC, compressedTOCSize, 1, inDat);
	char* decompressedTOC = decrypt(compressedTOC, 
		curr6Header.decompressedTOCSize, compressedTOCSize);
	delete[] compressedTOC;

	//File TOC reading loop
	uint32_t numOfFiles = *(uint32_t*)(decompressedTOC);
	PBG6FileInfo* curr6FileInfos = new PBG6FileInfo[numOfFiles]();
	uint32_t pos = sizeof(uint32_t);
	for (uint32_t fileIndex = 0; fileIndex < numOfFiles; ++fileIndex) {
		PBG6FileInfo curr6FileInfo = { 0 };
		std::string filename = (char*)(decompressedTOC + pos);
		int filenameLen = filename.length() + 1;
		curr6FileInfo.filename = new char[filenameLen];
		memcpy(curr6FileInfo.filename, filename.c_str(), filenameLen);
		pos += filenameLen;
		curr6FileInfo.compressedSize = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr6FileInfo.decompressedSize = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr6FileInfo.offset = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr6FileInfo.decompressedCRCSum = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr6FileInfos[fileIndex] = curr6FileInfo;
	}
	delete[] decompressedTOC;

	// File extraction loop
	for (uint32_t fileIndex = 0; fileIndex < numOfFiles; ++fileIndex) {
		int byteCount = MultiByteToWideChar(932, 0, curr6FileInfos[fileIndex].filename, -1, NULL, 0);
		wchar_t* wideFilename = new wchar_t[byteCount];
		// Store filename as wide char with proper Shift-JIS encoding
		MultiByteToWideChar(932, 0, curr6FileInfos[fileIndex].filename, 
			strlen(curr6FileInfos[fileIndex].filename) + 1, wideFilename, byteCount);

		printf("Unpacking %s...\n", curr6FileInfos[fileIndex].filename);

		// Read in compressed file data
		char* currFileData = new char[curr6FileInfos[fileIndex].compressedSize];
		fseek(inDat, curr6FileInfos[fileIndex].offset, SEEK_SET);
		fread(currFileData, curr6FileInfos[fileIndex].compressedSize, 1, inDat);

		// Set output path and write decompressed file
		wchar_t outPath[MAX_PATH];
		swprintf(outPath, L"%ls\\%ls", outFolderName, wideFilename);
		delete[] wideFilename;
		FILE* outFile = _wfopen(outPath, L"wb");
		if (!outFile) {
			printf("Failed to open output file!\n");
			return -8;
		}
		char* decompressedFile = decrypt(currFileData, curr6FileInfos[fileIndex].decompressedSize,
			curr6FileInfos[fileIndex].compressedSize);
		fwrite(decompressedFile, curr6FileInfos[fileIndex].decompressedSize, 1, outFile);
		delete[] currFileData;
		delete[] decompressedFile;
		fclose(outFile);
	}

	fclose(inDat);
	printf("Files successfully extracted!\n");
	return 0;
}

// Pack a PBG6 packfile
int pbg6Pack(wchar_t inFolderName[], wchar_t outDatName[])
{
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	// Form path to search for files in
	wchar_t searchPath[MAX_PATH];
	swprintf(searchPath, L"%ls\\*", inFolderName);

	// Find first file in given path
	hFind = FindFirstFileW(searchPath, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("Given folder not found...\n");
		return -3;
	}

	// Open output packfile
	FILE* outDat = _wfopen(outDatName, L"wb");
	if (!outDat) {
		printf("Failed to open output packfile!\n");
		return -9;
	}

	// Write temporary PBG6 header
	PBG6Header curr6Header = { 0 };
	fwrite(&curr6Header, sizeof(PBG6Header), 1, outDat);

	// Count valid files in given directory
	uint32_t numOfFiles = 0;
	do {
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			++numOfFiles;
		}
	} while (FindNextFileW(hFind, &ffd));

	// Pack all valid files in given directory
	PBG6FileInfo* curr6FileInfos = new PBG6FileInfo[numOfFiles]();
	hFind = FindFirstFileW(searchPath, &ffd);
	uint32_t fileIndex = 0;
	struct _stat s;
	do {
		// Make sure this file is not hidden, a directory, or ./..
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Get path of file to open and open it
			wchar_t filepath[MAX_PATH];
			swprintf(filepath, L"%ls\\%ls", inFolderName, ffd.cFileName);
			FILE* inFile = _wfopen(filepath, L"rb");
			if (!inFile) {
				printf("Error opening file...\n");
				return -4;
			}

			// Collect info for file entry
			PBG6FileInfo curr6FileInfo = { 0 };
			// Begin filename with '/' (PBG6 quirk)
			wchar_t slashFilename[MAX_PATH];
			swprintf(slashFilename, L"/%ls", ffd.cFileName);
			int byteCount = WideCharToMultiByte(932, 0, slashFilename, -1, NULL, 
				0, NULL, NULL);
			curr6FileInfo.filename = new char[byteCount];
			// Store wide filename as char with proper Shift-JIS encoding
			WideCharToMultiByte(932, 0, slashFilename, -1, curr6FileInfo.filename, 
				byteCount, NULL, NULL);

			printf("Packing %s...\n", curr6FileInfo.filename);

			curr6FileInfo.offset = ftell(outDat);
			_wstat(filepath, &s);
			curr6FileInfo.decompressedSize = s.st_size;

			// Write compressed file to packfile
			char* currFileData = new char[curr6FileInfo.decompressedSize];
			fread(currFileData, curr6FileInfo.decompressedSize, 1, inFile);
			std::vector<char> compressedFileData = encrypt(currFileData, 
				curr6FileInfo.decompressedSize);
			curr6FileInfo.compressedSize = compressedFileData.size();
			fwrite(&compressedFileData[0], curr6FileInfo.compressedSize, 1, outDat);
			
			// Calculate CRC32 checksum of decompressed file
			uint32_t table[256];
			crc32::generate_table(table);
			curr6FileInfo.decompressedCRCSum = crc32::update(table, 0, currFileData, 
				curr6FileInfo.decompressedSize);
			delete[] currFileData;

			curr6FileInfos[fileIndex] = curr6FileInfo;
			++fileIndex;
		}
	} while (FindNextFileW(hFind, &ffd) && fileIndex < numOfFiles);

	// Count size of all table of contents filenames
	int strlenTotal = 0;
	for (fileIndex = 0; fileIndex < numOfFiles; ++fileIndex) {
		strlenTotal += (strlen(curr6FileInfos[fileIndex].filename) + 1);
	}

	// Get info for packfile header
	curr6Header.tocOffset = ftell(outDat);
	curr6Header.decompressedTOCSize = sizeof(numOfFiles) + strlenTotal + 
		(numOfFiles * (sizeof(PBG6FileInfo) - sizeof(char*)));

	// Load all file info into a table of contents buffer
	char* toCompress = new char[curr6Header.decompressedTOCSize];
	// Need to do this to get rid of buffer overrun warning...
	if (curr6Header.decompressedTOCSize >= sizeof(numOfFiles)) {
		memcpy(toCompress, &numOfFiles, sizeof(numOfFiles));
	}
	uint32_t pos = sizeof(uint32_t);
	for (fileIndex = 0; fileIndex < numOfFiles; ++fileIndex) {
		memcpy(toCompress + pos, curr6FileInfos[fileIndex].filename, 
			strlen(curr6FileInfos[fileIndex].filename) + 1);
		pos += (strlen(curr6FileInfos[fileIndex].filename) + 1);
		memcpy(toCompress + pos, &curr6FileInfos[fileIndex].compressedSize, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr6FileInfos[fileIndex].decompressedSize, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr6FileInfos[fileIndex].offset, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr6FileInfos[fileIndex].decompressedCRCSum, sizeof(uint32_t));
		pos += sizeof(uint32_t);
	}

	// Calculate CRC32 checksum of decompressed table of contents
	uint32_t table[256];
	crc32::generate_table(table);
	curr6Header.decompressedTOCChecksum = crc32::update(table, 0, toCompress, 
		curr6Header.decompressedTOCSize);
	std::vector<char> compressedTOC = encrypt(toCompress, curr6Header.decompressedTOCSize);
	delete[] toCompress;
	fwrite(&compressedTOC[0], compressedTOC.size(), 1, outDat);

	// Rewrite proper header
	fseek(outDat, 0, SEEK_SET);
	curr6Header.magic = '6GBP';
	fwrite(&curr6Header, sizeof(PBG6Header), 1, outDat);
	fclose(outDat);

	printf("Files successfully packed!\n");
	return 0;
}