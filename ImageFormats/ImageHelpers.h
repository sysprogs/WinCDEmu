#pragma once

namespace ImageFormats
{
	static inline unsigned char bcd2int8(unsigned char bcd)
	{
		if ((bcd >= 0xA0) || ((bcd & 0x0F) >= 0x0A))
			return bcd;
		return (((bcd & 0xF0) >> 4) * 10) + (bcd & 0x0F);
	}

	struct MSFAddress
	{
		UCHAR M;
		UCHAR S;
		UCHAR F;

		unsigned ToLBA()
		{
			return ((M * 60 + S) * 75 + F) - 150;
		}

		static inline MSFAddress MSF(UCHAR m, UCHAR s, UCHAR f)
		{
			MSFAddress addr = {m,s,f};
			return addr;
		}

		static inline MSFAddress FromLBA(unsigned LBA)
		{
			MSFAddress addr;
			LBA += 150;
			addr.F = (UCHAR)(LBA % 75);
			LBA /= 75;
			addr.S = (UCHAR)(LBA % 60);
			addr.M = (UCHAR)(LBA / 60);
			return addr;
		}
	};

}