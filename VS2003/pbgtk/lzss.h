// LZSS implementation from nmlgc's ssg and thtk

#pragma once

#include <vector>
#include "stdint.h"

struct hash_t {
	unsigned int* hash;
	unsigned int* prev;
	unsigned int* next;

	hash_t(const unsigned int DICT_SIZE) {
		hash = new unsigned int[0x10000];
		prev = new unsigned int[DICT_SIZE];
		next = new unsigned int[DICT_SIZE];
		memset(hash, 0, 0x10000 * sizeof(unsigned int));
		memset(prev, 0, DICT_SIZE * sizeof(unsigned int));
		memset(next, 0, DICT_SIZE * sizeof(unsigned int));
	}
};

class BitReader {
	struct {
		size_t byte;
		uint8_t bit;

		void operator +=(unsigned int bitcount) {
			bit += bitcount;
			byte += (bit / 8);
			bit %= 8;
		}
	} cursor;

public:
	uint8_t* buffer;
	int size;

	BitReader(uint8_t* mem, int givenSize) {
		cursor.byte = 0;
		cursor.bit = 0;
		buffer = mem;
		size = givenSize;
	}

	// Returns 0xFF if we're at the end of the stream.
	uint8_t GetBit();

	// Returns 0xFFFFFFFF if we're at the end of the stream. Supports a maximum
	// of 32 bits.
	uint32_t GetBits(size_t bitcount);
};

struct BitWriter {
	std::vector<uint8_t> buffer;
	uint8_t bit_cursor:3;

	BitWriter() {
		bit_cursor = 0;
	}

	void PutBit(uint8_t bit);
	void PutBits(uint32_t bits, unsigned int bitcount);
};

uint8_t* decompress(uint8_t* fileData, int uncompSize, int compSize, const unsigned int LZSS_DICT_BITS);
std::vector<uint8_t> compress(uint8_t* fileData, int size, const unsigned int DICT_BITS);