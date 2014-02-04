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
 * \file object.h Interface for Object class
 */

#ifndef __OBJECT_H__
#define __OBJECT_H__

/**
 * \page dsh Data Structures Hierarchy
 *
 * \section Overview
 *
 * This is, basically, the center of Module/GUI Communication, however is also
 * used by the Core itself. An object is a container which can contain other
 * objects, as well as arbitary data. An object knows about its parent and
 * shall notify the parent upon events happening within itself. An object shall
 * also relay messages coming from deeper within the object hierarchy upwards.
 * Upon events, either internal or coming from lower levels, an object notifies
 * all of its watchers before passing the event up the hierarchy.
 *
 * \section Rationale
 *
 * The basic problem regarding Hydranode Core/GUI Communication is that we need
 * to relay arbitary data up to the user interface which we do not know the
 * specifics about. The data is defined and implemented within loaded modules
 * to which the Core Framework has no access. The intent is also to keep the
 * user interface oblivious about the modules data structures specifications,
 * while allowing the structures to be displayed, modified and interacted with.
 *
 * \section Implementation
 *
 * In order to achieve the afore-mentioned requirements, an Object class
 * implements the following characteristics:
 *
 * - It keeps a pointer to its parent object. The pointer may be null in case
 *   of top-level parent node.
 * - It keeps a unique identifier which refers to this concrete object. The
 *   identifier is assigned to the object by its parent when the object is
 *   added to the hierarchy.
 * - It keeps a string member which may be displayed to the user to allow the
 *   user identify the object.
 * - It keeps a list of callback functions which are to be called whenever there
 *   are events within this object, or below it in the hierarchy. This allows
 *   user interfaces to perform real-time updates of the data structure changes.
 *
 * The default constructor of the Object is not allowed. An Object is required
 * to have a parent, thus placing it immediately within the data structures
 * hierarchy upon construction.
 *
 * The only allowed constructor, as well as the destructor of Object are
 * protected, only accessible from derived classes. This enforces the
 * implementation to use only objects derived from Object in the hierarchy,
 * effectivly forbidding base class construction. It also disallows deleting
 * the Object pointers returned by traversing the hierarchy, since that would
 * give control of the data structures to outside world, which is a threat
 * to the original implementer of the data structure (e.g. the module).
 *
 * \section Requirements
 *
 * If an object contains other objects which in turn contain data members,
 * the Core/GUI Communication Protocol requires that the data fields names
 * for all of the contained objects are same. This means an object is required
 * to contain only objects of same type, thus with same data members. This
 * requirement is not enforced by the implementation of the base class due to
 * impelemtation complications, however is required to be enforced by derived
 * classes. Objects containing mixed set of objects of different types is not
 * allowed.
 *
 * \section Usage
 *
 * The module wishing to make its data structures available to the user
 * interface must first derive the container class from Object, and also
 * the actual data structure to be exported from Object. Modules are required
 * to place their data structures below their entry class. In order to make
 * the implemented structures available to the interface, the implementor
 * should override the virtual functions defined in the base class. A sample
 * implementation can be found in the Core Framework test-suite, at
 * tests/test-object/test-object.cpp.
 *
 * If the implemented data structure changes, the implementor is recommended
 * to notify the user interface through the notify() member function. This
 * allows the user interface to react to changes in the structure, allowing
 * real-time updates.
 *
 * \section Remarks
 *
 * The parent/child relationship is strong, e.g. the parent owns the child.
 * Additionally, the child always notifies its parent about its destruction,
 * thus deletion of a child is a safe operation - the child is guaranteed to
 * be removed from its parent. The result of this is that the implementors
 * of specific data structures in modules are no longer required to keep any
 * pointers or manage the lifetime of the contained objects (unless explicitly
 * needed).
 */

#include <hnbase/osdep.h>
#include <hnbase/event.h>
#include <hnbase/fwd.h>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/signals.hpp>

//! Object identifiers are 32-bit unsigned integers
typedef uint32_t ObjectId;

/**
 * Data type description
 */
