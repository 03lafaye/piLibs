/* 
 * Tiny PNG Output (C)
 * 
 * Copyright (c) 2014 Project Nayuki
 * https://www.nayuki.io/page/tiny-png-output-c
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program (see COPYING.txt).
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../libSystem/piTypes.h"
#include "../../../libSystem/piFile.h"


#include "TinyPngOut.h"

namespace piLibs {


/* Local declarations */

#define DEFLATE_MAX_BLOCK_SIZE 65535
#define MIN(x, y) ((x) < (y) ? (x) : (y))

static enum TinyPngOutStatus finish(const struct TinyPngOut *pngout);
static uint32_t crc32  (uint32_t state, const uint8_t *data, size_t len);
static uint32_t adler32(uint32_t state, const uint8_t *data, size_t len);


/* Public function implementations */

enum TinyPngOutStatus TinyPngOut_init(struct TinyPngOut *pngout, piFile *fout, int width, int height)
{
	// Check arguments
	if (width <= 0 || height <= 0)
		return TINYPNGOUT_INVALID_ARGUMENT;
	
	uint32_t lineSize = width * 3 + 1;
	
	uint32_t size = lineSize * height;  // Size of DEFLATE input
	pngout->deflateRemain = size;
	
	uint32_t overhead = size / DEFLATE_MAX_BLOCK_SIZE;
	if (overhead * DEFLATE_MAX_BLOCK_SIZE < size)
		overhead++;  // Round up to next block
	overhead = overhead * 5 + 6;
	size += overhead;  // Size of zlib+DEFLATE output
	
	// Set most of the fields
	pngout->width = lineSize;  // In bytes
	pngout->height = height;   // In pixels
	pngout->outStream = fout;
	pngout->positionX = 0;
	pngout->positionY = 0;
	pngout->deflateFilled = 0;
	pngout->adler = 1;
	
	// Write header (not a pure header, but a couple of things concatenated together)
	#define HEADER_SIZE 43
	uint8_t header[HEADER_SIZE] = {
		// PNG header
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
		// IHDR chunk
		0x00, 0x00, 0x00, 0x0D,
		0x49, 0x48, 0x44, 0x52,
        (uint8_t)(width  >> 24), (uint8_t)(width  >> 16), (uint8_t)(width  >> 8), (uint8_t)(width  >> 0),
            (uint8_t)(height >> 24), (uint8_t)(height >> 16), (uint8_t)(height >> 8), (uint8_t)(height >> 0),
		0x08, 0x02, 0x00, 0x00, 0x00,
		0, 0, 0, 0,  // IHDR CRC-32 to be filled in (starting at offset 29)
		// IDAT chunk
        (uint8_t)(size >> 24), (uint8_t)(size >> 16), (uint8_t)(size >> 8), (uint8_t)(size >> 0),
		0x49, 0x44, 0x41, 0x54,
		// DEFLATE data
		0x08, 0x1D,
	};
	uint32_t crc = crc32(0, &header[12], 17);
	header[29] = crc >> 24;
	header[30] = crc >> 16;
	header[31] = crc >>  8;
	header[32] = crc >>  0;
	if (fout->Write(header, HEADER_SIZE) != HEADER_SIZE)
		return TINYPNGOUT_IO_ERROR;
	
	pngout->crc = crc32(0, &header[37], 6);
	return TINYPNGOUT_OK;
}


enum TinyPngOutStatus TinyPngOut_write(struct TinyPngOut *pngout, const uint8_t *pixels, int count)
{
	const int32_t width  = pngout->width;
    const int32_t height = pngout->height;

	if (pngout->positionY == height) return TINYPNGOUT_DONE;
	if (count < 0 || pngout->positionY < 0 || pngout->positionY > height) return TINYPNGOUT_INVALID_ARGUMENT;
	
