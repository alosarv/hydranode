/*
 *  Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

/**
 * \file tag.cpp Implementation of Tag class
 */

#include <hncore/ed2k/tag.h>
#include <hnbase/utils.h>
#include <hnbase/log.h>

namespace Donkey {

static const std::string TRACE_TAG("ed2k.tag");

// Tag class
Tag::Tag(std::istream &i) : m_opcode(0), m_valueType(0) {
	uint32_t offset = i.tellg();            // Needed for exception throwing
	logTrace(TRACE_TAG,
		boost::format(" * Parsing tag at offset %d")
		% Utils::hexDump(offset)
	);
	m_valueType = Utils::getVal<uint8_t>(i);
	if (m_valueType & 0x80) {
		m_valueType &= 0x7f;
		m_opcode = Utils::getVal<uint8_t>(i);
		if ((m_valueType > 0x10) && (m_valueType -= 0x10)) {
			m_value = Utils::getVal<std::string>(
				i, m_valueType
			).value();
			m_valueType = TT_STRING;
			logTrace(TRACE_TAG,
				boost::format("   NewTag parsing complete: %s")
				% dump()
			);
			return;
		}
	} else {
		uint16_t len = Utils::getVal<uint16_t>(i);
		if (len == 1) {
			m_opcode = Utils::getVal<uint8_t>(i);
		} else {
			m_name = Utils::getVal<std::string>(i, len);
		}
	}
	logTrace(TRACE_TAG,
		boost::format("   Tag header parsed: %s") % dump(false)
	);
	switch (m_valueType) {
		case TT_UINT8:
			m_value = static_cast<uint32_t>(
				Utils::getVal<uint8_t>(i)
			);
			break;
		case TT_UINT16:
			m_value = static_cast<uint32_t>(
				Utils::getVal<uint16_t>(i)
			);
			break;
		case TT_UINT32:
			m_value = Utils::getVal<uint32_t>(i).value();
			break;
		case TT_STRING: {
			uint16_t len = Utils::getVal<uint16_t>(i);
			m_value = Utils::getVal<std::string>(i, len).value();
			break;
		}
		case TT_FLOAT:
			m_value = Utils::getVal<float>(i).value();
			break;
		case TT_BOOL:
			i.seekg(1, std::ios::cur);
			break;
		case TT_BOOLARR:
			i.seekg(Utils::getVal<uint16_t>(i), std::ios::cur);
			break;
		case TT_BLOB:
			i.seekg(Utils::getVal<uint16_t>(i), std::ios::cur);
			break;
		case TT_HASH:
			i.seekg(16, std::ios::cur);
			break;
		default:
			throw TagError(
				boost::format(
					"invalid valuetype %s at offset %s"
				) % Utils::hexDump(m_valueType)
				% Utils::hexDump(offset)
			);
	}
	logTrace(TRACE_TAG,
		boost::format("   Tag parsing complete. Tag data: %s") % dump()
	);
}

std::ostream& operator<<(std::ostream &o, const Tag &t) {
	Utils::putVal<uint8_t>(o, t.m_valueType);
	Utils::putVal<uint16_t>(o, t.m_name.size() ? t.m_name.size() : 1);
	if (t.m_name.size()) {
		Utils::putVal<std::string>(o, t.m_name, t.m_name.size());
	} else {
		Utils::putVal<uint8_t>(o, t.m_opcode);
	}
	using boost::any_cast;
	switch (t.m_valueType) {
		case Tag::TT_UINT8:
			Utils::putVal<uint8_t>(o, any_cast<uint8_t>(t.m_value));
			break;
		case Tag::TT_UINT16:
			Utils::putVal<uint16_t>(
				o, any_cast<uint16_t>(t.m_value)
			);
			break;
		case Tag::TT_UINT32:
			Utils::putVal<uint32_t>(
				o, any_cast<uint32_t>(t.m_value)
			);
			break;
		case Tag::TT_STRING: {
			uint16_t len = any_cast<std::string>(t.m_value).size();
			Utils::putVal<uint16_t>(o, len);
			Utils::putVal<std::string>(
				o, any_cast<std::string>(t.m_value), len
			);
			break;
		}
		case Tag::TT_FLOAT:
			Utils::putVal<float>(o, any_cast<float>(t.m_value));
			break;
		case Tag::TT_BOOL:
		case Tag::TT_BOOLARR:
		case Tag::TT_BLOB:
		case Tag::TT_HASH:
		default:
			throw TagError(
				boost::format("writing: invalid valuetype %s")
				% Utils::hexDump(t.m_valueType)
			);
	}
	return o;
}

std::string Tag::dump(bool data) const {
	using Utils::hexDump;
	using boost::any_cast;

	boost::format fmt("type=%1% %2%=%3% valueType=%4% value=%5%");

	fmt % hexDump(m_valueType) % (m_opcode ? "opcode" : "name");
	fmt % (m_opcode ? hexDump(m_opcode) : m_name) % hexDump(m_valueType);
	if (data) {
		switch (m_valueType) {
			case TT_UINT8:
			case TT_UINT16:
			case TT_UINT32:
				try {
					fmt % any_cast<uint32_t>(m_value);
				} catch (boost::bad_any_cast&) {
					fmt % "<unknown value>";
				}
				break;
			case TT_FLOAT:
				try {
					fmt % any_cast<float>(m_value);
				} catch (boost::bad_any_cast&) {
					fmt % "<unknown value>";
				}
				break;
			case TT_STRING:
				try {
					fmt % hexDump(
						any_cast<std::string>(m_value)
					);
				} catch (...) {
					fmt % "<unknown value>";
				}
				break;
			default:
				fmt % "<unknown-type>";
				break;
		}
	} else {
		fmt % "<incomplete>";
	}
	return fmt.str();
}

void warnUnHandled(const std::string &loc, const Tag &t) {
	using boost::format;
	logTrace(
		TRACE_TAG,
		format("Unhandled tag found in %1%: %2%") % loc % t.dump()
	);
}

} // end namespace Donkey
