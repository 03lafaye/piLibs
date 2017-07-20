#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include "piTypes.h"
#include "piFile.h"

namespace piLibs {

piFile::piFile()
{
    mInternal = nullptr;
}

piFile::~piFile()
{
}


//-------------------------------------------------------------------------------------------


char *piFile::ReadString(char *buffer, int num)
{
    return fgets(buffer, num, (FILE*)mInternal);
}

void piFile::Prints(const wchar_t *str)
{
    fwprintf( (FILE*)mInternal, str);
}

void piFile::Printf(const wchar_t *format, ...)
{
    va_list arglist;

    va_start(arglist, format);


    vfwprintf( (FILE*)mInternal, format, arglist);
    //int maxLen = pivscwprintf(format, arglist) + 1;
    //wchar_t *tmpstr = (wchar_t*)_malloca(maxLen*sizeof(wchar_t));
    //if (!tmpstr) return;


    //pivwsprintf(tmpstr, maxLen, format, arglist);

    va_end(arglist);

    //fwprintf((FILE*)mInternal, str);
}

//-------------------------------------------------------------------------------------------

bool piFile::Read( void *dst, uint64_t amount)
{
    // fread returns number of *elements read, which we have as always 1
    return fread(dst, (int)amount, 1, (FILE*)mInternal) == 1;
}

int piFile::Write( const void *dst, uint64_t amount)
{
    return (int)(fwrite(dst, (int)amount, 1, (FILE*)mInternal) * amount);
}


uint8_t piFile::ReadUInt8( void )
{
    uint8_t n;
    fread( &n, 1, 1, (FILE*)mInternal );
    return( n );
}


uint16_t piFile::ReadUInt16(void)
{
    uint16_t n;
    fread(&n, 1, 2, (FILE*)mInternal);
    return n;
}

uint32_t piFile::ReadUInt32( void )
{
    uint32_t n;
    fread(&n, 1, 4, (FILE*)mInternal);
    return n;
}

uint64_t piFile::ReadUInt64( void )
{
    uint64_t n;
    fread(&n, 1, 8, (FILE*)mInternal);
    return n;
}

float piFile::ReadFloat( void )
{
    float n;
    fread(&n, 1, 4, (FILE*)mInternal);
    return n;
}

//-------------------------------------------------------------------------------------------

void piFile::ReadFloatarray(float *dst, int num, int size, uint64_t amount )
{
    unsigned int           i;
    //unsigned char byte[4];
    unsigned char *ptr = (unsigned char *)dst;


    for( i=0; i<amount; i++ )
	{
	fread( ptr, num, 4, (FILE*)mInternal );
	ptr += size;
	}
}

void piFile::ReadFloatarray2(float *dst, uint64_t amount )
{
    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t)(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fread( dst, blocksize, 4, (FILE*)mInternal );
        dst += blocksize;
        }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 4, (FILE*)mInternal );
    #else
    fread( dst, amount, 4, (FILE*)mInternal );
    #endif
}

void piFile::ReadUInt32array(uint32_t *dst, uint64_t amount )
{
    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t)(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fread( dst, blocksize, 4, (FILE*)mInternal );
        dst += blocksize;
        }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 4, (FILE*)mInternal );
    #else
    fread( dst, amount, 4, (FILE*)mInternal );
    #endif
}


void piFile::ReadUInt64array(uint64_t *dst, uint64_t amount)
{
#ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t)(amount >> 24);
    size_t blocksize = 1 << 24;
    for (unsigned int nb = 0; nb<numblocks; nb++)
    {
        fread(dst, blocksize, 8, (FILE*)mInternal);
        dst += blocksize;
    }
    fread(dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 8, (FILE*)mInternal);
#else
    fread(dst, amount, 8, (FILE*)mInternal);
#endif
}


void piFile::ReadUInt32array2(uint32_t *dst, uint64_t amount, int size, int stride)
{
    if( size != stride )
    {
        for( uint64_t i=0; i<amount; i++ )
        {
            const int gap = (stride-size)*4;
            fread( dst, size, 4, (FILE*)mInternal );
            dst += size;
            fseek((FILE*)mInternal, gap, SEEK_CUR);
        }
        return;
    }

    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t)(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fread( dst, blocksize, 4, (FILE*)mInternal );
        dst += blocksize;
        }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 4, (FILE*)mInternal );
    #else
    fread( dst, amount, 4, (FILE*)mInternal );
    #endif
}

void piFile::ReadUInt16array(uint16_t *dst, uint64_t amount)
{
    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t )(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fread( dst, blocksize, 2, (FILE*)mInternal );
        dst += blocksize;
        }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 2, (FILE*)mInternal );
    #else
    fread( dst, amount, 2, (FILE*)mInternal );
    #endif
}

