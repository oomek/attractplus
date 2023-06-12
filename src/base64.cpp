#include "base64.hpp"

unsigned char base64_decode_char(const char base64)
{
	switch (base64)
	{
		case 'A': return 0;
		case 'B': return 1;
		case 'C': return 2;
		case 'D': return 3;
		case 'E': return 4;
		case 'F': return 5;
		case 'G': return 6;
		case 'H': return 7;
		case 'I': return 8;
		case 'J': return 9;
		case 'K': return 10;
		case 'L': return 11;
		case 'M': return 12;
		case 'N': return 13;
		case 'O': return 14;
		case 'P': return 15;
		case 'Q': return 16;
		case 'R': return 17;
		case 'S': return 18;
		case 'T': return 19;
		case 'U': return 20;
		case 'V': return 21;
		case 'W': return 22;
		case 'X': return 23;
		case 'Y': return 24;
		case 'Z': return 25;
		case 'a': return 26;
		case 'b': return 27;
		case 'c': return 28;
		case 'd': return 29;
		case 'e': return 30;
		case 'f': return 31;
		case 'g': return 32;
		case 'h': return 33;
		case 'i': return 34;
		case 'j': return 35;
		case 'k': return 36;
		case 'l': return 37;
		case 'm': return 38;
		case 'n': return 39;
		case 'o': return 40;
		case 'p': return 41;
		case 'q': return 42;
		case 'r': return 43;
		case 's': return 44;
		case 't': return 45;
		case 'u': return 46;
		case 'v': return 47;
		case 'w': return 48;
		case 'x': return 49;
		case 'y': return 50;
		case 'z': return 51;
		case '0': return 52;
		case '1': return 53;
		case '2': return 54;
		case '3': return 55;
		case '4': return 56;
		case '5': return 57;
		case '6': return 58;
		case '7': return 59;
		case '8': return 60;
		case '9': return 61;
		case '+': return 62;
		case '-': return 62;
		case '/': return 63;
		case '_': return 63;
		default: return 255;
	}
}

std::vector<BYTE> base64_decode(std::string const& data)
{
  int size = data.size();
	if (size < 2)
		return {};

	const auto padding = data[size - 1] == '=' ? (data[size - 2] == '=' ? 2 : 1) : 0;
	const auto size_without_padding = size - padding;
	const auto remainder = size_without_padding % 4;
	const auto safe_size = size_without_padding - remainder;
	std::vector<BYTE> out;
	out.reserve(size / 4 * 3);
	for (size_t i = 0; i < safe_size; i += 4)
	{
		const auto a = base64_decode_char(data[i]);
		const auto b = base64_decode_char(data[i + 1]);
		const auto c = base64_decode_char(data[i + 2]);
		const auto d = base64_decode_char(data[i + 3]);
		if (a == 255 || b == 255 || c == 255 || d == 255)
			return {};

		out.push_back((a << 2) | ((b & 0x30) >> 4));
		out.push_back(((b & 0x0F) << 4) | ((c & 0x3C) >> 2));
		out.push_back(((c & 0x03) << 6) | d);
	}

	if (remainder > 1)
	{
		const auto a = base64_decode_char(data[safe_size]);
		const auto b = base64_decode_char(data[safe_size + 1]);
		out.push_back((a << 2) | ((b & 0x30) >> 4));
		if (remainder > 2)
		{
			const auto c = base64_decode_char(data[safe_size + 2]);
			out.push_back(((b & 0x0F) << 4) | ((c & 0x3C) >> 2));
		}
	}
	return out;
}