	count *= 3;
	piFile *f = pngout->outStream;
	while (count > 0) 
    {
		// Start DEFLATE block
		if (pngout->deflateFilled == 0) 
        {
			#define BLOCK_HEADER_SIZE 5
			int size = MIN(pngout->deflateRemain, DEFLATE_MAX_BLOCK_SIZE);
			uint8_t blockheader[BLOCK_HEADER_SIZE] = {
                (uint8_t)(pngout->deflateRemain <= DEFLATE_MAX_BLOCK_SIZE ? 1 : 0),
                (uint8_t)(size >> 0),
                (uint8_t)(size >> 8),
                (uint8_t)((size ^ 0xFFFF) >> 0),
                (uint8_t)((size ^ 0xFFFF) >> 8),
			};
			if (f->Write(blockheader, BLOCK_HEADER_SIZE) != BLOCK_HEADER_SIZE)
				return TINYPNGOUT_IO_ERROR;
			pngout->crc = crc32(pngout->crc, blockheader, BLOCK_HEADER_SIZE);
		}
		
		// Calculate number of bytes to write in this loop iteration
		int n = MIN(count, width - pngout->positionX);
		n = MIN(DEFLATE_MAX_BLOCK_SIZE - pngout->deflateFilled, n);
		if (n <= 0)  // Impossible
            return TINYPNGOUT_IO_ERROR;
		
		// Beginning of row - write filter method
		if (pngout->positionX == 0) 
        {
			uint8_t b = 0;
            f->WriteUInt8(b);
			pngout->crc = crc32(pngout->crc, &b, 1);
			pngout->adler = adler32(pngout->adler, &b, 1);
			pngout->deflateRemain--;
			pngout->deflateFilled++;
			pngout->positionX++;
			n--;
		}
		
		// Write bytes and update checksums
		if (f->Write(pixels, n) != n) return TINYPNGOUT_IO_ERROR;
		pngout->crc = crc32(pngout->crc, pixels, n);
		pngout->adler = adler32(pngout->adler, pixels, n);
		
		// Increment the position
		count -= n;
		pixels += n;
		
		pngout->deflateRemain -= n;
		pngout->deflateFilled += n;
		if (pngout->deflateFilled == DEFLATE_MAX_BLOCK_SIZE)
			pngout->deflateFilled = 0;
		
		pngout->positionX += n;
		if (pngout->positionX == width) 
        {
			pngout->positionX = 0;
			pngout->positionY++;
			if (pngout->positionY == height) 
            {
				if (count > 0)
					return TINYPNGOUT_INVALID_ARGUMENT;
				return finish(pngout);
			}
		}
	}
	return TINYPNGOUT_OK;
}


/* Private function implementations */

static enum TinyPngOutStatus finish(const struct TinyPngOut *pngout) 
{
	#define FOOTER_SIZE 20
	uint32_t adler = pngout->adler;
	uint8_t footer[FOOTER_SIZE] = {
		(uint8_t)(adler >> 24), (uint8_t)(adler >> 16), (uint8_t)(adler >> 8), (uint8_t)(adler >> 0),
		0, 0, 0, 0,  // IDAT CRC-32 to be filled in (starting at offset 4)
		// IEND chunk
		0x00, 0x00, 0x00, 0x00,
		0x49, 0x45, 0x4E, 0x44,
		0xAE, 0x42, 0x60, 0x82,
	};
	uint32_t crc = crc32(pngout->crc, &footer[0], 4);
	footer[4] = crc >> 24;
	footer[5] = crc >> 16;
	footer[6] = crc >>  8;
	footer[7] = crc >>  0;
	
	if (pngout->outStream->Write(footer, FOOTER_SIZE) != FOOTER_SIZE)
		return TINYPNGOUT_IO_ERROR;
	return TINYPNGOUT_OK;
}


static uint32_t crc32(uint32_t state, const uint8_t *data, size_t len) 
{
	state = ~state;
	for( size_t i = 0; i < len; i++) 
    {
		for (unsigned int j = 0; j < 8; j++) {  // Inefficient bitwise implementation, instead of table-based
			uint32_t bit = (state ^ (data[i] >> j)) & 1;
			state = (state >> 1) ^ ((-bit) & 0xEDB88320);
		}
	}
	return ~state;
}


static uint32_t adler32(uint32_t state, const uint8_t *data, size_t len) 
{
	uint16_t s1 = state >>  0;
	uint16_t s2 = state >> 16;
	size_t i;
	for (i = 0; i < len; i++) {
		s1 = (s1 + data[i]) % 65521;
		s2 = (s2 + s1) % 65521;
	}
	return (uint32_t)s2 << 16 | s1;
}

}