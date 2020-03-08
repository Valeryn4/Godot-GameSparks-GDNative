// Copyright 2015 GameSparks Ltd 2015, Inc. All Rights Reserved.
#ifndef gsstl_h__
#define gsstl_h__

#pragma once

/*!
	This header is here, so that we can easily switch between
	different STL implementations. Current choices are:
		- STD-STL (what ever your compiler uses)

*/



#if defined(DOXYGEN)

/// if set to 1, the stl provided by the compiler is used.
/// @ingroup CompileOptions
#define USE_STD_STL <platform-dependend>

/// if set to 1, std::function will be used for callbacks. This also enables support for C++11 lambdas.
/// This is disabled on Marmalade.
/// @ingroup CompileOptions
#define STL_FUNCTIONAL_SUPPORTED <platform-dependend>

#endif

#include <GameSparks/GSPlatformDeduction.h>

#include <mutex>
#include <thread>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <cassert>
#include <array>
#include <utility>
#include <clocale>
#include <cmath>
#include <queue>
#include <limits>
#include <memory>
#include <cctype>

#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
#include "Engine.h"
#include "Engine/EngineTypes.h"
#include "utf8.h"

#undef USE_STD_STL
#else
#include <string>
#include <set>
#include <stdexcept>
#include <ostream>
#include <iostream>
#include <sstream>
#include <fstream>

#define USE_STD_STL 1
#endif


#if !defined(STL_FUNCTIONAL_SUPPORTED) && !defined(IW_SDK)
#define STL_FUNCTIONAL_SUPPORTED 1
#endif

// std function can only be used, if we have at least c++11 
#if (__cplusplus >= 201103L || _MSC_VER >= 1800 ) /*&& USE_STD_STL*/ && STL_FUNCTIONAL_SUPPORTED
#	define GS_USE_STD_FUNCTION 1
#	include <functional>
#else
#	undef GS_USE_STD_FUNCTION
#endif /* __cplusplus >= 201103L */

// include required headers
#if defined(USE_STD_STL)
#	include <string>
#	include <vector>
#	include <list>
#	include <map>
#	include <algorithm>
#	include <set>
#endif /* defined(USE_STD_STL) */

//! This namespace provides forwarding to the concrete STL implementation used.
namespace gsstl
{
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    typedef std::size_t size_t;
    
    typedef std::mutex mutex;
    typedef std::recursive_mutex recursive_mutex;
    typedef std::thread thread;
    
    namespace this_thread = std::this_thread;
    
    namespace chrono = std::chrono;
    
    template <typename T>
    class stl_allocator
    {
    public:
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T value_type;
        
        stl_allocator(){}
        ~stl_allocator(){}
        
        template <class U> struct rebind { typedef stl_allocator<U> other; };
        template <class U> stl_allocator(const stl_allocator<U>&){}
        
        pointer address(reference x) const {return &x;}
        const_pointer address(const_reference x) const {return &x;}
        size_type max_size() const throw() {return size_t(-1) / sizeof(value_type);}
        
        pointer allocate(size_type n, typename std::allocator<void>::const_pointer = 0)
        {
            return reinterpret_cast<pointer>(::operator new(n * sizeof (T)));
        }
        
        void deallocate(pointer p, size_type n)
        {
            ::operator delete(p); 
        }
        
        void construct(pointer p, const T& val)
        {
            new(static_cast<void*>(p)) T(val);
        }
        
        void construct(pointer p)
        {
            new(static_cast<void*>(p)) T();
        }
        
        void destroy(pointer p)
        {
            p->~T();
        }
    };
    
    template<typename T, typename U>
    inline bool operator == (const stl_allocator<T>&, const stl_allocator<U>) { return true; }
    
    template<typename T, typename U>
    inline bool operator != (const stl_allocator<T>&, const stl_allocator<U>) { return false; }
    
