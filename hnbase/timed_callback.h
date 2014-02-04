/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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

#ifndef __TIMED_CALLBACK_H__
#define __TIMED_CALLBACK_H__

/**
 * \file timed_callback.h Interface for timed callbacks
 */

#include <hnbase/event.h>
#include <boost/function.hpp>

namespace Utils {
	typedef std::vector<boost::intrusive_ptr<bool> > ValidatorVector;

	/**
	 * TimedCallback class, used for implementation of timedCallback()
	 * method. This class is a Singleton, and encapsulates the event table,
	 * through which the timed callbacks are done. The actual event object
	 * is the function to be called.
	 */
	class HNBASE_EXPORT TimedCallback {
	public:
		typedef std::pair<
			ValidatorVector, boost::function<void()>
		> EventType;

		DECLARE_EVENT_TABLE(Utils::TimedCallback*, EventType);

		/**
		 * \returns the single instance of this class
		 */
		static TimedCallback &instance();

	private:
		TimedCallback();
		~TimedCallback();
		TimedCallback(const TimedCallback&);
		TimedCallback& operator=(const TimedCallback&);

		//! The single instance of this class
		static TimedCallback *s_instance;

		/**
		 * Event handler for self-posted events; calls the function
		 * passed as the event object.
		 */
		void onEvent(TimedCallback *, EventType event) {
			ValidatorVector::iterator itor;

			for(
				itor = event.first.begin();
				itor != event.first.end();
				++itor
			) {
				if(!*(*itor)) {
					// object is no longer valid, give up
					return;
				}
			}

			// Invoke callback
			event.second();
		}
	};

	/**
	 * CollectValidators visits an object and obtain validators from it,
	 * directly if the object is Trackable, or indirectly (iterating
	 * though bounded objects) if object supports boost::visit_each.
	 */
	template<typename U>
	struct CollectValidators {
	private:
		/**
		 * This class implements a visitor that adds each trackable
		 * object's validator to a given vector. Is mostly an hack
		 * of bound_objects_visitor from boost/signals/trackable.hpp
		 */
		class Visitor {
		public:
			Visitor(ValidatorVector &objects)
			: m_objects(objects)
			{ }

			template<typename T>
			void operator()(const T& t) const {
				decode(t, 0);
			}

		private:
			ValidatorVector &m_objects;

			template<bool t>
			struct Bool2Type { };

			// decode() decides between a reference wrapper and
			// anything else
			template<typename T>
			void decode(
				const boost::reference_wrapper<T>& t, int
			) const {
				add_if_trackable(t.get_pointer());
			}

			template<typename T>
			void decode(const T& t, long) const {
				Bool2Type<
					(boost::is_pointer<T>::value)
				> is_a_pointer;

				maybe_get_pointer(t, is_a_pointer);
			}

			// maybe_get_pointer() decides between a pointer and a
			// non-pointer
			template<typename T>
			void maybe_get_pointer(
				const T& t, Bool2Type<true>
			) const {
				add_if_trackable(t);
			}

			template<typename T>
			void maybe_get_pointer(
				const T& t, Bool2Type<false>
			) const {
				// Take the address of this object, because the
				// object itself may be trackable
				add_if_trackable(boost::addressof(t));
			}

			// add_if_trackable() adds trackable objects to the list
			// of bound objects
			inline void add_if_trackable(const Trackable* b) const {
				if(b) {
					m_objects.push_back(b->getValidator());
				}
			}

			// ignore other pointers
			inline void add_if_trackable(const void *) const { }

			// Ignore any kind of pointer to function. I think
			// these are to ignore member functions of an object.
			template<typename R>
			inline void add_if_trackable(R (*)()) const { }

			template<typename R, typename T1>
			inline void add_if_trackable(R (*)(T1)) const { }

			template<typename R, typename T1, typename T2>
			inline void add_if_trackable(R (*)(T1, T2)) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3
			> inline void add_if_trackable(R (*)(T1, T2, T3)) const
			{ }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5,
				typename T6
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5, T6)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5,
				typename T6, typename T7
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5, T6, T7)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5,
				typename T6, typename T7, typename T8
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5, T6, T7, T8)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5,
				typename T6, typename T7, typename T8,
				typename T9
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5, T6, T7, T8, T9)
			) const { }

			template<
				typename R, typename T1, typename T2,
				typename T3, typename T4, typename T5,
				typename T6, typename T7, typename T8,
				typename T9, typename T10
			> inline void add_if_trackable(
				R (*)(T1, T2, T3, T4, T5, T6, T7, T8, T9, T10)
			) const { }
		};

	public:
		/**
		 * Constructor
		 *
		 * @param u          The object to be inspected
		 * @param validators Reference to a vector<intrusive_ptr<bool> >,
		 *                   where found validators are added.
		 */
		CollectValidators(const U& u, ValidatorVector &validators) {
			Visitor v(validators);
			boost::visit_each(v, u, 0);
		}
	};

	/**
	 * Request a function to be called after a specific timeout.
	 *
	 * @param fun          Function to be called
	 * @param timeout      Delay in milliseconds, after which the function
	 *                     shall be called.
	 *
	 * \note The function will only be called once.
	 */
	template<typename Fun>
	inline void timedCallback(Fun fun, uint32_t timeout) {
		ValidatorVector validators;

		CollectValidators<Fun>(fun, validators);

		TimedCallback::instance().getEventTable().postEvent(
			&TimedCallback::instance(),
			std::make_pair(
				validators,
				boost::function<void ()>(fun)
			),
			timeout
		);
	}

	template<typename T>
	inline void timedCallback(T *obj, void (T::*fun)(), uint32_t timeout) {
		timedCallback(boost::bind(fun, obj), timeout);
	}
}

#endif
