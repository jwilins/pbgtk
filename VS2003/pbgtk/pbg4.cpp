// PBG4
// jwilins
// Unpacks and repacks PBG4 files from early Seihou 3 (pre-C67) builds

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <Windows.h>
#include "stdint.h"
#include <string>
#include "lzss.h"

struct PBG4Header {
	uint32_t magic;	// PBG4
	uint32_t numOfFiles;
	uint32_t tocOffset;
	uint32_t decompressedTOCSize;
};

struct PBG4FileInfo {
	char* filename;
	uint32_t offset;
	uint32_t uncompressedSize;
	uint32_t zeros;
};

// Extract a PBG4 packfile
int pbg4Extract(wchar_t inDatName[], wchar_t outFolderName[])
{
	// Open input packfile
	FILE* inDat = _wfopen(inDatName, L"rb");
	if (!inDat) {
		printf("Error opening packfile!\n");
		return -1;
	}

	// Read in packfile header and check magic
	PBG4Header curr4Header = { 0 };
	fread(&curr4Header, sizeof(PBG4Header), 1, inDat);
	if (curr4Header.magic != '4GBP') {	// PBG4
		printf("Not a valid packfile!\n");
		return -2;
	}

	// Create directory provided by user if nonexistent
	if (CreateDirectoryW(outFolderName, NULL) == 0 
		&& GetLastError() != ERROR_ALREADY_EXISTS) {
		printf("Unable to create given directory!\n");
		return -5;
	}

	// Get compressed table of contents size
	struct _stat s;
	_wstat(inDatName, &s);
	size_t compressedTOCSize = s.st_size - curr4Header.tocOffset;
	
	// Read in and decompress table of contents
	uint8_t* compressedTOC = new uint8_t[compressedTOCSize];
	fseek(inDat, curr4Header.tocOffset, SEEK_SET);
	fread(compressedTOC, compressedTOCSize, 1, inDat);
	uint8_t* decompressedTOC = decompress(compressedTOC, curr4Header.decompressedTOCSize,
		compressedTOCSize, 13);
	delete[] compressedTOC;

	// File TOC reading loop
	PBG4FileInfo* curr4FileInfos = new PBG4FileInfo[curr4Header.numOfFiles]();
	uint32_t pos = 0;
	for (uint32_t fileIndex = 0; fileIndex < curr4Header.numOfFiles; ++fileIndex) {
		PBG4FileInfo curr4FileInfo = { 0 };
		std::string filename = (char*)(decompressedTOC + pos);
		int filenameLen = filename.length() + 1;
		curr4FileInfo.filename = new char[filenameLen];
		memcpy(curr4FileInfo.filename, filename.c_str(), filenameLen);
		pos += filenameLen;
		curr4FileInfo.offset = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr4FileInfo.uncompressedSize = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr4FileInfo.zeros = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr4FileInfos[fileIndex] = curr4FileInfo;
	}
	delete[] decompressedTOC;

	// File extraction loop
	for (uint32_t fileIndex = 0; fileIndex < curr4Header.numOfFiles; ++fileIndex) {
		int byteCount = MultiByteToWideChar(932, 0, curr4FileInfos[fileIndex].filename, 
			-1, NULL, 0);
		wchar_t* wideFilename = new wchar_t[byteCount];
		// Store filename as wide char with proper Shift-JIS encoding
		MultiByteToWideChar(932, 0, curr4FileInfos[fileIndex].filename,
			strlen(curr4FileInfos[fileIndex].filename) + 1, wideFilename, byteCount);

		printf("Unpacking %s...\n", curr4FileInfos[fileIndex].filename);
		delete[] curr4FileInfos[fileIndex].filename;

		// Calculate compressed file size from the difference between the next file's offset
		// and this file's offset... or the difference between the table of contents size and
		// this file's offset if this is the last file
		size_t compressedSize = 0;
		if (fileIndex + 1 != curr4Header.numOfFiles) {
			compressedSize = curr4FileInfos[fileIndex + 1].offset -
				curr4FileInfos[fileIndex].offset;
		}
		else {
			compressedSize = curr4Header.tocOffset -
				curr4FileInfos[fileIndex].offset;
		}

		// Read in compressed file data
		uint8_t* currFileData = new uint8_t[compressedSize];
		fseek(inDat, curr4FileInfos[fileIndex].offset, SEEK_SET);
		fread(currFileData, compressedSize, 1, inDat);

		// Set output path and write decompressed file
		wchar_t outPath[MAX_PATH];
		swprintf(outPath, L"%ls\\%ls", outFolderName, wideFilename);
		delete[] wideFilename;
		FILE* outFile = _wfopen(outPath, L"wb");
		if (!outFile) {
			printf("Failed to open output file!\n");
			return -8;
		}
		uint8_t* decompressedFile = decompress(currFileData, 
			curr4FileInfos[fileIndex].uncompressedSize, compressedSize, 13);
		fwrite(decompressedFile, curr4FileInfos[fileIndex].uncompressedSize, 1, outFile);
		fclose(outFile);
		delete[] currFileData;
		delete[] decompressedFile;
	}

	fclose(inDat);
	printf("Files successfully extracted!\n");
	return 0;
}