    /*struct char_traits : public std::char_traits<char> {
        static inline char* move(char* dest, const char* src, size_t n) {
            return n == 0 ? dest : (char*)FMemory::Memmove(dest, src, n);
        }
        
        static inline char* copy(char* dest, const char* src, size_t n) {
            assert(src < dest || src >= dest + n);
            return n == 0 ? dest : (char*)FMemory::Memcpy(dest, src, n);
        }
        
        static inline void assign(char& c1, const char& c2) {
            c1 = c2;
        }
        
        static inline char* assign (char* p, size_t n, char c) {
            return n == 0 ? p : (char*)FMemory::Memset(p, to_int_type(c), n);
        }
    };
    
    typedef std::basic_string<char, char_traits, stl_allocator<char>> string;
    typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, stl_allocator<wchar_t>> wstring;
    
    typedef std::basic_stringstream<char, char_traits, stl_allocator<char>> stringstream;
    typedef std::basic_stringstream<wchar_t, std::char_traits<wchar_t>, stl_allocator<wchar_t>> wstringstream;*/
    
    static const char endl = '\n';
    
    template<class _Tp, class _Alloc = stl_allocator<_Tp>>
    using vector = std::vector<_Tp, _Alloc>;
    
    class string {
    public:
        typedef size_t size_type;
        typedef char value_type;
        
        static const size_t npos = -1;
        
        string();
        string(const string &str);
        string(const char *pString);
        string(const char *pString, size_t n);
        string(size_t n, const char c);
        string(const unsigned char *pFirst, const unsigned char *pLast);
        string(const char *pFirst, const char *pLast);
        string(std::vector<char>::iterator first, std::vector<char>::iterator last);
        ~string();
        
        size_t size() const;
        size_t length() const;
        
        const char *c_str() const;
        
        void reserve(size_t n = 0);
        void resize(size_t n);
        
        bool empty() const;
        
        const char *data() const;
        
        string substr(size_t pos = 0, size_t len = npos) const;
        
        size_t find(const string& str, size_t pos = 0) const;
        size_t find(char c, size_t pos = 0) const;
        size_t find_first_not_of(const char *s, size_t pos = 0) const;
        
        string &replace(size_t pos, size_t len, const string &str);
        
        char *begin();
        const char *begin() const;
        char *end();
        const char *end() const;
        
        void ltrim();
        void rtrim();
        void trim();
        
        void push_back(char c);
        string &append(const string &str);
        string &append(const char *s, size_t n);
        
        string &operator = (const char c);
        string &operator = (const string &rhs);
        string &operator = (const char *rhs);
        bool operator == (const string &rhs) const;
        bool operator != (const string &rhs) const;
        bool operator != (const char *rhs) const;
        string &operator += (const string &rhs);
        string &operator += (const char *rhs);
        string &operator += (const char rhs) ;
        string operator + (const string &rhs);
        string operator + (const char *rhs);
        char &operator [] (size_t pos);
        const char &operator [] (size_t pos) const;
        bool operator < (const string &rhs) const;

    private:
        char *_buffer;
        size_t _size;
        size_t _capacity;
    };
    
    string operator + (const char *lhs, const string &rhs);
    string operator + (const string &lhs, const string &rhs);
    string operator + (const string &lhs, const char *rhs);
    
    /*std::ostream &operator << (std::ostream &os, const string &str) {
        os << str.c_str();
        
        return os;
    }*/
    
    enum _flags {
        _none = 0,
        _boolalpha = 1L << 0,
        _hex = 1L << 1
    };
    
    static const _flags boolalpha = _boolalpha;
    static const _flags noboolalpha = _none;
    static const _flags hex = _hex;
    
    class ostream {
    public:
        ostream() {
        }
        
        ~ostream() {
        }
        
        ostream &operator << (bool rhs) {
            if (_boolalpha) {
                _buffer += (rhs ? string("true") : string("false"));
            } else {
                _buffer += (rhs ? "1" : "0");
            }
            
            return *this;
        }
        