enum DataType {
	ODT_UNKNOWN   = 0x00,             //!< 00000000  unknown
	ODT_INT       = 0x01,             //!< 00000001  integer
	ODT_STRING    = 0x02,             //!< 00000010  string
	ODT_BOOL      = 0x04,             //!< 00000100  boolean
	ODT_CHOICE    = 0x08,             //!< 00001000  choice
	ODT_RESERVED1 = 0x10,             //!< 00010000  <reserved for future>
	ODT_REQUIRED  = 0x20,             //!< 00100000  required/always present
	ODT_READONLY  = 0x40,             //!< 01000000  read-only
	ODT_UNSIGNED  = 0x80              //!< 10000000  unsigned
};

//! Base object class
class HNBASE_EXPORT Object : public boost::noncopyable {
public:
	DECLARE_EVENT_TABLE(Object*, ObjectEvent);
	class Operation;

	//! @name Accessors for internal data members.
	//@{
	//! Retrieve the name of this object
	std::string getName() const;
	//! Retrieve the unique identifier for this object
	ObjectId getId() const { return m_id; }
	//! Rename this object
	void setName(const std::string &name);
	//! Retrieve the parent of this object
	Object* getParent() const;
	//! Change the parent of this object
	void setParent(Object *newParent);
	//! Retrieve the number of children this object has
	uint32_t getChildCount() const;
	//! Retrieve a specific child
	Object* getChild(uint32_t id) const;
	//! Check if this object has children
	bool hasChildren() const;
	//@}

	//! @name Customization
	//! Modules deriving their objects from this class in order to make
	//! them available to user interface can override any of these virtual
	//! functions to describe themselves to the user interface, allow access
	//! to internal data structures and perform operations with themselves.
	//@{
	//! Retrieve the number of data fields within this object
	virtual uint32_t getDataCount() const;
	//! Retrieve the data at specific field
	virtual std::string getData(uint32_t num) const;
	//! Retrieve the data field name
	virtual std::string getFieldName(uint32_t num) const;
	//! Retrieve the data type of the field
	virtual DataType getFieldType(uint32_t num) const;
	//! Fill the passed vector with possible values for this field.
	//! Generally used in conjuction with FieldType OFT_CHOICE
	virtual void getValueChoices(std::vector<std::string> *cont) const;
	//! Retrieve the number of operations for this object
	virtual uint32_t getOperCount() const;
	//! Retrieve a specific operation description
	virtual Operation getOper(uint32_t n) const;
	//! Perform an operation with this object
	virtual void doOper(const Operation &oper);
	//! Modify the data at specific field
	virtual void setData(uint32_t num, const std::string &data);
	//@}

	//! Internal map constant iterator
	typedef std::map<ObjectId, Object*>::const_iterator CIter;

	//! @name Iterators and accessors for our internal data tree for easier
	//! navigation. The iterators are const to avoid outside world modifying
	//! our internal structures.
	//@{
	/**
	 * \returns Iterator to the beginning of our internal map
	 */
	CIter begin() const;
	/**
	 * \returns Iterator to the one-past-end of our internal map
	 */
	CIter end() const;
	//@}

	/**
	 * This signal is emitted whenever this object, or any object beneath
	 * it is changed, added or removed. The name for this signal has been
	 * chosen to avoid collisions with (possible) derived object variables,
	 * since this is public.
	 */
	boost::signal<void (Object*, ObjectEvent)> objectNotify;

	/**
	 * Searches this objects children for an object. This is faster than
	 * using the static findObject() method due to the smaller size of the
	 * map.
	 *
	 * @param id        Id to search for
	 * @returns         Pointer to object if found, null if not found
	 */
	Object* findChild(ObjectId id) const;

	//! Static accessors for locating an object with a specific identifier
	//! from the entire hierarchy. May return null if the object could not
	//! be found.
	static Object* findObject(ObjectId id);

	/**
	 * Operation is a "command" one can perform with an object. An operation
	 * has a name and optional list of arguments the command supports.
	 */
	class HNBASE_EXPORT Operation {
	public:
		class Argument;

