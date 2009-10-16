#if !defined(CODE_GOOGLE_COM_P_V8_V8_BIND_H_INCLUDED)
#define CODE_GOOGLE_COM_P_V8_V8_BIND_H_INCLUDED 1
////////////////////////////////////////////////////////////////////////
//    A mini-framework for binding C++ natives to v8 (JavaScript engine)
//    script-side objects in a type-safe manner (the v8 API only covers
//    (void *) natively).
//
//    v8: http://code.google.com/p/v8/
//
//    Author: Stephan Beal (http://wanderinghorse.net/home/stephan/)
//
//    License: same as v8 (see below)
////////////////////////////////////////////////////////////////////////
// Copyright 2007-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////

//#include <cstdint> // arg! Requires C++0x!
//#include <stdint.h> // hope the client's platform is recent!
#include <v8.h>
#include <string>
#include <map>
/**
   The v8 namespace is the primary namespace of Google's v8 engine.
*/
namespace v8 {
namespace juice {
/**
   The bind namespace holds functions and types for binding JS-side values
   to native values in a type-safe manner. For starters, see BindNative().
*/
namespace bind {
    using namespace v8;

    /**
       Convenience typedef.
    */
    typedef Handle<Value> ValueHandle;

    /**
       The "bind key" type used by the BindNative() family of
       functions.
    */
    typedef void const * BindKeyType;

#if ! defined(DOXYGEN)
    /**
       Internal library details.
    */
    namespace Detail
    {
	/**
	   Utility type for stripping qualifications from types for
	   some template algos.
	   
	   Specializations ensure that type_stripper<T>::type
	   is always T, unqualified.
	*/
	template <typename T>
	struct type_stripper
	{
	    typedef T type;
	};
	template <typename T>
	struct type_stripper<T &>
	{
	    typedef T type;
	};
	template <typename T>
	struct type_stripper<T const &>
	{
	    typedef T type;
	};
	template <typename T>
	struct type_stripper<T *>
	{
	    typedef T type;
	};
	template <typename T>
	struct type_stripper<T const *>
	{
	    typedef T type;
	};
        /**
           A type for holding metadata for
           JS-to-Native bindings.

           NativeType_ is a non-cv-qualified type.
           It may be pointer-qualified, and internally
           this type maps only to pointers.
        */
        template <typename NativeType_>
        struct BinderMetadata
        {
            /** Binder key type. */
            typedef void const * KeyType;
            /** Binder "raw" native type. */
            typedef NativeType_ NativeType;
            /**
               Bound native type - always pointer-qualified NativeType.
             */
            typedef typename Detail::type_stripper<NativeType>::type * NativePtr;
            /**
               This object's key.
            */
            KeyType key;
            /**
               This object's value.
             */
            NativePtr value;
            /**
               Initializes an empty binding.
            */
            BinderMetadata() : key(0), value(0)
            {}
            /**
               Initializes a binding with the given key/value pair.
               Does NOT insert that binding anywhere.
            */
            BinderMetadata( KeyType k, NativePtr v ) : key(k), value(v)
            {}
            typedef std::map< void const *, BinderMetadata > MapType;
            /**
               For use with std::map, returns true if (this->key < rhs.key).
            */
            bool operator<( BinderMetadata const & rhs )
            {
                return this->key < rhs.key;
            }
            /**
               Shared map for storing bindings.
            */
            static MapType & map()
            {
                static MapType bob;
                return bob;
            }
        };
    }
#endif // !DOXYGEN

    /**
       Associates obj with the given key, such that
       GetBoundNative(key) will return that object. It requires that
       key be non-0. In practice it is simplest to use obj as key (but
       note that key holds a typeless void pointer, instead of a
       type-safe pointer).

       obj must live until UnbindNative() is called, or
       GetBoundNative() will return a dangling pointer.

       Calls to BindNative() must be balanced by calls to UnbindNative()
       or internal mapping resources may be leaked. The ScopedBinder
       class can simplify ensuring this requirement is met.

       This function returns true if it binds the object, else false. Error
       conditions include:

       - The given key is already bound to some value of that type.

       - If this routine is called post-main(), results are
       undefined. This is because the destruction order of
       static/shared data is undefined, and this affects both the
       bindings data and the state of the associated v8 engine. It is
       possible that it would be called after either v8 or the shared
       map have been destroyed, either of which is likely to cause a
       crash (v8 normally provides very pretty trace information when
       it does this!).


       Notes about thread safety:

       This code is technically not thread-safe because it uses
       static, shared data to store the bindings. That said, the v8
       engine does not allow more than one thread to use it at a time
       (and also makes heavy use of static functions to access shared
       data). Since the binding API is only intended to be used from
       v8 client code, we inherit the property that only one thread at
       a time will access the shared data.


       Potential TODOs:

       - Consider adding a bind count, essentially a reference count
       for the bindings, and don't unbind until the count goes to 0.
       This would simplify some binding operations which we currently
       can't do because we end up with multiple unbind operations
       happening at unpredictable (or "predictable but unfortunate")
       times. e.g. with that we could make ScopedBinder() copyable.
    */
    template <typename NT>
    bool BindNative( void const * key,  NT * obj )
    {
	if( 0 == key ) return false;
	//typedef typename Detail::type_stripper<NT>::type * PT;
	//typedef std::map< void const *, PT > MT;
        typedef Detail::BinderMetadata<NT> BI;
        typedef typename BI::MapType MT;
	MT & map = BI::map();
	typename MT::iterator it = map.find( key );
	if( it != map.end() ) return false;
	map[key] = BI( key, obj );
	return true;
    }

