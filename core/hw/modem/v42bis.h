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
#pragma once
#include "types.h"
#include "internal.h"
#include <deque>
#include <map>
#include <vector>
#include <stdexcept>

namespace modem::v42b
{
// N3 Character size (bits)
static constexpr u32 CHAR_SIZE = 8;
// N4 Number of characters in the alphabet
static constexpr u32 ALPHABET_SIZE = 1 << CHAR_SIZE;
// N6 Number of control codewords
static constexpr u32 CW_RESERVED = 3;
// N5 Index number of first dictionary entry used to store a string
static constexpr u32 CW_FIRST = ALPHABET_SIZE + CW_RESERVED;
// 5.1 Minimum and default value for P1 (Total number of codewords)
static constexpr u32 MIN_TOTAL_CODEWORDS = 512;
// 5.1 Valid range for P2 (Maximum string length)
static constexpr int DEF_STRING_SIZE = 6;
static constexpr int MAX_STRING_SIZE = 250;

class Exception : public std::runtime_error
{
public:
	Exception(const char *what)
		: std::runtime_error(what)
	{}
};

class V42Base : public BufferedTransformer
{
protected:
	struct Node
	{
		u8 character;
		Node *parent;
		std::map<int, Node *> children;

		Node(u8 character = 0, Node *parent = nullptr)
			: character(character), parent(parent)
		{}

		bool isLeaf() const {
			return children.empty();
		}
		bool isFree() const {
			return parent == nullptr;
		}
		bool isRoot() const {
			return parent == reinterpret_cast<Node *>(-1);
		}

		Node *getChild(u8 octet)
		{
			auto it = children.find(octet);
			if (it == children.end())
				return nullptr;
			else
				return it->second;
		}

		void addChild(Node& child) {
			children[child.character] = &child;
		}
		void removeChild(Node &child) {
			children.erase(child.character);
			child.parent = nullptr;
		}

		static Node makeRootNode(u8 character) {
			return Node{ character, reinterpret_cast<Node *>(-1) };
		}
	};

	class Dictionary
	{
	public:
		Dictionary(u32 maxCodeWords) {
			nodes.resize(maxCodeWords);
			reset();
		}

		void reset();

		Node& get(u16 code) {
			return nodes[code];
		}

		u16 getCode(Node *node) const {
			return node - &nodes[0];
		}

		Node *getChild(u16 code, u8 childCharacter) {
			return nodes[code].getChild(childCharacter);
		}

		void addChild(u16 childCode, Node& parent, u8 childCharacter);

	private:
		std::vector<Node> nodes;
	};

	V42Base(u32 maxCodeWords, int maxStringLength)
		: maxCodeWords(maxCodeWords), maxStringLength(maxStringLength), dict(maxCodeWords)
	{
	}

	void push(u8 b) {
		buffer.push_back(b);
	}

	void reset();
	void recoverNode();
	int acceptChar(u8 octet);
	virtual void checkDictSize() {}

	u32 maxCodeWords;							// V.42 param N2
	int maxStringLength;						// V.42 param N7
	u32 nextCodeWord = CW_FIRST; 				// V.42 param C1
	u32 codeWordSize = CHAR_SIZE + 1;			// V.42 param C2
	u32 codeWordThreshold = 2 * ALPHABET_SIZE;	// V.42 param C3
	Dictionary dict;
	int lastAddedCode = -1;
	int matchLen = 0;
	int curCode = -1;
	bool transparent = true;
	// This flag indicates that the current codeword has already been transfered
	// and that string matching should restart (but dictionary update should proceed as normal)
	// Used for mode transitions, and flush in compressed mode
	bool modeTransition = false;
	u8 escapeCode = 0;
};

class Compressor : public V42Base
{
public:
	Compressor(u32 maxCodeWords = MIN_TOTAL_CODEWORDS, int maxStringLength = DEF_STRING_SIZE);

	void write(u8 v) override;
	void changeMode();
	void reset();

	void flush() {
		flush(false);
	}

private:
	void checkDictSize() override;
	void flush(bool enterTransparent);

	int compressTest = 0;
	int compressTestCount = 64;
	u32 outBitBuf = 0;
	int outBitPos = 0;

	void pushCode(u16 code);
};

class Decompressor : public V42Base
{
public:
	Decompressor(u32 maxCodeWords = MIN_TOTAL_CODEWORDS, int maxStringLength = DEF_STRING_SIZE);

	void write(u8 v) override;
	void reset();

private:
	void flush();

	bool escaped = false;
	u32 inputBitBuffer = 0;
	int inputBitCount = 0;
	std::vector<u8> decodeBuf;
};

} // namespace modem::42b

