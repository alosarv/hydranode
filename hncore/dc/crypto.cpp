/*
 *  Copywrite (C) Infinite  Adam Smith <hellfire00@sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Thanks to the DC++ team for this, I'm no good at algorithms just yet,
 * so code here is mostly retyped from DC++ source (don't learn with c'n'p).
 *   - Adam Smith
 */

#include <hncore/dc/directconnect.h>

namespace DC {
	namespace Crypto {

const std::string TRACE = "directconnect-crypto";

std::string huffmanEncode(std::string input) {
	using namespace std;
	using boost::format;

	string out;

	out.append("HE3\x0d");
	// Check for null input (same as DC++ check)
	// Basicly alot of the concepts here are from CryptoManager.cpp, I'd
	// like to thank the DC++ dev group for their contribution here, been a
	// very learning experience, thank you.
	if (input.size() == 0) {
		out.append(7, '\0');
		return out;
	}

	map<char, uint32_t> weights;
	char checksum = '\0';
	// Generate the weight of each byte, also checksumming here
	for (string::iterator i = input.begin(); i != input.end(); i++) {
		weights[(*i)] += 1;
		checksum ^= *i;
	}
	// FIXME: change this to use ostringstream + Utils::putVal
	out.append(1, (char)checksum);
	size_t s = input.size();
	out.append((char*)&s, 4);
	s = weights.size();
	out.append((char*)&s, 2);
	// Thats it for this section, next output is when we've done the
	// encoding table for bitlengths

	for (
		map<char, uint32_t>::iterator i  = weights.begin();
		i != weights.end(); i++
	) {
		logTrace(TRACE, format("Char: %s Weight: %d")
			% (*i).first % (*i).second);
	}

	logTrace(TRACE, "Sorting");

	list<Node*> nodes;
	for (
		map<char, uint32_t>::iterator i = weights.begin();
		i != weights.end(); i++
	) {
		Node* comnode = new Node((*i).first, (*i).second);
		// add sorting here :/
		nodes.push_back(comnode);
	}

	logTrace(TRACE, "Here goes, building node table");

	while (nodes.size() > 1) {
		Node* comnode = new Node(*nodes.begin(), *++nodes.begin());
		nodes.pop_front();
		nodes.pop_front();
		nodes.push_back(comnode);
	}

	logTrace(TRACE, "Node table complete, encoding table next");
	boost::dynamic_bitset<> stuff;
	encodeMap encodingMap;
	encodingTable(*nodes.begin(), &encodingMap, stuff);
	logTrace(TRACE, format("encodingMap size is %s") % encodingMap.size());

	// FIXME: change this to ostringstream + Utils::putVal

	boost::dynamic_bitset<> output;
	for (
		encodeMap::iterator i = encodingMap.begin();
		i != encodingMap.end(); ++i
	) {
		logTrace(TRACE, format("%s - %s") % (*i).first % (*i).second);
		// This is the char to bit size mapping
		out.append(&(*i).first, 1);
		s = (*i).second.size();
		out.append((char*)&s, 1);
		// and here is the lookups (actual bit paterns of the chars)
		for (size_t j = 0; j < encodingMap[(*i).first].size(); ++j) {
			output.push_back(encodingMap[(*i).first][j]);
		}
	}
	// Complete the last byte in the bit pattern table
	while (output.size() % 8) {
		output.push_back(0);
	}

	// Now the content! :P
	std::string encodeMe = input;
	for (
		std::string::iterator striter = encodeMe.begin();
		striter != encodeMe.end(); ++striter
	) {
		for (size_t i = 0; i < encodingMap[*striter].size(); ++i) {
			output.push_back(encodingMap[*striter][i]);
		}
	}
	//logTrace(TRACE, format("bitstream is %x (size:%d)(num_blocks:%d)")
	//	% output % output.size() % output.num_blocks());

	/*
	 * Here goes, take our dynamic_bitset and convert it to bytes and
	 * append that to the string.  The bit (128) is ORed onto the byte
	 * (which starts as null) and then bitshifted right one if the bit
	 * in the bitset is true.  Should be protocol specific, and the
	 * bitset should handle platform endianess.
	 * TODO: See if this can be done with a ostringstream + Utils::putVal
	 */
	unsigned char byte = '\0';
	uint32_t addbyte = 0;
	uint32_t position = 0;
	if (output.size() % 8) { addbyte = 1; }
	for (
		uint32_t block = 0;
		block < ((output.size() / 8) + addbyte); ++block
	) {
		for (uint32_t bits = 0; bits < 8; ++bits) {
			byte >>= 1;
			position = (block * 8) + bits;
			if (position < output.size() && output[position] == 1) {
				byte |= 128;
			}
		}

		out.append((char*)&byte, 1);
		byte = '\0';
	}

	/* This code is for decoding, add somewhere else later
	cout << "now decoding = ";
	Node* currentNode = *nodes.begin();
	for (size_t i = 0; i < output.size(); ++i) {
		if (output[i] == 0) {
			currentNode = currentNode->left;
		} else {
			currentNode = currentNode->right;
		}
		if (currentNode->chr != -1) {
			// Found char
			cout << currentNode->chr;
			currentNode = *nodes.begin();
		}
	}
	cout << endl;
	*/
	//logTrace(TRACE, format("output is %1%") % Utils::hexDump(out));
	return out;
}

void encodingTable(
	Node* currentNode, encodeMap* encodingMap, boost::dynamic_bitset<> currentCode
) {
	using boost::dynamic_bitset;
	using boost::format;

	dynamic_bitset<> left;
	dynamic_bitset<> right;
	if (currentNode->chr != -1) {
		encodingMap->insert(
			std::make_pair(currentNode->chr, currentCode)
		);

	} else {
		left = currentCode;
		left.push_back(0);
		right = currentCode;
		right.push_back(1);
		encodingTable(currentNode->left, encodingMap, left);
		encodingTable(currentNode->right, encodingMap, right);
	}
}

} // Crypto ns end
} // DC ns end
