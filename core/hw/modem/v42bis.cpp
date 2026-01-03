/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "v42bis.h"
#include <algorithm>
#include "stdclass.h"

namespace modem::v42b
{

enum
{
	// Control code words (compressed mode)
    ETM = 0,         // Enter transparent mode
    FLUSH = 1,       // Flush data
    STEPUP = 2,      // Step up codeword size

	// Command codes (transparent mode)
    ECM = 0,         // Enter compression mode
    EID = 1,         // Escape character in data
    RESET = 2        // Force reinitialization
};

void V42Base::Dictionary::reset()
{
	for (u16 i = CW_RESERVED; i < CW_FIRST; i++)
		nodes[i] = Node::makeRootNode(i - CW_RESERVED);
	std::fill(nodes.begin() + CW_FIRST, nodes.end(), Node{});
}

void V42Base::Dictionary::addChild(u16 childCode, Node& parent, u8 childChar)
{
	nodes[childCode] = { childChar, &parent };
	parent.addChild(nodes[childCode]);

#if 0 // !defined(NDEBUG) || defined(DEBUGFAST)
	std::string newString;
	Node *node = &nodes[childCode];
	while (true)
	{
		std::string s;
		if (isprint(node->character))
			s = (char)node->character;
		else
			s = strprintf("\\x%02x", node->character);
		newString = s + newString;
		if (node->isRoot())
			break;
		node = node->parent;
	}
	DEBUG_LOG(MODEM, "V.42bis Dict::added  %04x <=> '%s'", childCode, newString.c_str());
#endif
}

void V42Base::reset()
{
	dict.reset();
	nextCodeWord = CW_FIRST;
	codeWordSize = CHAR_SIZE + 1;
	codeWordThreshold = 2 * ALPHABET_SIZE;
	lastAddedCode = -1;
	matchLen = 0;
	curCode = -1;
	transparent = true;
	modeTransition = false;
	escapeCode = 0;
}

void V42Base::recoverNode()
{
	// 7.7 Node recovery
	// 6.5 Recovering a dictionary entry to use next
	while (true)
	{
		// 6.5(a) and (b) counter C1 shall be incremented; if the value of C1 exceeds N2-1 then C1 shall be set to N5
		if (++nextCodeWord >= maxCodeWords)
			nextCodeWord = CW_FIRST;
		Node& node = dict.get(nextCodeWord);
		// 6.5(c) We need to reuse a leaf or find a free node
		if (!node.isLeaf())
			continue;
		if (node.isFree())
			break;
		// 6.5(d) if the node is a leaf node, then it shall be detached from its parent
		node.parent->removeChild(node);
		break;
	}
}

int V42Base::acceptChar(u8 octet)
{
	if (curCode == -1)
	{
		curCode = octet + CW_RESERVED;
		matchLen = 1;
		modeTransition = false;
		return -1;
	}
	matchLen++;
	Node *node = dict.getChild(curCode, octet);
	if (node != nullptr && !modeTransition)
	{
		// node does exist
		u16 codeword = dict.getCode(node);
		// 6.3(b) If the string matches a dictionary entry, and the entry is not that entry
		// created by the last invocation of the string matching procedure, then the
		// next character shall be read and appended to the string and this step
		// repeated.
		if (codeword != lastAddedCode) {
			curCode = codeword;
			return -1;
		}
	}
	// 6.4 Add the string to the dictionary
	if (node == nullptr && matchLen <= maxStringLength)
	{
		checkDictSize();
		dict.addChild(nextCodeWord, dict.get(curCode), octet);
		lastAddedCode = nextCodeWord;
		recoverNode();
	}
	else {
		lastAddedCode = -1;
	}
	int emitCode;
	if (modeTransition)
		// this code's string has already been pushed in transparent mode
		emitCode = -1;
	else
		emitCode = curCode;
	modeTransition = false;
	curCode = octet + CW_RESERVED;
	matchLen = 1;
	return emitCode;
}

Compressor::Compressor(u32 maxCodeWords, int maxStringLength)
	: V42Base(maxCodeWords, maxStringLength)
{
	reset();
}

void Compressor::pushCode(u16 code)
{
	outBitBuf |= code << outBitPos;
	outBitPos += codeWordSize;
    while (outBitPos >= 8)
    {
        push(outBitBuf & 0xff);
        outBitBuf >>= 8;
        outBitPos -= 8;
    }
}

void Compressor::write(u8 b)
{
	if (transparent)
		push(b);
	if (b == escapeCode)
	{
		if (transparent)
			push(EID);
		escapeCode += 51;
	}
	int matchedLen = this->matchLen;
	int code = acceptChar(b);
	if (code != -1)
	{
		if (!transparent)
			// 7.5 Transfer - output the last state of the string
			pushCode(code);

		// 7.8 Data compressibility test
		// Same algorithm as the dreamcast modem driver, but we could use something different
		// Accumulate the difference in bits between compressed and uncompressed size. Positive numbers
		// indicate compressible data.
		compressTest += 8 * matchedLen - (int)codeWordSize;
		compressTestCount -= matchedLen;
		if (compressTestCount <= 0)
		{
			if (transparent && compressTest > 16)
				// Schedule a switch to compressed mode
				changeMode();
			else if (!transparent && compressTest < -16)
				// Schedule a switch to transparent mode
				changeMode();
		}
	}
}

void Compressor::changeMode()
{
	if (transparent)
	{
		DEBUG_LOG(MODEM, "Compressor: switching to compressed mode");
		// 7.8.1 Transition to compressed mode
		push(escapeCode);
		push(ECM);
		transparent = false;
		compressTestCount = 256;
	}
	else
	{
		DEBUG_LOG(MODEM, "Compressor:: switching to transparent mode");
		// 7.8.2 Transition to transparent mode
		flush(true);
		transparent = true;
		compressTestCount = 64;
	}
	modeTransition = true;
	compressTest = 0;
}

void Compressor::reset()
{
	V42Base::reset();
	outBitBuf = 0;
	outBitPos = 0;
	compressTest = 0;
	compressTestCount = 64;
}

void Compressor::checkDictSize()
{
	if (transparent)
		return;
	while (nextCodeWord >= codeWordThreshold && codeWordThreshold <= maxCodeWords)
	{
		DEBUG_LOG(MODEM, "Compressor: STEPUP");
		pushCode(STEPUP);
		codeWordSize++;
		codeWordThreshold *= 2;
	}
}

void Compressor::flush(bool enterTransparent)
{
	if (!transparent)
	{
		// Output the last state of the string
		if (curCode != -1 && !modeTransition)
		{
			// 7.8.2(a) ensure that the codeword representing any partially encoded data has been transferred
			pushCode(curCode);
			modeTransition = true;
		}
		if (enterTransparent) {
			// 7.8.2(c) indicate to the peer data compression function by transferring the ETM control codeword a
			// transition to transparent mode
			pushCode(ETM);
		}
		else {
			// 7.9(c) if the previous step leaves extra bits pending for transmission (octet alignment not yet achieved), then
			// transfer the FLUSH codeword
			if ((outBitPos % 8) != 0)
				pushCode(FLUSH);
		}
		// 7.8.2(d), 7.9(c) transmit sufficient 0 bits to recover octet alignment
		while (outBitPos > 0)
		{
			push(outBitBuf & 0xff);
			outBitBuf >>= 8;
			outBitPos -= 8;
		}
		outBitPos = 0;
	}
}

Decompressor::Decompressor(u32 maxCodeWords, int maxStringLength)
	: V42Base(maxCodeWords, maxStringLength)
{
	reset();
	decodeBuf.reserve(maxStringLength);
}

void Decompressor::write(u8 b)
{
	if (transparent)
	{
		if (escaped)
		{
			escaped = false;
			bool acceptChar = false;
			switch (b)
			{
			case ECM:
				DEBUG_LOG(MODEM, "Decompressor: ECM");
				transparent = false;
				modeTransition = true;
				break;
			case EID:
				DEBUG_LOG(MODEM, "Decompressor: EID");
				acceptChar = true; // the escape code needs to be processed normally
				b = escapeCode;
				escapeCode += 51;
				break;
			case RESET:
				DEBUG_LOG(MODEM, "Decompressor: RESET");
				reset();
				break;
			default:
				// 5.8(a) receipt of a reserved command code is an error condition
				WARN_LOG(MODEM, "V.42bis: invalid command code %02x", b);
				throw Exception("Invalid command code");
			}
			if (!acceptChar)
				return;
		}
		else if (b == escapeCode) {
			escaped = true;
			return;
		}
		push(b);
		acceptChar(b);
	}
	else
	{
		inputBitBuffer |= (u32)b << inputBitCount;
		inputBitCount += 8;
		if (inputBitCount < (int)codeWordSize)
			return;
		u32 newCode = (inputBitBuffer << (32 - codeWordSize)) >> (32 - codeWordSize);
		inputBitCount -= codeWordSize;
		inputBitBuffer >>= codeWordSize;

		if (newCode < CW_RESERVED)
		{
			// Control codes
			switch (newCode)
			{
			case ETM:
				DEBUG_LOG(MODEM, "Decompressor: ETM");
				transparent = true;
				modeTransition = true;
				inputBitCount = 0;
				inputBitBuffer = 0;
				break;
			case FLUSH:
				DEBUG_LOG(MODEM, "Decompressor: FLUSH");
				flush();
                break;
            case STEPUP:
            	DEBUG_LOG(MODEM, "Decompressor: STEPUP");
                ++codeWordSize;
                codeWordThreshold *= 2;
                // 5.8(a) receipt of a STEPUP codeword when it would cause the value of C2 to exceed N1 is an error condition
                if (codeWordThreshold > maxCodeWords)
        			throw Exception("Decompressor dictionary size exceeded limit");
                break;
            }
            return;
		}
		modeTransition = false;
		Node *node = &dict.get(newCode);
		// 5.8(c) receipt of a codeword representing an empty dictionary entry is an error condition
		if (node->isFree()) {
			WARN_LOG(MODEM, "Unknown code %x", newCode);
			throw Exception("Unknown V.42bis code");
		}
		// Walk up the tree one character at a time
		decodeBuf.clear();
		u8 firstChar;
		while (true)
		{
			firstChar = node->character;
			decodeBuf.insert(decodeBuf.begin(), firstChar);
			if (node->isRoot())
				break;
			node = node->parent;
		}
		// Output the decoded string
		for (u8 v : decodeBuf)
		{
			push(v);
			if (v == escapeCode)
				escapeCode += 51;
		}
		// 6.4 Add the string to the dictionary
		if (matchLen < maxStringLength
			// 6.4(a) The string does not exceed N7 in length
			&& lastAddedCode != curCode
			// 6.4(b) The string is not already in the dictionary
			&& dict.getChild(curCode, firstChar) == nullptr)
		{
			dict.addChild(nextCodeWord, dict.get(curCode), firstChar);
			recoverNode();
			lastAddedCode = nextCodeWord;
		}
		else {
			lastAddedCode = -1;
		}
		curCode = newCode;
		matchLen = decodeBuf.size();
	}
}

void Decompressor::flush()
{
	// 7.9(c) if step a) leaves extra bits pending for transmission (octet alignment not yet achieved),
	// then transfer sufficient 0 bits to recover octet alignment
	inputBitBuffer >>= inputBitCount & 7;
	inputBitCount &= ~7;
}

void Decompressor::reset()
{
	V42Base::reset();
	escaped = false;
	inputBitBuffer = 0;
	inputBitCount = 0;
}

} // namespace modem::v42b
