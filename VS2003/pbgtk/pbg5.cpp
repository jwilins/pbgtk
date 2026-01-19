// PBG5
// jwilins (and some code/algorithm from ExpHP)
// Extracts and repacks datfiles in the PBG5 format used in Seihou 3 (Banshiryuu) C67 and Samidare

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <Windows.h>
#include "stdint.h"
#include <string>
#include "lzss.h"
#include "crc32.h"

struct PBG5Header {
	uint32_t magic;	// PBG5
	uint32_t numOfFiles;
	uint32_t tocOffset;
	uint32_t decompressedTOCSize;
};

struct PBG5FileInfo {
	char* filename;
	uint32_t offset;
	uint32_t uncompressedSize;
	uint32_t decompressedCRCSum;
};

// Extract a PBG5 packfile
int pbg5Extract(wchar_t inDatName[], wchar_t outFolderName[])
{
	// Open input packfile
	FILE* inDat = _wfopen(inDatName, L"rb");
	if (!inDat) {
		printf("Error opening packfile!\n");
		return -1;
	}

	// Read in PBG5 packfile header and check magic
	PBG5Header curr5Header = { 0 };
	fread(&curr5Header, sizeof(PBG5Header), 1, inDat);
	if (curr5Header.magic != '5GBP') {	// PBG5
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
	size_t compressedTOCSize = s.st_size - curr5Header.tocOffset;

	// Read in and decompress table of contents
	uint8_t* compressedTOC = new uint8_t[compressedTOCSize];
	fseek(inDat, curr5Header.tocOffset, SEEK_SET);
	fread(compressedTOC, compressedTOCSize, 1, inDat);
	uint8_t* decompressedTOC = decompress(compressedTOC, curr5Header.decompressedTOCSize,
		compressedTOCSize, 15);
	delete[] compressedTOC;

	// Table of contents reading loop
	PBG5FileInfo* curr5FileInfos = new PBG5FileInfo[curr5Header.numOfFiles];
	uint32_t pos = 0;
	for (uint32_t fileIndex = 0; fileIndex < curr5Header.numOfFiles; ++fileIndex)
	{
		PBG5FileInfo curr5FileInfo = { 0 };
		std::string filename = (char*)(decompressedTOC + pos);
		int filenameLen = filename.length() + 1;
		curr5FileInfo.filename = new char[filenameLen];
		memcpy(curr5FileInfo.filename, filename.c_str(), filenameLen);
		pos += filenameLen;
		curr5FileInfo.offset = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr5FileInfo.uncompressedSize = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr5FileInfo.decompressedCRCSum = *(uint32_t*)(decompressedTOC + pos);
		pos += sizeof(uint32_t);
		curr5FileInfos[fileIndex] = curr5FileInfo;
	}
	delete[] decompressedTOC;

	// File extraction loop
	for (uint32_t fileIndex = 0; fileIndex < curr5Header.numOfFiles; ++fileIndex) {
		int byteCount = MultiByteToWideChar(932, 0, curr5FileInfos[fileIndex].filename, -1, NULL, 0);
		wchar_t* wideFilename = new wchar_t[byteCount];
		// Store filename as wide char with proper Shift-JIS encoding
		MultiByteToWideChar(932, 0, curr5FileInfos[fileIndex].filename,
			strlen(curr5FileInfos[fileIndex].filename) + 1, wideFilename, byteCount);

		printf("Unpacking %s...\n", curr5FileInfos[fileIndex].filename);

		// Calculate compressed file size from the difference between the next file's offset
		// and this file's offset... or the difference between the table of contents size and
		// this file's offset if this is the last file
		size_t compressedSize = 0;
		if (fileIndex + 1 != curr5Header.numOfFiles) {
			compressedSize = curr5FileInfos[fileIndex + 1].offset -
				curr5FileInfos[fileIndex].offset;
		}
		else {
			compressedSize = curr5Header.tocOffset -
				curr5FileInfos[fileIndex].offset;
		}

		// Read in compressed fle data
		uint8_t* currFileData = new uint8_t[compressedSize];
		fseek(inDat, curr5FileInfos[fileIndex].offset, SEEK_SET);
		fread(currFileData, compressedSize, 1, inDat);

		//Set output path and write decompressed file
		wchar_t outPath[MAX_PATH];
		swprintf(outPath, L"%ls\\%ls", outFolderName, wideFilename);
		delete[] wideFilename;
		FILE* outFile = _wfopen(outPath, L"wb");
		if (!outFile) {
			printf("Failed to open output file!\n");
			return -8;
		}
		uint8_t* decompressedFileData = decompress(currFileData, 
			curr5FileInfos[fileIndex].uncompressedSize, compressedSize, 15);
		delete[] currFileData;
		fwrite(decompressedFileData, curr5FileInfos[fileIndex].uncompressedSize, 1, outFile);
		delete[] decompressedFileData;
		fclose(outFile);
	}
	
	fclose(inDat);
	printf("Files successfully extracted!\n");
	return 0;
}

// Pack a PBG5 packfile
int pbg5Pack(wchar_t inFolderName[], wchar_t outDatName[])
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

	// Write temporary PBG5 header
	PBG5Header curr5Header = { 0 };
	fwrite(&curr5Header, sizeof(PBG5Header), 1, outDat);

	// Count valid files to pack
	do {
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			++curr5Header.numOfFiles;
		}
	} while (FindNextFileW(hFind, &ffd));

	PBG5FileInfo* curr5FileInfos = new PBG5FileInfo[curr5Header.numOfFiles]();
	hFind = FindFirstFileW(searchPath, &ffd);

	uint32_t fileIndex = 0;
	struct _stat s;
	// File packing loop
	do {
		// Make sure this file is not hidden, a directory, or ./..
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Get file path and open input file
			wchar_t filepath[MAX_PATH];
			swprintf(filepath, L"%ls\\%ls", inFolderName, ffd.cFileName);
			FILE* inFile = _wfopen(filepath, L"rb");
			if (!inFile) {
				printf("Error opening file...\n");
				return -4;
			}

			// Collect info for current file
			PBG5FileInfo curr5FileInfo = { 0 };
			int byteCount = WideCharToMultiByte(932, 0, ffd.cFileName, -1, 
				NULL, 0, NULL, NULL);
			curr5FileInfo.filename = new char[byteCount];
			// Store wide filename as char with proper Shift-JIS encoding
			WideCharToMultiByte(932, 0, ffd.cFileName, -1, curr5FileInfo.filename,
				byteCount, NULL, NULL);

			printf("Packing %s...\n", curr5FileInfo.filename);

			curr5FileInfo.offset = ftell(outDat);
			_wstat(filepath, &s);
			curr5FileInfo.uncompressedSize = s.st_size;

			// Compress and write file to packfile
			uint8_t* currFileData = new uint8_t[curr5FileInfo.uncompressedSize];
			fread(currFileData, curr5FileInfo.uncompressedSize, 1, inFile);
			std::vector<uint8_t> compressedData = compress(currFileData, 
				curr5FileInfo.uncompressedSize, 15);
			fwrite(&compressedData[0], compressedData.size(), 1, outDat);

			// Calculate CRC32 of uncompressed file
			uint32_t table[256];
			crc32::generate_table(table);
			curr5FileInfo.decompressedCRCSum = crc32::update(table, 0, currFileData, 
				curr5FileInfo.uncompressedSize);
			delete[] currFileData;

			curr5FileInfos[fileIndex] = curr5FileInfo;
			++fileIndex;
		}
	} while (FindNextFileW(hFind, &ffd) && fileIndex < curr5Header.numOfFiles);

	// Get total size of strings in table of contents
	int strlenTotal = 0;
	for (fileIndex = 0; fileIndex < curr5Header.numOfFiles; ++fileIndex) {
		strlenTotal += (strlen(curr5FileInfos[fileIndex].filename) + 1);
	}

	curr5Header.tocOffset = ftell(outDat);
	curr5Header.decompressedTOCSize = strlenTotal + (curr5Header.numOfFiles * 
		(sizeof(PBG5FileInfo) - sizeof(char*)));
	uint8_t* toCompress = new uint8_t[curr5Header.decompressedTOCSize];
	uint32_t pos = 0;
	// Create buffer for table of contents
	for (fileIndex = 0; fileIndex < curr5Header.numOfFiles; ++fileIndex) {
		memcpy(toCompress + pos, curr5FileInfos[fileIndex].filename, 
			strlen(curr5FileInfos[fileIndex].filename) + 1);
		pos += (strlen(curr5FileInfos[fileIndex].filename) + 1);
		memcpy(toCompress + pos, &curr5FileInfos[fileIndex].offset, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr5FileInfos[fileIndex].uncompressedSize, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		memcpy(toCompress + pos, &curr5FileInfos[fileIndex].decompressedCRCSum, sizeof(uint32_t));
		pos += sizeof(uint32_t);
	}

	// Compress and write table of contents buffer to packfile
	std::vector<uint8_t> compressedTOC = compress(toCompress, curr5Header.decompressedTOCSize, 15);
	fwrite(&compressedTOC[0], compressedTOC.size(), 1, outDat);
	delete[] toCompress;

	//Rewrite proper header
	curr5Header.magic = '5GBP';
	fseek(outDat, 0, SEEK_SET);
	fwrite(&curr5Header, sizeof(PBG5Header), 1, outDat);
	fclose(outDat);

	printf("Files successfully packed!\n");
	return 0;
}