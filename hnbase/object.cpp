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
 * \file object.cpp Implementation of Object (and related) classes
 */


#include <hnbase/pch.h>
#include <hnbase/object.h>                     // Interface declarations
#include <hnbase/log.h>                        // For logging functions

// Object class
// ------------
IMPLEMENT_EVENT_TABLE(Object, Object*, ObjectEvent);

// Static members
std::map<ObjectId, Object*> Object::s_usedIds;
bool Object::s_notify = false;

// Construction and destruction logic
// On construction, we self-register ourselves with our parent (if existing).
Object::Object(Object *parent, const std::string &name /* = "" */)
: m_parent(parent), m_name(name), m_id(), m_children() {
#ifndef NDEBUG // Generator testing
	uint32_t tryCount = 0;
	// generate new unique identifier
	do {
		m_id = Utils::getRandom();
		++tryCount;
	} while (s_usedIds.find(m_id) != s_usedIds.end());
	if (tryCount > 1) {
		logTrace(
			TRACE_OBJECT, boost::format(
				"Took %d trys to generate new identifier."
			) % tryCount
		);
	}
#else
	do {
		m_id = Utils::getRandom();
	} while (s_usedIds.find(m_id) != s_usedIds.end());
#endif
	s_usedIds.insert(std::make_pair(m_id, this));

	if (m_parent) {
		m_parent->addChild(this);
	}
}

// On destruction, first call the destructors of all our children, and then
// have parent remove us.
Object::~Object() {
	notify(this, OBJ_DESTROY);
	if (m_parent) {
		m_parent->delChild(this);
	}

	std::map<ObjectId, Object*>::iterator j = s_usedIds.find(m_id);
	if (j == s_usedIds.end()) {
		logDebug(
			"Internal Object error: Deleting child, but identifier "
			"could not be found."
		);
		return;
	}
	s_usedIds.erase(j);
}

// Data and operations getters
std::string Object::getName() const {
	return m_name;
}
/*virtual*/ uint32_t Object::getDataCount() const {
	return 0;
}
/*virtual*/ std::string Object::getData(uint32_t) const {
	logTrace(
		TRACE_OBJECT,
		boost::format("%s: Object does not have data.") % m_name
	);
	return std::string();
}
/*virtual*/ std::string Object::getFieldName(uint32_t) const {
	logTrace(
		TRACE_OBJECT,
		boost::format("%s: Object does not heave header information.")
		% m_name
	);
	return std::string();
}
DataType Object::getFieldType(uint32_t) const { return ODT_UNKNOWN; }
void Object::getValueChoices(std::vector<std::string>*) const {}
/*virtual*/ uint32_t Object::getOperCount() const {
	return 0;
}
/*virtual*/ Object::Operation Object::getOper(uint32_t) const {
	logTrace(TRACE_OBJECT,
		boost::format("%s: Object does not have operations.") % m_name
	);
	return Object::Operation();
}

// Data and operations setters
void Object::setName(const std::string &name) {
	m_name = name;
}
/*virtual*/ void Object::doOper(const Object::Operation&) {
	logTrace(TRACE_OBJECT,
		boost::format("%s: Object does not have operations.") % m_name
	);
}
/*virtual*/ void Object::setData(uint32_t, const std::string&) {
	logTrace(
		TRACE_OBJECT,
		boost::format("%s: Object does not have data.") % m_name
	);
}

// Hierarchy traversing
Object* Object::getParent() const { return m_parent; }
uint32_t Object::getChildCount() const {
	if (m_children) {
		return m_children->size();
	}
	return 0;
}
Object* Object::getChild(ObjectId id) const {
	if (m_children) {
		CIter i = m_children->find(id);
		if (i == m_children->end()) {
			return 0;
		} else {
			return (*i).second;
		}
	}
	return 0;
}
bool Object::hasChildren() const {
	if (m_children) {
		return m_children->size() > 0;
	}
	return false;
}

// Hierarchy modifying
void Object::setParent(Object *newParent) {
	if (m_parent) {
		m_parent->delChild(this);
	}
	m_parent = newParent;
	if (newParent) {
		newParent->addChild(this);
	}
}

// Add new child. We generate a new ID for the child which is unique to us
// (however, not unique to the rest of the world).
void Object::addChild(Object *child) {
	if (!m_children) {
		m_children = new std::map<ObjectId, Object*>;
	}

	std::pair<Iter, bool> ret;
	ret = m_children->insert(std::make_pair(child->m_id, child));
	if (ret.second == false) {
		logDebug(
			"Internal Object error: Already have child with "
			"this ID."
		);
	} else {
		logTrace(
			TRACE_OBJECT,
			boost::format("Added child %s(%d) to %s")
			% child->m_name % child->m_id % m_name
		);
	}
	notify(child, OBJ_ADDED);
	// Virtual function call
	onChildAdded(child->m_id);
}

// Remove a child. Note that we do NOT call delete on the child - this function
// is private and is called only from Object destructor - calling delete here
// would cause double deleting.
void Object::delChild(Object *child) {
	if (!m_children) {
		logDebug(
			"Internal Object error: Attempt to delete a child "
			"without any childs listed."
		);
		return;
	}

	Iter i = m_children->find(child->m_id);
	if (i == m_children->end()) {
		logDebug("Object: No such child.");
		return;
	}
	m_children->erase(i);
	if (!m_children->size()) {
		delete m_children;
		m_children = 0;
	}
	notify(child, OBJ_REMOVED);
	// Virtual function call
	onChildRemoved(child->m_id);
}

// Notifications
void Object::notify(Object* source, ObjectEvent event) {
	if (s_notify) {
		objectNotify(source, event);
	}
	if (m_parent && s_notify) {
		m_parent->notify(source, event);
	}
}

void Object::notify(ObjectEvent event) {
	notify(this, event);
}

// iterator accessors. Note the return values are constant iterators - this
// is to protect our internal structures from unwanted modifications.
Object::CIter Object::begin() const {
	if (m_children) {
		return m_children->begin();
	} else {
		return s_usedIds.end();
	}
}

Object::CIter Object::end() const {
	if (m_children) {
		return m_children->end();
	} else {
		return s_usedIds.end();
	}
}

// global finder - handy way to quickly find objects w/o surfing through the
// entire hierarchy
/*static*/ Object* Object::findObject(ObjectId id) {
	std::map<ObjectId, Object*>::iterator i = s_usedIds.find(id);
	if (i == s_usedIds.end()) {
		return 0;
	} else {
		return (*i).second;
	}
}

Object* Object::findChild(ObjectId id) const {
	if (m_children) {
		CIter i = m_children->find(id);
		if (i != m_children->end()) {
			return (*i).second;
		}
	}
	return 0;
}

// Convenience functions
void Object::onChildAdded(ObjectId) {}
void Object::onChildRemoved(ObjectId) {}

void Object::enableNotify() { s_notify = true; }
void Object::disableNotify() { s_notify = false; }
