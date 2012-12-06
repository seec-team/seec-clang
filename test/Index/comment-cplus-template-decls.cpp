// RUN: rm -rf %t
// RUN: mkdir %t
// RUN: c-index-test -test-load-source all -comments-xml-schema=%S/../../bindings/xml/comment-xml-schema.rng -target x86_64-apple-darwin10 std=c++11 %s > %t/out
// RUN: FileCheck %s < %t/out

// Ensure that XML we generate is not invalid.
// RUN: FileCheck %s -check-prefix=WRONG < %t/out
// WRONG-NOT: CommentXMLInvalid
// rdar://12378714

/**
 * \brief Aaa
*/
template<typename T> struct A {
/**
 * \brief Bbb
*/
  A();
/**
 * \brief Ccc
*/
  ~A();
/**
 * \brief Ddd
*/
  void f() { }
};
// CHECK: <Declaration>template &lt;typename T&gt; struct A {\n}</Declaration>
// CHECL: <Declaration>A&lt;T&gt;()</Declaration>
// CHECK: <Declaration>void ~A&lt;T&gt;()</Declaration>

/**
 * \Brief Eee
*/
template <typename T> struct D : A<T> {
/**
 * \brief
*/
  using A<T>::f;
  
  void f();
};
// CHECK: <Declaration>template &lt;typename T&gt; struct D :  A&lt;T&gt; {\n}</Declaration>
// CHECK: <Declaration>using A&lt;T&gt;::f</Declaration>