    /** Equivalent to BindNative(obj,obj). */
    template <typename NT>
    bool BindNative( NT * obj )
    {
	return BindNative( obj, obj );
    }

    /**
       Equivalent to BindNative( id->Value(), obj ).
    */
    template <typename NT>
    bool BindNative( Handle<External> const & id,  NT * obj )
    {
	return id.IsEmpty()
	    ? false
	    : BindNative( id->Value(), obj );
    }

    /**
       Equivalent to BindNative( id->Value(), obj ).
    */
    template <typename NT>
    bool BindNative( Local<External> const & id,  NT * obj )
    {
	return id.IsEmpty()
	    ? false
	    : BindNative( id->Value(), obj );
    }

    /**
       Returns the NT object (if any) bound to the lookup key, or 0 if
       BindNative() has not been called for that key.
    */
    template <typename NT>
    NT * GetBoundNative( BindKeyType key )
    {
	if( 0 == key ) return false;
        typedef Detail::BinderMetadata<NT> BI;
	//typedef typename Detail::type_stripper<NT>::type * PT;
	//typedef std::map< void const *, PT > MT;
	typedef typename BI::MapType MT;
        MT & map = BI::map();
	typename MT::iterator it = map.find( key );
	return (it == map.end() )
	    ? 0
	    : (*it).second.value;
    }

    /**
       Convenience overload. If id->IsExternal() then this function
       returns the bound NT object (if any) associated with the
       External data, otherwise 0 is returned.
    */
    template <typename NT>
    NT * GetBoundNative( Handle<Value> id )
    {
	External * ex = id.IsEmpty() ? 0 :
	    (id->IsExternal() ? External::Cast( *id ) : 0);
	return ex
	    ? GetBoundNative<NT>( ex->Value() )
	    : 0;
    }
    /**
       Disassociates a native object which has been bound using
       BindNative(). The arguments must be the same ones passed to
       BindNative(). On success true is returned and the association
       is removed. On error (object not registered, invalid arguments,
       or key is bound to a different object) false is returned.
    */
    template <typename NT>
    bool UnbindNative( BindKeyType key, NT const * obj )
    {
	if( 0 == key ) return false;
        typedef Detail::BinderMetadata<NT> BI;
        typedef typename BI::MapType MT;
	MT & map = BI::map();
	typename MT::iterator it = map.find( key );
	if( map.end() == it ) return false;
	if( (*it).second.value != obj ) return false;
	map.erase( it );
	return true;
    }

    /** Equivalent to UnbindNative(obj,obj). */
    template <typename NT>
    bool UnbindNative( NT * obj )
    {
	return UnbindNative( obj, obj );
    }

//     /**
//        Equivalent to UnbindNative( id->Value(), obj ).
//     */
//     template <typename NT>
//     bool UnbindNative( Local<External> const & id,  NT * obj )
//     {
// 	return id.IsEmpty()
// 	    ? false
// 	    : UnbindNative( id->Value(), obj );
//     }

    /**
       If id is-a External then this is equivalent to UnbindNative(
       id->Value(), obj ), otherwise it returns false.
    */
    template <typename NT>
    bool UnbindNative( Handle<Value> const & id,  NT * obj )
    {
	const External * ex = id.IsEmpty() ? 0 :
	    (id->IsExternal() ? External::Cast( *id ) : 0);
	return ex
	    ? UnbindNative<NT>( ex->Value(), obj )
	    : false;
    }

    /**
       A sentry type which calls BindNative() at construction
       and UnbindNative() at destruction.
    */
    template <typename NT>
    class ScopedBinder
    {
    public:
	/**
	   Calls BindNative(cx,id,obj) and sets this->bound
	   to the result.
	*/
	ScopedBinder( BindKeyType key, NT * obj )
	    : bound( BindNative( key, obj ) ), id(key), native(obj)
	{
	}

	/**
	   Unbinds the native which was bound via the ctor.
	*/
	~ScopedBinder()
	{
	    if( this->bound )
	    {
		UnbindNative( this->id, this->native );
	    }
	}
    private:
	ScopedBinder( ScopedBinder const & that );
	ScopedBinder & operator=( ScopedBinder const & that );
	bool bound;
	BindKeyType id;
	NT * native;
    };

}}} /* namespaces */

#endif /* CODE_GOOGLE_COM_P_V8_V8_BIND_H_INCLUDED */
