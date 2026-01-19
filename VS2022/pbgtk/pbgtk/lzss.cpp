// LZSS implementation from nmlgc's ssg and thtk

#include "lzss.h"

uint8_t BitReader::GetBit()
{
	if (cursor.byte >= size) {
		return 0xFF;
	}
	const bool ret = ((buffer[cursor.byte] >> (7 - cursor.bit)) & 1);
	cursor += 1;
	return ret;
}

uint32_t BitReader::GetBits(size_t bitcount)
{
	const unsigned int bytes_remaining = (size - cursor.byte);

	if ((bitcount > 32) || (bytes_remaining == 0)) {
		return 0xFFFFFFFF;
	}

	if (bitcount > 25) {
		uint32_t first3Bytes = GetBits(24);
		bitcount -= 24;
		return first3Bytes << bitcount | GetBits(bitcount);
	}

	if (((bitcount + 7) / 8) >= bytes_remaining) {
		bitcount = (((bytes_remaining * 8) - cursor.bit) < bitcount)
				   ? ((bytes_remaining * 8) - cursor.bit) : bitcount;
	}
	const unsigned int window_size = (cursor.bit + bitcount);

	uint32_t window = (buffer[cursor.byte + 0] << 24);
	if ((bitcount > 1) && (window_size > 8)) {
		window |= (buffer[cursor.byte + 1] << 16);
	}
	if ((bitcount > 9) && (window_size > 16)) {
		window |= (buffer[cursor.byte + 2] << 8);
	}
	if ((bitcount > 17) && (window_size > 24)) {
		window |= (buffer[cursor.byte + 3] << 0);
	}
	window <<= cursor.bit;
	cursor += bitcount;
	return (window >> (32 - bitcount));
}

void BitWriter::PutBit(uint8_t bit)
{	
	if (bit_cursor == 0)
	{
		buffer.push_back(0x00);
	}

	uint8_t& byte = buffer.back();
	
	byte |= ((bit & 1) << (7 - bit_cursor));
	bit_cursor++;
}

void BitWriter::PutBits(uint32_t bits, unsigned int bitcount)
{
	uint32_t mask = (1 << (bitcount - 1));
	for (unsigned int i = 0; i < bitcount; ++i) {
		PutBit((bits & mask) != 0);
		mask >>= 1;
	}
}

static inline unsigned int generate_key(unsigned char* array, const unsigned int base, const unsigned int mask)
{
	return ((array[(base + 1) & mask] << 8) |
		array[(base + 2) & mask]) ^ (array[base] << 4);
}

static inline void list_remove(hash_t* hash, const unsigned int key, const unsigned int offset)
{
	/* This function always removes the last entry in the list,
	 * or no entry at all. */

	 /* Set any previous entry's next pointer to HASH_NULL. */
	hash->next[hash->prev[offset]] = 0;

	/* XXX: This condition is not neccessary, but it might
	 * help optimization by not having to generate the key. */
	if (hash->prev[offset] == 0)
		/* If the entry being removed was the head, clear the head. */
		if (hash->hash[key] == offset)
			hash->hash[key] = 0;
}

static inline void list_add(hash_t* hash, const unsigned int key, const unsigned int offset)
{
	hash->next[offset] = hash->hash[key];
	hash->prev[offset] = 0;
	/* Update the previous pointer of the old head. */
	hash->prev[hash->hash[key]] = offset;
	hash->hash[key] = offset;
}

static inline void output(uint8_t literal, uint8_t* uncompressed, uint8_t* dict, uint32_t& out_i, const unsigned int mask) {
	uncompressed[out_i] = literal;
	dict[out_i & mask] = literal;
	++out_i;
}

// Generic LZSS decompression
// Uses 15 dict bits if PBG5 or later, or 13 if PBG4 or earlier
uint8_t* decompress(uint8_t* fileData, int uncompSize, int compSize, const unsigned int LZSS_DICT_BITS)
{
	const unsigned int LZSS_SEQ_BITS = 4;
	const unsigned int LZSS_SEQ_MIN = 3;
	const unsigned int LZSS_DICT_MASK = ((1 << LZSS_DICT_BITS) - 1);
	const unsigned int LZSS_SEQ_MAX = (LZSS_SEQ_MIN + ((1 << LZSS_SEQ_BITS) - 1));
	const unsigned int LZSS_DICT_SIZE = 1 << LZSS_DICT_BITS;

	// Textbook LZSS (from nmlgc's ssg)
	BitReader device(fileData, compSize);
	uint8_t* dict = new uint8_t[LZSS_DICT_SIZE];
	uint32_t out_i = 0;

	memset(dict, 0, LZSS_DICT_SIZE);

	uint8_t* uncompressed = new uint8_t[uncompSize];

	while (out_i < uncompSize) {
		const bool is_literal = device.GetBit();
		if (is_literal) {
			output(device.GetBits(8), uncompressed, dict, out_i, LZSS_DICT_MASK);
		}
		else {
			uint32_t seq_offset = device.GetBits(LZSS_DICT_BITS);
			if (seq_offset == 0) {
				break;
			}
			else {
				--seq_offset;
			}
			const unsigned int seq_length = (
				device.GetBits(LZSS_SEQ_BITS) + LZSS_SEQ_MIN
				);
			for (unsigned int i = 0; i < seq_length; ++i) {
				output(dict[seq_offset++ & LZSS_DICT_MASK], uncompressed, dict, out_i, LZSS_DICT_MASK);
			}
		}
	}

	delete[] dict;
	return uncompressed;
}