void piFile::ReadUInt8array(uint8_t *dst, uint64_t amount)
{
    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t)(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fread( dst, blocksize, 1, (FILE*)mInternal );
        dst += blocksize;
        }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 1, (FILE*)mInternal );
    #else
    fread( dst, amount, 1, (FILE*)mInternal );
    #endif
}

void piFile::ReadDoublearray2(double *dst, uint64_t amount )
{
    #ifdef WINDOWS
    // shit, windows cannot read big blocks of data at once...
    size_t numblocks = (size_t )(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
    {
        fread( dst, blocksize, 8, (FILE*)mInternal );
        dst += blocksize;
    }
    fread( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 8, (FILE*)mInternal );
    #else
    fread( dst, amount, 8, (FILE*)mInternal );
    #endif
}


void piFile::WriteUInt8(uint8_t x )
{
    fwrite( &x, 1, 1, (FILE*)mInternal );
}

void piFile::WriteUInt16(uint16_t x )
{
    fwrite( &x, 1, 2, (FILE*)mInternal );
}

void piFile::WriteUInt32(uint32_t n )
{
    fwrite( &n, 1, 4, (FILE*)mInternal );
}

void piFile::WriteUInt64(uint64_t n)
{
    fwrite(&n, 1, 8, (FILE*)mInternal);
}

void piFile::WriteFloat(float x )
{
    fwrite( &x, 1, 4, (FILE*)mInternal );
}

void piFile::WriteFloatarray2(float *ori, uint64_t amount )
{
    fwrite( ori, 4, (size_t)amount, (FILE*)mInternal );
}

void piFile::WriteDoublearray2(double *ori, uint64_t amount )
{
    fwrite( ori, (size_t)amount, 8, (FILE*)mInternal );
}

void piFile::WriteUInt32array(uint32_t *dst, uint64_t amount)
{
    #ifdef WINDOWS
    // shit, windows cannot fwrite big blocks of data at once...
    size_t numblocks = (size_t)(amount>>24);
    size_t blocksize = 1<<24;
    for( unsigned int nb=0; nb<numblocks; nb++ )
        {
        fwrite( dst, blocksize, 4, (FILE*)mInternal );
        dst += blocksize;
        }
    fwrite( dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 4, (FILE*)mInternal );
    #else
    fwrite( dst, amount, 4, (FILE*)mInternal );
    #endif
}

void piFile::WriteUInt64array(uint64_t *dst, uint64_t amount)
{
#ifdef WINDOWS
    // shit, windows cannot fwrite big blocks of data at once...
    size_t numblocks = (size_t)(amount >> 24);
    size_t blocksize = 1 << 24;
    for (unsigned int nb = 0; nb<numblocks; nb++)
    {
        fwrite(dst, blocksize, 8, (FILE*)mInternal);
        dst += blocksize;
    }
    fwrite(dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 8, (FILE*)mInternal);
#else
    fwrite(dst, amount, 8, (FILE*)mInternal);
#endif
}

void piFile::WriteUInt16array( uint16_t *dst, uint64_t amount)
{
#ifdef WINDOWS
	// shit, windows cannot fwrite big blocks of data at once...
	size_t numblocks = (size_t)(amount >> 24);
	size_t blocksize = 1 << 24;
	for (unsigned int nb = 0; nb<numblocks; nb++)
	{
        fwrite(dst, blocksize, 2, (FILE*)mInternal);
		dst += blocksize;
	}
    fwrite(dst, (size_t)(amount - (uint64_t)(numblocks*blocksize)), 2, (FILE*)mInternal);
#else
	fwrite(dst, amount, 4, fp);
#endif
}
/*
void write_int16array(uint16 *dst, size_t amount)
{
    fwrite( dst, amount, 2, (FILE*)mInternal );
}

void write_int8array(uint8 *dst, size_t amount)
{
    fwrite( dst, amount, 1, (FILE*)mInternal );
}*/



/*
int  FILE_Copy( char *dst, char *ori )
{
	FILE    *fo;
	FILE    *fd;
    long    l;
    char    *tmp;

	fo = FILE_Open( ori, "rb" );
	if( !fo )
		return( 0 );

	fd = FILE_Open( dst, "wb" );
	if( !fd )
        {
        fclose( fo );
		return( 0 );
        }

    tmp = (char*)malloc( 65536 );
    if( !tmp )
        {
	    fclose( fo );
        fclose( fd );
        return( 0 );
        }
    
    l = FILE_GetLength( fo );
    long i, n;
    n = l >> 16;

    for( i=0; i<n; i++ )
        {
        fread( tmp, 1, 65536, fo );
        fwrite( tmp, 1, 65536, fd );
        }

    n = l & 65535;
    fread(  tmp, 1, n, fo );
    fwrite( tmp, 1, n, fd );
    

	fclose( fo );
    fclose( fd );

    free( tmp );
	return( 1 );

}*/

}