        ostream &operator << (int rhs) {
            char buffer[16];
            
            if (_hex) {
                sprintf(buffer, "%x", rhs);
            } else {
                sprintf(buffer, "%d", rhs);
            }
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (unsigned int rhs) {
            char buffer[16];
            
            if (_hex) {
                sprintf(buffer, "%x", rhs);
            } else {
                sprintf(buffer, "%u", rhs);
            }
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (const float rhs) {
            char buffer[32];
            
            sprintf(buffer, "%f", rhs);
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (const double rhs) {
            char buffer[32];
            
            sprintf(buffer, "%f", rhs);
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (unsigned long rhs) {
            char buffer[32];
            
            if (_hex) {
                sprintf(buffer, "%lx", rhs);
            } else {
                sprintf(buffer, "%lu", rhs);
            }
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (const long long rhs) {
            char buffer[32];
            
            if (_hex) {
                sprintf(buffer, "%llx", rhs);
            } else {
                sprintf(buffer, "%lld", rhs);
            }
            
            _buffer += buffer;
            
            return *this;
        }
        
        ostream &operator << (const char *rhs) {
            _buffer += rhs;
            
            return *this;
        }

        /*ostream &operator << (const wchar_t *rhs) {
            _buffer += rhs;
            
            return *this;
        }*/
        
        ostream &operator << (const _flags rhs) {
            if (rhs == boolalpha) {
                _boolalpha = true;
            } else if (rhs == noboolalpha) {
                _boolalpha = false;
            } else if (rhs == hex) {
                _hex = true;
            }
            
            return *this;
        }
        
        ostream &operator << (const void *rhs) {
            char buf[32];
            
            sprintf(buf, "%p", rhs);
            
            _buffer += buf;
            
            return *this;
        }
        
        ostream &operator << (const string &rhs) {
            _buffer += rhs;
            
            return *this;
        }
        
        ostream &operator << (std::thread::id rhs) {
            // TODO
            
            return *this;
        }
        
    protected:
        bool _boolalpha = false;
        bool _hex = false;
        string _buffer;
    };
    
    class stringstream : public ostream {
    public:
        string str() {
            return _buffer;
        }
    };

    class ostringstream : public stringstream {
    };
    
    extern ostream clog;
    extern ostream cerr;
    extern ostream cout;
    
    /*string setw(int n) {
        return string("");
    }
    
    string setfill(char c) {
        return string("");
    }*/
    
    template<class _Kty, class _Ty, class _Pr = std::less<_Kty>, class _Alloc = stl_allocator<std::pair<const _Kty, _Ty>>>
    using map = std::map<_Kty, _Ty, _Pr, _Alloc>;
    
    template<class _Ty, class _Alloc = stl_allocator<_Ty>>
    using list = std::list<_Ty, _Alloc>;
    
    template<class Mutex>
    using lock_guard = std::lock_guard<Mutex>;
    
    template<class _T1, class _T2>
    using pair = std::pair<_T1, _T2>;
    
    template<class T, class D = std::default_delete<T>>
    using unique_ptr = std::unique_ptr<T, D>;
    
    template<class T, size_t N>
    using array = std::array<T, N>;
    
    template<class T, class Container = std::deque<T>>
    using queue = std::queue<T, Container>;
    
    template<class T>
    using numeric_limits = std::numeric_limits<T>;
    
    template<typename... Args>
    auto move(Args&&... args) -> decltype(std::move(std::forward<Args>(args)...)) {
        return std::move(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto swap(Args&&... args) -> decltype(std::swap(std::forward<Args>(args)...)) {
        return std::swap(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto reverse(Args&&... args) -> decltype(std::reverse(std::forward<Args>(args)...)) {
        return std::reverse(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto sort(Args&&... args) -> decltype(std::sort(std::forward<Args>(args)...)) {
        return std::sort(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto find(Args&&... args) -> decltype(std::find(std::forward<Args>(args)...)) {
        return std::find(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto find_if(Args&&... args) -> decltype(std::find_if(std::forward<Args>(args)...)) {
        return std::find_if(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto fmin(Args&&... args) -> decltype(std::fmin(std::forward<Args>(args)...)) {
        return std::fmin(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto pow(Args&&... args) -> decltype(std::pow(std::forward<Args>(args)...)) {
        return std::pow(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto copy(Args&&... args) -> decltype(std::copy(std::forward<Args>(args)...)) {
        return std::copy(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto back_inserter(Args&&... args) -> decltype(std::back_inserter(std::forward<Args>(args)...)) {
        return std::back_inserter(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto make_pair(Args&&... args) -> decltype(std::make_pair(std::forward<Args>(args)...)) {
        return std::make_pair(std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    auto fill(Args&&... args) -> decltype(std::fill(std::forward<Args>(args)...)) {
        return std::fill(std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto transform(Args&&... args) -> decltype(std::transform(std::forward<Args>(args)...)) {
        return std::transform(std::forward<Args>(args)...);
    }

    template<typename... Args>
    auto tolower(Args&&... args) -> decltype(std::tolower(std::forward<Args>(args)...)) {
        return std::tolower(std::forward<Args>(args)...);
    }
    
    using runtime_error = std::runtime_error;
    
    /*template <class Arg, class Result>
    std::pointer_to_unary_function<Arg,Result> ptr_fun(Result (*f)(Arg))
    {
        return std::pointer_to_unary_function<Arg,Result>(f);
    }
    
    template <class Arg1, class Arg2, class Result>
    std::pointer_to_binary_function<Arg1,Arg2,Result> ptr_fun(Result (*f)(Arg1,Arg2))
    {
        return std::pointer_to_binary_function<Arg1,Arg2,Result>(f);
    }
    
    template <class Predicate>
    std::unary_negate<Predicate> not1(const Predicate& pred)
    {
        return std::unary_negate<Predicate>(pred);
    }*/

    extern string to_string(int val);
    
	template <class T> const T& min(const T& a, const T& b) {
		return !(b<a) ? a : b;
	}

#elif defined(USE_STD_STL)
    using namespace std;
#endif /* defined(USE_STD_STL) */

#if GS_USE_STD_FUNCTION
    using std::function;
#endif

    extern string &ltrim(string &s);
    extern string &rtrim(string &s);
    extern string &trim(string &s);
}

#if !defined(GS_COMPILER_HAS_NULLPTR_SUPPORT) || !GS_COMPILER_HAS_NULLPTR_SUPPORT
#	if defined(_MSC_VER) && _MSC_VER >= 1600 // vs has nullptr support since version 10 (2010)
#		define GS_COMPILER_HAS_NULLPTR_SUPPORT 1
#	endif
#endif

#if !defined(nullptr) && !defined(DOXYGEN) && !defined(GS_COMPILER_HAS_NULLPTR_SUPPORT)
namespace gsstl
{
    //!  a nullptr implementation for compilers that do not support the C++11 nullptr yet.
    //! based on SC22/WG21/N2431 = J16/07-0301
    struct nullptr_t
    {
        template<typename any> operator any * () const
        {
            return 0;
        }
        template<class any, typename T> operator T any:: * () const
        {
            return 0;
        }
    private:
        #ifdef _MSC_VER
                struct pad {};
                pad __[sizeof(void*)/sizeof(pad)];
        #else
                char __[sizeof(void*)];
        #endif

        //  nullptr_t();// {}
        //  nullptr_t(const nullptr_t&);
        //  void operator = (const nullptr_t&);
        void operator &() const;
        template<typename any> void operator +(any) const
        {
            /*I Love MSVC 2005!*/
        }
        template<typename any> void operator -(any) const
        {
            /*I Love MSVC 2005!*/
        }
    };
    static const nullptr_t nullptr_instance = nullptr_t();// = {};
}

#if !defined(WINAPI_FAMILY) && __cplusplus <= 199711L
#	define nullptr gsstl::nullptr_instance
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#	define GS_DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
#	define GS_DEPRECATED(func) __declspec(deprecated) func
#else
#	pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#	define GS_DEPRECATED(func) func
#endif

#endif // gsstl_h__
