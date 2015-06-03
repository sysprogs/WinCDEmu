#pragma once
#include <string>


namespace UDFHelpers
{
	unsigned short CRC16(const void *pData, size_t length);
	std::wstring UncompressUnicode(size_t numberOfBytes, const unsigned char *UDFCompressed);

	template <size_t _Len> static inline std::wstring UncompressUnicode(const unsigned char (&UDFCompressed)[_Len])
	{
		return UncompressUnicode(_Len, UDFCompressed);
	}
}