		/**
		 * This constructor should be used by modules to describe an
		 * operation to an user interface. It may optionally be follwed
		 * by argument additions through addArgument() method.
		 *
		 * @param name                         Name of the command
		 * @param requiresArgs                 If arguments are required
		 */
		Operation(const std::string &name, bool requiresArgs)
		: m_name(name), m_requiresArgs(requiresArgs) {}

		/**
		 * This constructor should be used by UI parser to construct
		 * a command to be sent to an object.
		 *
		 * @param name                         Name of the command
		 * @param args                         Optional argument list
		 */
		Operation(const std::string &name, std::vector<Argument> args)
		: m_name(name), m_argList(args) {}

		//! Destructor
		~Operation() {}

		/**
		 * Add an argument to this operation.
		 *
		 * @param arg                          Argument to be added
		 */
		void addArg(const Argument &arg) {
			m_argList.push_back(arg);
		}

		//! @name Accessors
		//@{
		uint32_t    getArgCount()  const { return m_argList.size(); }
		std::string getName()      const { return m_name;           }
		bool        requiresArgs() const { return m_requiresArgs;   }
		Argument getArg(uint32_t num) const { return m_argList.at(num);}
		//@}

		/**
		 * Get argument by name
		 *
		 * @param name                         Argument to retrieve
		 * @return                             The argument, if found
		 *
		 * \throws std::runtime_error if argument is not found
		 */
		Argument getArg(const std::string &name) const {
			std::vector<Argument>::const_iterator i;
			i = m_argList.begin();
			while (i != m_argList.end()) {
				if ((*i).getName() == name) {
					return *i;
				}
				++i;
			}
			throw std::runtime_error("Argument not found.");
		}

		/**
		 * Argument class represents one argument that can be passed
		 * to an operation. An argument has a name, type, and boolean
		 * indicating whether this argument is always required to be
		 * present in an operation.
		 */
		class HNBASE_EXPORT Argument {
		public:
			/**
			 * This constructor should be used by modules to add
			 * an argument to an operation being described.
			 *
			 * @param name                 Name of the argument
			 * @param required             If argument is required
			 * @param type                 Type of argument's data
			 */
			Argument(
				const std::string &name, bool required,
				DataType type
			) : m_name(name), m_required(required), m_type(type) {}

			/**
			 * This constructor should be used by UI parser to
			 * construct an argument to be added to an operation
			 * which is then passed to an object.
			 *
			 * @param name                 Name of the argument
			 * @param value                Value of the argument
			 *
			 * \note The type of argument is intentionally left
			 *       undefined here, since it would add nothing
			 *       for the creator, and the object receiving the
			 *       argument checks the type internally anyway.
			 */
			Argument(
				const std::string &name,
				const std::string &value
			) : m_name(name), m_value(value) {}

			//! Dummy destructor
			~Argument() {}

			//! @name Accessors
			//@{
			std::string getValue()   const { return m_value;    }
			std::string getName()    const { return m_name;     }
			DataType    getType()    const { return m_type;     }
			bool        isRequired() const { return m_required; }
			//@}
		private:
			std::string m_name;      //!< Name of the argument
			bool m_required;         //!< If this is required
			DataType m_type;         //!< Argument value type
			std::string m_value;     //!< Argument value
		};
	private:
		friend class Object;
		//!< Default constructor for internal usage only
		Operation() {}
		std::string m_name;              //!< Name of the operation
		bool m_requiresArgs;             //!< If arguments are required
		std::vector<Argument> m_argList; //!< Argument list
	};

protected:
	/**
	 * Only allowed constructor.
	 *
	 * @param parent       Pointer to the parent object
	 * @param name         Optional name for this object
	 */
	Object(Object *parent, const std::string &name = "");

	/**
	 * Protected destructor.
	 */
	virtual ~Object();

	/**
	 * Notify about an event. The event is propagated up the objects
	 * hierarchy, and passed to all watchers of this object and all its
	 * parent objects.
	 *
	 * @param source        Event source object
	 * @param event         Event type
	 */
	void notify(Object *source, ObjectEvent event);

