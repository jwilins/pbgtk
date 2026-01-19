// PBG1A
// jwilins (and some code/algorithm from nmlgc/ReC98)
// Extracts and repacks datfiles in the PBG1A format used in Seihou 1 (Shuusou Gyoku)

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <vector>
#include <Windows.h>
#include <sys/stat.h>
#include <string>
#include "stdint.h"
#include "lzss.h"

struct PBG1AHeader {
	uint32_t magic;	// PBG\x1A
	uint32_t checksum;
	uint32_t numOfFiles;
};

struct PBG1AFileInfo {
	uint32_t uncompressedSize;
	uint32_t offset;
	uint32_t compressedChecksum;
};

// Extract a PBG1A packfile
int pbg1AExtract(wchar_t inDatName[], wchar_t outFolderName[], std::wstring renameType)
{
	// Open input packfile
	FILE* inDat = _wfopen(inDatName, L"rb");
	if (!inDat) {
		printf("Error opening packfile!\n");
		return -1;
	}

	// Read in packfile header
	PBG1AHeader curr1AHeader = { 0 };
	fread(&curr1AHeader, sizeof(PBG1AHeader), 1, inDat);

	if (curr1AHeader.magic != '\x1AGBP') {	// PBG1A
		printf("Not a valid packfile!\n");
		return -2;
	}

	// Create directory provided by user if nonexistent
	if (CreateDirectoryW(outFolderName, NULL) == 0 && 
		GetLastError() != ERROR_ALREADY_EXISTS) {
		printf("Unable to create given directory!\n");
		return -5;
	}

	// Read in file infos
	PBG1AFileInfo* curr1AFileInfos = new PBG1AFileInfo[curr1AHeader.numOfFiles];
	for (uint32_t fileIndex = 0; fileIndex < curr1AHeader.numOfFiles; ++fileIndex) {
		fread(&curr1AFileInfos[fileIndex], sizeof(PBG1AFileInfo), 1, inDat);
	}

	// File extraction loop
	struct _stat s;
	for (uint32_t fileIndex = 0; fileIndex < curr1AHeader.numOfFiles; ++fileIndex) {
		// Calculate compressed file size from the difference between the next file's offset
		// and this file's offset... or the difference between this file's offset and the 
		// packfile size if this is the last file
		uint32_t compressedSize = 0;
		if (fileIndex + 1 != curr1AHeader.numOfFiles) {
			compressedSize = curr1AFileInfos[fileIndex + 1].offset -
				curr1AFileInfos[fileIndex].offset;
		}
		else {
			_wstat(inDatName, &s);
			compressedSize = s.st_size -
				curr1AFileInfos[fileIndex].offset;
		}

		// Read in file data
		uint8_t* currFileData = new uint8_t[compressedSize];
		fread(currFileData, compressedSize, 1, inDat);

		// Set output path and write decompressed file
		wchar_t outPath[MAX_PATH];
		uint32_t pos = swprintf(outPath, L"%ls\\%02i", outFolderName, fileIndex);
		if (renameType != L"none") {
			wcscat(outPath, L"_");
			++pos;
			if (renameType == L"enemy") {
				if (fileIndex < 6) {
					swprintf(outPath + pos, L"STG%i.ECL", (fileIndex % 6) + 1);
				}
				else if (fileIndex < 12) {
					swprintf(outPath + pos, L"STG%i.SCL", (fileIndex % 6) + 1);
				}
				else if (fileIndex < 18) {
					swprintf(outPath + pos, L"STG%i.MAP", (fileIndex % 6) + 1);
				}
				else if (fileIndex < 24) {
					swprintf(outPath + pos, L"STG%i.DEM", (fileIndex % 6) + 1);
				}
				else if (fileIndex == 24) {
					wcscat(outPath, L"STG7.ECL");
				}
				else if (fileIndex == 25) {
					wcscat(outPath, L"STG7.SCL");
				}
				else if (fileIndex == 26) {
					wcscat(outPath, L"STG7.MAP");
				}
				else if (fileIndex < 47) {
					swprintf(outPath + pos, L"MUSCMT%02i.TXT", fileIndex % 27);
				}
				else if (fileIndex == 47) {
					wcscat(outPath, L"ENDING.SCL");
				}
			}
			else if (renameType == L"graph") {
				if (fileIndex == 0) {
					wcscat(outPath, L"COMMON");
				}
				else if (fileIndex < 7) {
					swprintf(outPath + pos, L"STG%iENM", fileIndex);
				}
				else if (fileIndex < 13) {
					swprintf(outPath + pos, L"STG%iBG", (fileIndex % 7) + 1);
				}
				else if (fileIndex < 23) {
					swprintf(outPath + pos, L"FACE%i", fileIndex % 13);
				}
				else if (fileIndex == 23) {
					wcscat(outPath, L"MUSICROOM");
				}
				else if (fileIndex == 24) {
					wcscat(outPath, L"TITLE");
				}
				else if (fileIndex == 25) {
					wcscat(outPath, L"SCORE");
				}
				else if (fileIndex == 26) {
					wcscat(outPath, L"VIVBOMB");
				}
				else if (fileIndex == 27) {
					wcscat(outPath, L"STG7BG");
				}
				else if (fileIndex == 28) {
					wcscat(outPath, L"STG7ENM");
				}
				else if (fileIndex == 29) {
					wcscat(outPath, L"STG7ENM2");
				}
				else if (fileIndex == 30) {
					wcscat(outPath, L"STG7ENM3");
				}
				else if (fileIndex == 31) {
					wcscat(outPath, L"SH01LOGO");
				}
				wcscat(outPath, L".BMP");
			}
			else if (renameType == L"graph2") {
				if (fileIndex != 0) {
					swprintf(outPath + pos, L"END%02i", fileIndex);
				}
				else {
					wcscat(outPath, L"CREDITS");
				}
				wcscat(outPath, L".BMP");
			}
			else if (renameType == L"music") {
				swprintf(outPath + pos, L"SH01_%02i.MID", fileIndex);
			}
			else if (renameType == L"sound") {
				std::wstring soundFilenames[] = { L"KEBARI.WAV", L"TAME.WAV", L"LASER.WAV",
					L"LASER2.WAV", L"BOMB.WAV", L"SELECT.WAV", L"HIT.WAV", L"CANCEL.WAV",
					L"WARNING.WAV", L"SBLASER.WAV", L"BUZZ.WAV", L"MISSILE.WAV", L"JOINT.WAV",
					L"DEAD.WAV", L"SBBOMB.WAV", L"BOSSBOMB.WAV", L"ENEMYSHOT.WAV",
					L"HLASER.WAV", L"TAMEFAST.WAV", L"WARP.WAV" };
				wcscat(outPath, soundFilenames[fileIndex].c_str());
			}
		}

		printf("Unpacking file %u...\n", fileIndex);

		// Decompress and write file data
		FILE* outFile = _wfopen(outPath, L"wb");
		if (!outFile) {
			printf("Unable to open output file!\n");
			return -8;
		}
		uint8_t* decompressedFile = decompress(currFileData, 
			curr1AFileInfos[fileIndex].uncompressedSize, compressedSize, 13);
		delete[] currFileData;
		fwrite(decompressedFile, curr1AFileInfos[fileIndex].uncompressedSize, 1, outFile);
		delete[] decompressedFile;
		fclose(outFile);
	}

	fclose(inDat);
	printf("Files successfully extracted!\n");
	return 0;
}

