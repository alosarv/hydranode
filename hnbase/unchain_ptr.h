/* Copyright 2005-2006 Alo Sarv
 * Distributed under the Boost Software Licence, Version 1.0
 * (See accompanying file LICENCE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef __UNCHAIN_PTR_H__
#define __UNCHAIN_PTR_H__

#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/type_traits.hpp>
#include <boost/mpl/if.hpp>

namespace Utils {
namespace detail {

template<typename Class>
struct unchainptr_impl {
	// non-const overloads
	Class& operator()(Class &x) const { return x; }

	template<typename ChainedPtr>
	Class& operator()(ChainedPtr &x) const {
		return operator()(*x);
	}

	Class& operator()(boost::reference_wrapper<Class> &x) const {
		return operator()(x.get());
	}
};

template<typename Class>
struct unchainptr_const_impl {
	// const overloads
	const Class& operator()(const Class &x) const { return x; }

	template<typename ChainedPtr>
	const Class& operator()(const ChainedPtr &x) const {
		return operator()(*x);
	}

	const Class& operator()(
		const boost::reference_wrapper<const Class> &x
	) const {
		return operator()(x.get());
	}
};

template<typename T>
struct get_type {
	typedef T type;
};
template<typename T>
struct get_type<T*> {
	typedef typename get_type<T>::type type;
};
template<typename T>
struct get_type<T *const> {
	typedef typename get_type<T>::type type;
};
template<typename T>
struct get_type<T&> {
	typedef typename get_type<T>::type type;
};
template<typename T>
struct get_type<boost::shared_ptr<T> > {
	typedef typename get_type<T>::type type;
};
template<typename T>
struct get_type<boost::weak_ptr<T> > {
	typedef typename get_type<T>::type type;
};
template<typename T>
struct get_type<boost::intrusive_ptr<T> > {
	typedef typename get_type<T>::type type;
};

} // end namespace detail

/**
 * Unchains any pointer (even smart and nested) type, and returns a reference
 * to the real type. Const-correctness is preserved, thus passing a pointer to
 * const object and expecting non-const reference in return is an error.
 *
 * @param ptr        Pointer expected to be unchained. Non-pointers are returned
 *                   unmodified.
 * @returns          Class& or const Class&, depending on input parameters.
 */
template<typename ChainedPtr>
inline typename detail::get_type<ChainedPtr>::type&
unchain_ptr(ChainedPtr ptr) {
	return typename boost::mpl::if_<
		boost::is_const<ChainedPtr>,
		detail::unchainptr_const_impl<
			typename detail::get_type<ChainedPtr>::type
		>,
		detail::unchainptr_impl<
			typename detail::get_type<ChainedPtr>::type
		>
	>::type()(ptr);
}

} // end namespace Utils

#endif