	/**
	 * More convenient function for derived classes - no need to pass
	 * source, and event type has convenient default value.
	 */
	void notify(ObjectEvent event = OBJ_MODIFIED);

	/**
	 * Enable notifications propagation up the hierarchy
	 * (default is enabled)
	 */
	static void enableNotify();

	/**
	 * Disable notifications propagation up the hierarchy
	 */
	static void disableNotify();

	/**
	 * @name Convenience methods for derived classes.
	 * Override these functions to receive notifications about
	 * the object's contents changes.
	 */
	//@{
	//! A child has been added.
	virtual void onChildAdded(ObjectId id);
	//! A child has been removed (either deleted or reparented)
	virtual void onChildRemoved(ObjectId id);
	//@}
private:
	//! Private iterator for the internal data container
	typedef std::map<ObjectId, Object*>::iterator Iter;

	//! Default constructor is private - Object needs parent for
	//! construction to avoid having Object's around w/o parents.
	Object();

	//! @name Copying is not allowed
	//@{
	Object(const Object&);
	Object& operator=(Object&);
	//@}

	//! Pointer to the parent of this object
	Object *m_parent;

	//! Optional name of this object
	std::string m_name;

	//! Unique identifier, assigned by parent upon addition
	ObjectId m_id;

	//! Map of all child objects
	std::map<ObjectId, Object*> *m_children;

	//! @name Internal usage functions. These are private on purpose.
	//@{
	//! Add a new child.
	void addChild(Object *child);
	//! Remove a child
	void delChild(Object *child);
	//@}

	//! Contains all identifiers currently in use and the object they
	//! refer to.
	static std::map<ObjectId, Object*> s_usedIds;

	//! Whether to propagate notifications up the hierarchy
	//! Disabling this speeds up application startup/shutdown
	//! @see disableNotify() / enableNotify()
	static bool s_notify;
};

/**
 * Bottom line -> This is what the final data structures hierarchy might look
 * like:
	Hydranode
	   +-- MetaDb
	   |      +-- MetaData
	   |      |      +-- File Size
	   |      |      +-- Modification date
	   |      |      +-- File Names
	   |      |      |       |-- FileName[0]
           |      |      |       |-- FileName[1]
	   |      |      |       |-- FileName[2]
	   |      |      +-- HashSets
	   |      |      |       +-- ED2K
	   |      |      |       |    + PartSize (9728000)
	   |      |      |       |    + FileHash
	   |      |      |       |    + PartHashes
	   |      |      |       |          + PartHash[0]
	   |      |      |       |          + PartHash[1]
	   |      |      |       |          + PartHash[2]
	   |      |      |       +-- SHA1
	   |      |      |            + PartSize (0)
	   |      |      |            + FileHash
	   |      |      +-- AudioMetaData
	   |      |               + Title
	   |      |               + Album
	   |      |               + Performer
	   |      +-- MetaData
	   |             + File Size
	   |             ...
	   |      ...
	   +-- SharedFilesList
	   |       +-- SharedFile
	   |       |       +-- FileSize
	   |       |       +-- PartData
	   |       |              +-- PartData
	   |       |                     +-- Completed
	   |       |                     +-- Verified
	   |       +-- SharedFile
	   |           ...
	   +-- Modules
	         +-- ED2K
		 |     +-- ServerList
		 |     |        +-- Server
		 |     |        |     +-- IP
		 |     |        |     +-- Port
		 |     |        +-- Server
		 |     +-- CreditsDb
		 |              +-- ClientCredits
		 |              |        +-- Uploaded
		 |              |        +-- Downloaded
		 |              |        +-- Hash
		 |              |        +-- PubKey
		 |              +-- ClientCredits
		 |                       ...
		 +-- BitTorrent
		 |      +-- ...
		 +-- gnutella
		 |      +-- ...
		 +-- ...


*/

#endif // !__OBJECT_H__