// Generic (optimized from thtk) LZSS compression
// Uses 15 dict bits if PBG5 or later, or 13 if PBG4 or earlier
std::vector<uint8_t> compress(uint8_t* fileData, int size, const unsigned int LZSS_DICT_BITS)
{
	const unsigned int LZSS_SEQ_BITS = 4;
	const unsigned int LZSS_SEQ_MIN = 3;
	const unsigned int LZSS_DICT_MASK = ((1 << LZSS_DICT_BITS) - 1);
	const unsigned int LZSS_SEQ_MAX = (LZSS_SEQ_MIN + ((1 << LZSS_SEQ_BITS) - 1));
	const unsigned int LZSS_DICT_SIZE = 1 << LZSS_DICT_BITS;

	// thtk LZSS implementation
	BitWriter device;
	hash_t hash(LZSS_DICT_SIZE);
	unsigned char* dict = new unsigned char[LZSS_DICT_SIZE];
	unsigned int dict_head = 1;
	unsigned int dict_head_key = 0;
	unsigned int waiting_bytes = 0;
	size_t bytes_read = 0;
	unsigned int i = 0;

	memset(dict, 0, sizeof(unsigned char) * LZSS_DICT_SIZE);

	// Fill the forward-looking buffer
	for (i = 0; i < LZSS_SEQ_MAX && i < size; ++i) {
		dict[dict_head + i] = fileData[bytes_read];
		++bytes_read;
		++waiting_bytes;
	}

	dict_head_key = generate_key(dict, dict_head, LZSS_DICT_MASK);

	while (waiting_bytes) {
		unsigned int match_len = LZSS_SEQ_MIN - 1;
		unsigned int match_offset = 0;
		unsigned int offset;

		// Find a good match
		for (offset = hash.hash[dict_head_key];
			offset != 0 && waiting_bytes > match_len;
			offset = hash.next[offset]) {
			// First, check a character further ahead to see if this match can
			// be any longer than the current match
			if (dict[(dict_head + match_len) & LZSS_DICT_MASK] ==
				dict[(offset + match_len) & LZSS_DICT_MASK]) {
				// Then check the previous characters
				for (i = 0;
					i < match_len &&
					(dict[(dict_head + i) & LZSS_DICT_MASK] ==
						dict[(offset + i) & LZSS_DICT_MASK]);
						++i)
					;

				if (i < match_len)
					continue;

				// Finally, try to extend the match
				for (++match_len;
					match_len < waiting_bytes &&
					(dict[(dict_head + match_len) & LZSS_DICT_MASK] ==
						dict[(offset + match_len) & LZSS_DICT_MASK]);
					++match_len)
					;

				match_offset = offset;
			}
		}

		// Write data to the output buffer
		if (match_len < LZSS_SEQ_MIN) {
			match_len = 1;
			device.PutBit(1);
			device.PutBits(dict[dict_head], 8);
		}
		else {
			device.PutBit(0);
			device.PutBits(match_offset, LZSS_DICT_BITS);
			device.PutBits(match_len - LZSS_SEQ_MIN, 4);
		}

		// Add bytes to the dictionary
		for (i = 0; i < match_len; ++i) {
			const unsigned int offset =
				(dict_head + LZSS_SEQ_MAX) & LZSS_DICT_MASK;

			if (offset != 0)
				list_remove(&hash, generate_key(dict, offset, LZSS_DICT_MASK), offset);
			if (dict_head != 0)
				list_add(&hash, dict_head_key, dict_head);

			if (bytes_read < size) {
				dict[offset] = fileData[bytes_read];
				++bytes_read;
			}
			else {
				--waiting_bytes;
			}

			dict_head = (dict_head + 1) & LZSS_DICT_MASK;
			dict_head_key = generate_key(dict, dict_head, LZSS_DICT_MASK);
		}
	}

	if (LZSS_DICT_BITS == 13) {
		// Write the sentinel offset
		device.PutBit(false);
		device.PutBits(0, LZSS_DICT_BITS);
	}

	delete[] dict;
	delete[] hash.hash;
	delete[] hash.prev;
	delete[] hash.next;
	
	return device.buffer;
}