// Main
// jwilins
// Main pbgtk program code (used at launch)

#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include "stdint.h"
#include <Windows.h>
#include "pbg1a.h"
#include "pbg3.h"
#include "pbg4.h"
#include "pbg5.h"
#include "pbg6.h"

// Print program usage
void printUsage(wchar_t exeName[])
{
	printf("Usage: %ls extract version in_dat out_folder (--rename (preset))\n", exeName);
	printf("OR     %ls pack version in_folder out_dat (--remove-extensions)\n", exeName);
}

// Print auto-rename option usage
void printRenameUsage()
{
	printf("Improper auto-renaming preset specified!\n");
	printf("For Seihou 1 (PBG1A): enemy, graph, graph2, music, or sound\n");
	printf("For Seihou 2 (PBG3): enemy, graph, graph2, graph3, music, or sound\n");
}

// Main program
int wmain(int argc, wchar_t* argv[])
{
	// Force console output to Shift-JIS
	SetConsoleOutputCP(932);

	// Make sure there are enough arguments to run the utility
	if (argc < 5) {
		printUsage(argv[0]);
		return 0;
	}

	// Store option and version arguments as wide strings
	std::wstring option = argv[1];
	std::wstring version = argv[2];

	if (option == L"extract") {
		switch (version.at(0)) {
			case '1':
				if (argc >= 6) {
					std::wstring extraArg = argv[5];
					if (extraArg == L"--rename") {
						// Make sure an auto-renaming type is provided
						if (argc >= 7) {
							// Store auto-renaming type as lowercase string
							std::wstring lowercaseType = argv[6];
							for (uint32_t charIndex = 0; charIndex < lowercaseType.length(); ++charIndex) {
								lowercaseType[charIndex] = tolower(lowercaseType[charIndex]);
							}
							std::wstring renameTypes[] = { L"enemy", L"graph", L"graph2", L"music", L"sound" };
							// Try to find and use renaming type if valid
							for (uint32_t typeIndex = 0; typeIndex < sizeof(renameTypes) / sizeof(renameTypes[0]); ++typeIndex) {
								if (lowercaseType == renameTypes[typeIndex]) {
									return pbg1AExtract(argv[3], argv[4], lowercaseType);
								}
							}
							printRenameUsage();
							return -6;
						}
						else {
							printRenameUsage();
							return -7;
						}
					}
				}
				return pbg1AExtract(argv[3], argv[4], L"none");
			case '3':
				if (argc >= 6) {
					std::wstring extraArg = argv[5];
					if (extraArg == L"--rename") {
						// Make sure an auto-renaming type is provided
						if (argc >= 7) {
							// Store auto-renaming type as lowercase string
							std::wstring lowercaseType = argv[6];
							for (uint32_t charIndex = 0; charIndex < lowercaseType.length(); ++charIndex) {
								lowercaseType[charIndex] = tolower(lowercaseType[charIndex]);
							}
							std::wstring renameTypes[] = { L"enemy", L"graph", L"graph2", L"graph3", L"music", L"sound" };
							// Try to find and use renaming type if valid
							for (uint32_t typeIndex = 0; typeIndex < sizeof(renameTypes) / sizeof(renameTypes[0]); ++typeIndex) {
								if (lowercaseType == renameTypes[typeIndex]) {
									return pbg3Extract(argv[3], argv[4], lowercaseType);
								}
							}
							printRenameUsage();
							return -6;
						}
						else {
							printRenameUsage();
							return -7;
						}
					}
				}
				return pbg3Extract(argv[3], argv[4], L"none");
			case '4':
				return pbg4Extract(argv[3], argv[4]);
			case '5':
				return pbg5Extract(argv[3], argv[4]);
			case '6':
				return pbg6Extract(argv[3], argv[4]);
			default:
				printUsage(argv[0]);
				return 0;
		}
	}
	else if (option == L"pack")
	{
		switch (version.at(0)) {
			case '1':
				return pbg1APack(argv[3], argv[4]);
			case '3':
				if (argc >= 6) {
					std::wstring extraArg = argv[5];
					if (extraArg == L"--remove-extensions") {
						return pbg3Pack(argv[3], argv[4], true);
					}
				}
				return pbg3Pack(argv[3], argv[4], false);
			case '4':
				return pbg4Pack(argv[3], argv[4]);
			case '5':
				return pbg5Pack(argv[3], argv[4]);
			case '6':
				return pbg6Pack(argv[3], argv[4]);
			default:
				printUsage(argv[0]);
				return 0;
		}
	}
	else {
		printUsage(argv[0]);
		return 0;
	}
}