// Pack a PBG1A packfile
int pbg1APack(wchar_t inFolderName[], wchar_t outDatName[])
{
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	// Form file search path
	wchar_t searchPath[MAX_PATH];
	swprintf(searchPath, L"%ls\\*", inFolderName);

	// Find first file in given path
	hFind = FindFirstFileW(searchPath, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("Given folder not found...\n");
		return -3;
	}

	// Open output packfile for writing
	FILE* outDat = _wfopen(outDatName, L"wb");
	if (!outDat) {
		printf("Failed to open output packfile!\n");
		return -9;
	}
	// Write temporary header space
	PBG1AHeader curr1AHeader = { 0 };
	fwrite(&curr1AHeader, sizeof(PBG1AHeader), 1, outDat);

	// Count valid files in directory
	do {
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			++curr1AHeader.numOfFiles;
		}
	} while (FindNextFileW(hFind, &ffd));

	// Write temporary file info space
	PBG1AFileInfo* curr1AFileInfos = new PBG1AFileInfo[curr1AHeader.numOfFiles]();
	fwrite(curr1AFileInfos, curr1AHeader.numOfFiles * sizeof(PBG1AFileInfo), 1, outDat);
	hFind = FindFirstFileW(searchPath, &ffd);

	uint32_t fileIndex = 0;
	struct _stat s;
	// File packing loop
	do {
		// Make sure this file isn't hidden, a directory, or ./..
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			// Get proper path of this file and open it
			wchar_t filepath[MAX_PATH];
			swprintf(filepath, L"%ls\\%ls", inFolderName, ffd.cFileName);
			FILE* inFile = _wfopen(filepath, L"rb");
			if (!inFile) {
				printf("Error opening file...\n");
				return -4;
			}

			// Convert this wide filename to an ordinary string
			int byteCount = WideCharToMultiByte(932, 0, ffd.cFileName, -1, NULL, 0, NULL, NULL);
			char* filename = new char[byteCount];
			// Store wide filename as char with proper Shift-JIS encoding
			WideCharToMultiByte(932, 0, ffd.cFileName, -1, filename,
				byteCount, NULL, NULL);
			printf("Packing %s...\n", filename);
			delete[] filename;

			// Get file info
			PBG1AFileInfo curr1AFileInfo = { 0 };
			_wstat(filepath, &s);
			curr1AFileInfo.uncompressedSize = s.st_size;
			curr1AFileInfo.offset = ftell(outDat);

			// Read in file data
			uint8_t* currFileData = new uint8_t[curr1AFileInfo.uncompressedSize];
			fread(currFileData, curr1AFileInfo.uncompressedSize, 1, inFile);

			// Compress and write file data (13 dict bits)
			std::vector<uint8_t> compressedData = compress(currFileData, 
				curr1AFileInfo.uncompressedSize, 13);
			fwrite(&compressedData[0], compressedData.size(), 1, outDat);
			delete[] currFileData;

			// Literally sum compressed bytes for checksum
			for (uint32_t byteIndex = 0; byteIndex < compressedData.size(); ++byteIndex) {
				uint8_t currByte = compressedData[byteIndex];
				curr1AFileInfo.compressedChecksum += currByte;
			}
			// Increment the packfile checksum using the file's checksum,
			// uncompressed size, and offset
			curr1AHeader.checksum += curr1AFileInfo.compressedChecksum;
			curr1AHeader.checksum += curr1AFileInfo.uncompressedSize;
			curr1AHeader.checksum += curr1AFileInfo.offset;
			curr1AFileInfos[fileIndex] = curr1AFileInfo;
			++fileIndex;
		}
	} while (FindNextFileW(hFind, &ffd) && fileIndex < curr1AHeader.numOfFiles);

	//Rewrite proper header
	fseek(outDat, 0, SEEK_SET);
	curr1AHeader.magic = '\x1AGBP';	// PBG\x1A
	fwrite(&curr1AHeader, sizeof(PBG1AHeader), 1, outDat);
	fwrite(curr1AFileInfos, sizeof(PBG1AFileInfo), curr1AHeader.numOfFiles, outDat);
	fclose(outDat);

	printf("Files successfully packed!\n");
	return 0;
}