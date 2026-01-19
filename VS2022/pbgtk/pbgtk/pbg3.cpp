// PBG3
// jwilins
// Unpacks and repacks files from the PBG3 format used in Seihou 2 (Kioh Gyoku)

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <string>
#include <Windows.h>
#include "stdint.h"
#include "lzss.h"

struct PBG3Header {
	uint32_t magic;	// PBG3
	uint32_t numOfFiles;
	uint32_t tocOffset;
};

struct PBG3FileInfo {
	uint32_t unknown1;
	uint32_t unknown2;
	uint32_t compressedChecksum;
	uint32_t offset;
	uint32_t uncompressedSize;
	char* filename;
};

class PBG3BitReader {
	private:
		BitReader reader;
	public:
		PBG3BitReader(uint8_t* mem, size_t givenSize) :
			reader(mem, givenSize) {}

		unsigned int readInt()
		{
			uint32_t size = reader.GetBits(2) + 1;
			return reader.GetBits(size * 8);
		}

		std::string readString()
		{
			std::string filenameStr = "";
			uint32_t currByte = reader.GetBits(8);
			while (currByte != 0x00) {
				filenameStr.push_back((char)currByte);
				currByte = reader.GetBits(8);
			}
			return filenameStr;
		}
};

class PBG3BitWriter {
	private:
		BitWriter writer;
	public:
		void writeInt(uint32_t anInt)
		{
			unsigned int size = 1;
			if (anInt & 0xFFFFFF00) {
				size = 2;
				if (anInt & 0xFFFF0000) {
					size = 3;
					if (anInt & 0xFF000000) {
						size = 4;
					}
				}
			}

			writer.PutBits(size - 1, 2);
			writer.PutBits(anInt, size * 8);
		}

		void writeString(char* aString)
		{
			for (unsigned int charIndex = 0; charIndex < strlen(aString) + 1; ++charIndex) {
				writer.PutBits(aString[charIndex], 8);
			}
		}

		std::vector<uint8_t> getBuffer()
		{
			return writer.buffer;
		}
};

// Recursive file packing
int searchAndPack(const wchar_t* folderName, FILE* outDat, std::vector<PBG3FileInfo> &curr3FileInfos,
				  bool removeExtensions, const wchar_t* baseFolderName)
{
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	// Set file serach path
	wchar_t searchPath[MAX_PATH];
	swprintf(searchPath, L"%ls\\*", folderName);

	// Find first file in given path
	hFind = FindFirstFileW(searchPath, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("Given folder not found...\n");
		return -3;
	}

	// Recursive file packing loop
	struct _stat s;
	do {
		// Make sure this file isn't hidden or ./..
		if ((wcscmp(ffd.cFileName, L".") && wcscmp(ffd.cFileName, L".."))
			&& !(ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
			// Get file path
			wchar_t path[MAX_PATH];
			swprintf(path, L"%ls\\%ls", folderName, ffd.cFileName);
			// Recursively call to pack if this is a directory
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				int searchPackResult = searchAndPack(path, outDat, curr3FileInfos,
													 removeExtensions, baseFolderName);
				if (searchPackResult != 0) {
					return searchPackResult;
				}
			}
			else {
				// Main packing
				// Open file
				FILE* inFile = _wfopen(path, L"rb");
				if (!inFile) {
					printf("Error opening file...\n");
					return -4;
				}

				// Get file info
				PBG3FileInfo curr3FileInfo = { 0 };
				curr3FileInfo.offset = ftell(outDat);
				_wstat(path, &s);
				curr3FileInfo.uncompressedSize = s.st_size;

				int byteCount = WideCharToMultiByte(932, 0, path, -1, 
					NULL, 0, NULL, NULL);
				char* fullPath = new char[byteCount];
				// Store wide filename as char with proper Shift-JIS encoding
				WideCharToMultiByte(932, 0, path, -1, fullPath,
					byteCount, NULL, NULL);
				std::string filename = fullPath;
				delete[] fullPath;

				// Delete base path provided by user
				filename.erase(0, wcslen(baseFolderName) + 1);
				// Convert '\' to '/' for packed paths
				int slashPos = 0;
				while ((slashPos = filename.find("\\")) != std::string::npos) {
					filename[slashPos] = '/';
				}
				// Remove file extension if requested
				if (removeExtensions) {
					int lastDotPos = filename.find_last_of(".");
					if (lastDotPos != std::string::npos) {
						filename.erase(lastDotPos);
					}
				}
				curr3FileInfo.filename = new char[filename.length() + 1];
				memcpy(curr3FileInfo.filename, filename.c_str(), filename.length() + 1);

				printf("Packing %s...\n", curr3FileInfo.filename);

				// Read in file data
				uint8_t* currFileData = new uint8_t[curr3FileInfo.uncompressedSize];
				fread(currFileData, curr3FileInfo.uncompressedSize, 1, inFile);

				// Compress and write file data
				std::vector<uint8_t> compressedData = compress(currFileData, curr3FileInfo.uncompressedSize, 13);
				fwrite(&compressedData[0], compressedData.size(), 1, outDat);
				delete[] currFileData;

				// Literally sum compressed file bytes for checksum
				for (uint32_t byteIndex = 0; byteIndex < compressedData.size(); ++byteIndex) {
					uint8_t currByte = compressedData[byteIndex];
					curr3FileInfo.compressedChecksum += currByte;
				}

				curr3FileInfos.push_back(curr3FileInfo);
			}
		}
	} while (FindNextFileW(hFind, &ffd));

	return 0;
}