// Pack a PBG4 packfile
int pbg4Pack(wchar_t inFolderName[], wchar_t outDatName[])
{
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	// Form path to search for files
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
	// Write temporary PBG4 header
	PBG4Header curr4Header = { 0 };
	fwrite(&curr4Header, sizeof(PBG4Header), 1, outDat);

	// Count valid files in directory for packing
	do {
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			++curr4Header.numOfFiles;
		}
	} while (FindNextFileW(hFind, &ffd));

	PBG4FileInfo* curr4FileInfos = new PBG4FileInfo[curr4Header.numOfFiles]();
	hFind = FindFirstFileW(searchPath, &ffd);

	uint32_t fileIndex = 0;
	struct _stat s;
	// File packing loop
	do {
		// Make sure this file isn't hidden, a directory, or ./..
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Get input file path and open it for reading
			wchar_t filepath[MAX_PATH];
			swprintf(filepath, L"%ls\\%ls", inFolderName, ffd.cFileName);
			FILE* inFile = _wfopen(filepath, L"rb");
			if (!inFile) {
				printf("Error opening file...\n");
				return -4;
			}

			// Get info for current file
			PBG4FileInfo curr4FileInfo = { 0 };
			_wstat(filepath, &s);
			int byteCount = WideCharToMultiByte(932, 0, ffd.cFileName, -1, NULL, 0, NULL, NULL);
			curr4FileInfo.filename = new char[byteCount];
			// Store wide filename as char with proper Shift-JIS encoding
			WideCharToMultiByte(932, 0, ffd.cFileName, -1, curr4FileInfo.filename,
				byteCount, NULL, NULL);

			printf("Packing %s...\n", curr4FileInfo.filename);

			curr4FileInfo.offset = ftell(outDat);
			curr4FileInfo.uncompressedSize = s.st_size;
			curr4FileInfos[fileIndex] = curr4FileInfo;

			// Read in and compress file data
			uint8_t* currFileData = new uint8_t[curr4FileInfo.uncompressedSize]();
			fread(currFileData, curr4FileInfo.uncompressedSize, 1, inFile);
			std::vector<uint8_t> compressedData = compress(currFileData, 
				curr4FileInfo.uncompressedSize, 13);
			// Write compressed data to packfile
			fwrite(&compressedData[0], compressedData.size(), 1, outDat);
			delete[] currFileData;
			++fileIndex;
		}
	} while (FindNextFileW(hFind, &ffd) && fileIndex < curr4Header.numOfFiles);

	// Get total size of all strings in the table of contents
	int strlenTotal = 0;
	for (fileIndex = 0; fileIndex < curr4Header.numOfFiles; ++fileIndex) {
		strlenTotal += (strlen(curr4FileInfos[fileIndex].filename) + 1);
	}

	// Collect info for packfile header
	curr4Header.tocOffset = ftell(outDat);
	curr4Header.decompressedTOCSize = strlenTotal + (curr4Header.numOfFiles * 
		(sizeof(PBG4FileInfo) - sizeof(char*)));
	uint8_t* toCompress = new uint8_t[curr4Header.decompressedTOCSize];
	uint32_t pos = 0;
	// Form table of contents buffer
	for (fileIndex = 0; fileIndex < curr4Header.numOfFiles; ++fileIndex) {
		memcpy(toCompress + pos, curr4FileInfos[fileIndex].filename, 
			strlen(curr4FileInfos[fileIndex].filename) + 1);
		pos += (strlen(curr4FileInfos[fileIndex].filename) + 1);
		memcpy(toCompress + pos, &curr4FileInfos[fileIndex].offset, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr4FileInfos[fileIndex].uncompressedSize, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr4FileInfos[fileIndex].zeros, sizeof(uint32_t));
		pos += sizeof(uint32_t);
	}

	// Compress and write table of contents
	std::vector<uint8_t> compressedData = compress(toCompress, curr4Header.decompressedTOCSize, 13);
	fwrite(&compressedData[0], compressedData.size(), 1, outDat);

	// Rewrite proper header
	fseek(outDat, 0, SEEK_SET);
	curr4Header.magic = '4GBP';
	fwrite(&curr4Header, sizeof(PBG4Header), 1, outDat);
	fclose(outDat);

	printf("Files successfully packed!\n");
	return 0;
}