/* Copyright 2005-2006 Alo Sarv
 * Distributed under the Boost Software Licence, Version 1.0
 * (See accompanying file LICENCE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/mpl/assert.hpp>
#include <hnbase/unchain_ptr.h>
#include <iostream>
using namespace Utils;

// compile-time checks for detail::get_type template
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int*>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int**>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int***>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int****>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int*&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int**&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int***&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int****&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int *const>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int *const&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<int **const&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int*>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int*&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int *const>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int *const&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<const int, detail::get_type<const int **const&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int> >::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int>&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int>*>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int>*&>::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int*> >::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::shared_ptr<int*&> >::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::weak_ptr<int> >::type> ));
BOOST_MPL_ASSERT(( boost::is_same<int, detail::get_type<boost::intrusive_ptr<int> >::type> ));

// calls const function on type
template<typename T>
void process1(T t) {
	unchain_ptr(t).foo();
}

// calls non-const function on type
template<typename T>
void process2(T t) {
	unchain_ptr(t).bar();
}

struct X {
	void foo() const { std::cerr << "foo()" << std::endl; }
	void bar() { std::cerr << "bar()" << std::endl; }
};

struct nullDeleter {
	template<typename T>
	void operator()(T t) {}
};

int main() {
	X t1;
	process1(t1);
	process2(t1);
	X *t2 = new X;
	process1(t2);
	process2(t2);
	X **t3 = &t2;
	process1(t3);
	process2(t3);
	boost::shared_ptr<X> t4(new X);
	process1(t4);
	process2(t4);
	boost::shared_ptr<const X> t5(new X);
	process1(t5);
	boost::shared_ptr<X*> t6(&t2, nullDeleter());
	process1(t6);
	process2(t6);
}