// Extract a PBG3 packfile
int pbg3Extract(wchar_t inDatName[], wchar_t outFolderName[], std::wstring renameType)
{
	// Open input packfile
	FILE* inDat = _wfopen(inDatName, L"rb");
	if (!inDat) {
		printf("Error opening packfile!\n");
		return -1;
	}

	// Read and check PBG3 magic
	PBG3Header curr3Header = { 0 };
	fread(&curr3Header.magic, sizeof(uint32_t), 1, inDat);
	if (curr3Header.magic != '3GBP') {	// PBG3
		printf("Not a valid packfile!\n");
		return -2;
	}

	// Header after magic will at maximum be 9 bytes long, so read all of those
	uint8_t maxHeaderBytes[9] = { 0 };
	fread(&maxHeaderBytes, sizeof(uint8_t), 9, inDat);
	// Initialize bitreader and read bitstream values
	PBG3BitReader reader(maxHeaderBytes, 9);
	curr3Header.numOfFiles = reader.readInt();
	curr3Header.tocOffset = reader.readInt();

	// Create directory provided by user if nonexistent
	if (CreateDirectoryW(outFolderName, NULL) == 0
		&& GetLastError() != ERROR_ALREADY_EXISTS) {
		printf("Unable to create given directory!\n");
		return -5;
	}

	// Read in table of contents
	struct _stat s;
	_wstat(inDatName, &s);
	fseek(inDat, curr3Header.tocOffset, SEEK_SET);
	size_t tocSize = s.st_size - curr3Header.tocOffset;
	uint8_t* tocData = new uint8_t[tocSize];
	fread(tocData, tocSize, 1, inDat);

	// Read in bitstream file infos
	PBG3BitReader tocReader(tocData, tocSize);
	PBG3FileInfo* curr3FileInfos = new PBG3FileInfo[curr3Header.numOfFiles]();
	for (uint32_t fileIndex = 0; fileIndex < curr3Header.numOfFiles; ++fileIndex) {
		curr3FileInfos[fileIndex].unknown1 = tocReader.readInt();
		curr3FileInfos[fileIndex].unknown2 = tocReader.readInt();
		curr3FileInfos[fileIndex].compressedChecksum = tocReader.readInt();
		curr3FileInfos[fileIndex].offset = tocReader.readInt();
		curr3FileInfos[fileIndex].uncompressedSize = tocReader.readInt();
		std::string filename = tocReader.readString();
		curr3FileInfos[fileIndex].filename = new char[filename.length() + 1];
		memcpy(curr3FileInfos[fileIndex].filename, filename.c_str(), filename.length() + 1);
	}

	// File extraction loop
	for (uint32_t fileIndex = 0; fileIndex < curr3Header.numOfFiles; ++fileIndex) {
		std::string filename = curr3FileInfos[fileIndex].filename;
		delete[] curr3FileInfos[fileIndex].filename;
		int prevIndex = 0;
		int slashPos = 0;
		// Start full folder path with the user-provided path
		wchar_t fullFolderName[MAX_PATH];
		int pos = swprintf(fullFolderName, outFolderName);
		// Create each subdirectory in the current filename (and replace '/' with '\')
		while ((slashPos = filename.find("/", prevIndex)) != std::string::npos) {
			std::string folderName = filename.substr(prevIndex, slashPos - prevIndex);
			int byteCount = MultiByteToWideChar(932, 0,  folderName.c_str(), -1, NULL, 0);
			wchar_t* wideFolderName = new wchar_t[byteCount];
			// Store folder name as wide char with proper Shift-JIS encoding
			MultiByteToWideChar(932, 0, folderName.c_str(), folderName.length() + 1,
				wideFolderName, byteCount);
			swprintf(fullFolderName + pos, L"\\%ls", wideFolderName);
			delete[] wideFolderName;
			if (CreateDirectoryW(fullFolderName, NULL) == 0 
				&& GetLastError() != ERROR_ALREADY_EXISTS) {
				printf("Unable to create given directory!\n");
				return -5;
			}
			filename[slashPos] = '\\';
			prevIndex = slashPos + 1;
		}

		printf("Unpacking %s...\n", filename.c_str());

		// Calculate compressed file size from the difference between the next file's offset
		// and this file's offset... or the difference between the TOC offset and this file's
		// offset if this is the last file
		size_t compressedSize = 0;
		if (fileIndex + 1 != curr3Header.numOfFiles) {
			compressedSize = curr3FileInfos[fileIndex + 1].offset -
				curr3FileInfos[fileIndex].offset;
		}
		else {
			compressedSize = curr3Header.tocOffset -
				curr3FileInfos[fileIndex].offset;
		}

		// Read compressed file data
		fseek(inDat, curr3FileInfos[fileIndex].offset, SEEK_SET);
		uint8_t* currFileData = new uint8_t[compressedSize];
		fread(currFileData, compressedSize, 1, inDat);

		int byteCount = MultiByteToWideChar(932, 0, filename.c_str(), -1, NULL, 0);
		wchar_t* wideFilename = new wchar_t[byteCount];
		// Store filename as wide char with proper Shift-JIS encoding
		MultiByteToWideChar(932, 0, filename.c_str(), filename.length() + 1,
			wideFilename, byteCount);

		// Form extracted file path
		wchar_t outPath[MAX_PATH];
		swprintf(outPath, L"%ls\\%ls", outFolderName, wideFilename);
		if (renameType != L"none") {
			if (filename == "@VERSION@") {
				wcscat(outPath, L".STR");
			}
			else {
				if (renameType == L"enemy") {
					if (filename.substr(0, 7) == "SCRIPT\\") {
						wcscat(outPath, L".SCL");
					}
					else {
						wcscat(outPath, L".STR");
					}
				}
				else if (renameType == L"graph") {
					if (wcscmp(wideFilename, L"GRP\\タイトル")) {
						wcscat(outPath, L".BMP");
					}
					else {
						wcscat(outPath, L".JPG");
					}
				}
				else if (renameType == L"graph2" || renameType == L"graph3") {
					if (filename.substr(0, 5) == "LOAD\\" || filename == "YUKA\\ATK02D" || filename == "YUKA\\ATK02U") {
						wcscat(outPath, L".BMP");
					}
					else {
						wcscat(outPath, L".TGA");
					}
				}
				else if (renameType == L"music") {
					if (filename.substr(0, 6) == "MUSIC\\") {
						wcscat(outPath, L".BMP");
					}
					else {
						wcscat(outPath, L".POS");
					}
				}
				else if (renameType == L"sound") {
					wcscat(outPath, L".WAV");
				}
			}
		}

		// Decompress and write output file
		FILE* outFile = _wfopen(outPath, L"wb");
		if (!outFile) {
			printf("Unable to open output file!\n");
			return -8;
		}
		uint8_t* decompressedFileData = decompress(currFileData, curr3FileInfos[fileIndex].uncompressedSize,
			compressedSize, 13);
		fwrite(decompressedFileData, curr3FileInfos[fileIndex].uncompressedSize, 1, outFile);
		delete[] decompressedFileData;
		fclose(outFile);
		delete[] wideFilename;
	}

	fclose(inDat);
	printf("Files successfully extracted!\n");
	return 0;
}

// Pack a PBG3 packfile
int pbg3Pack(wchar_t inFolderName[], wchar_t outDatName[], bool removeExtension)
{
	// Open output packfile for writing
	FILE* outDat = _wfopen(outDatName, L"wb");
	if (!outDat) {
		printf("Failed to open output packfile!\n");
		return -9;
	}

	// Write temporary header of 13 bytes
	char zeros[13] = { 0 };
	fwrite(zeros, sizeof(char), 13, outDat);

	std::vector<PBG3FileInfo> curr3FileInfos;
	int searchPackResult = searchAndPack(inFolderName, outDat, curr3FileInfos, removeExtension,
		inFolderName);
	if (searchPackResult != 0) {
		return searchPackResult;
	}
	
	// Collect info for packfile header
	PBG3Header curr3Header = { 0 };
	curr3Header.numOfFiles = curr3FileInfos.size();
	curr3Header.tocOffset = ftell(outDat);

	// Write PBG3 header bitstream
	PBG3BitWriter tocWriter;
	for (uint32_t fileIndex = 0; fileIndex < curr3Header.numOfFiles; ++fileIndex) {
		tocWriter.writeInt(curr3FileInfos[fileIndex].unknown1);
		tocWriter.writeInt(curr3FileInfos[fileIndex].unknown2);
		tocWriter.writeInt(curr3FileInfos[fileIndex].compressedChecksum);
		tocWriter.writeInt(curr3FileInfos[fileIndex].offset);
		tocWriter.writeInt(curr3FileInfos[fileIndex].uncompressedSize);
		tocWriter.writeString(curr3FileInfos[fileIndex].filename);
	}
	// Write final bitstream buffer
	std::vector<uint8_t> tocBuffer = tocWriter.getBuffer();
	fwrite(&tocBuffer[0], tocBuffer.size(), 1, outDat);

	// Create packfile header bitstream
	PBG3BitWriter headWriter;
	headWriter.writeInt(curr3Header.numOfFiles);
	headWriter.writeInt(curr3Header.tocOffset);
	std::vector<uint8_t> headBuffer = headWriter.getBuffer();

	// Write header magic
	fseek(outDat, 0, SEEK_SET);
	curr3Header.magic = '3GBP';
	fwrite(&curr3Header.magic, sizeof(uint32_t), 1, outDat);
	// Write header bitstream buffer
	fwrite(&headBuffer[0], headBuffer.size(), 1, outDat);
	fclose(outDat);

	printf("Files successfully packed!\n");
	return 0;
}