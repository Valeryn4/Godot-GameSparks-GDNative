#include <GameSparks/GSPlatformDeduction.h>
#include <GameSparks/gsstl.h>

namespace gsstl
{
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
    string::string() {
        _size = 0;
        _capacity = 1;
        
        _buffer = new char[_capacity];
        
        _buffer[0] = '\0';
    }
    
    string::string(const string &str) {
        _size = str._size;
        _capacity = str._capacity;
        
        if (str._buffer && _capacity > 0) {
            _buffer = new char[_capacity];
            
            memcpy(_buffer, str._buffer, _capacity);
        }
    }
    
    string::string(const char *pString) {
        if (pString) {
            _capacity = utf8size(pString);
            _size = _capacity - 1;
            
            if (_capacity > 0) {
                _buffer = new char[_capacity];
                
                memcpy(_buffer, pString, _capacity);
            }
        }
    }

    string::string(const char *pString, size_t n) {
        if (pString && n > 0) {
            _size = n;
            _capacity = n + 1;
            
            _buffer = new char[_capacity];
                
            memcpy(_buffer, pString, _size);

            _buffer[_size] = '\0';
        }
    }
    
    string::string(size_t n, const char c) {
        if (n > 0) {
            _size = n;
            _capacity = n + 1;
            
            _buffer = new char[_capacity];
            
            for (int a = 0; a < n; a ++) {
                _buffer[a] = c;
            }
            
            _buffer[n] = '\0';
        }
    }

    string::string(const unsigned char *pFirst, const unsigned char *pLast) {
    	_size = pLast - pFirst;
        
        if (_size > 0) {
            _capacity = _size + 1;
            
            _buffer = new char[_capacity];
            
            memcpy(_buffer, pFirst, _size);
            
            _buffer[_size] = '\0';
        }
    }
    
    string::string(const char *pFirst, const char *pLast) {
        _size = pLast - pFirst;
        
        if (_size > 0) {
            _capacity = _size + 1;
            
            _buffer = new char[_capacity];
            
            memcpy(_buffer, pFirst, _size);
            
            _buffer[_size] = '\0';
        }
    }
    
    string::string(std::vector<char>::iterator first, std::vector<char>::iterator last) {
        int index = 0;
        
        _size = last - first;
        
        if (_size > 0) {
            _capacity = _size + 1;
            
            _buffer = new char[_capacity];
            
            for (std::vector<char>::iterator i = first; i != last; i ++) {
                _buffer[index ++] = *i;
            }
            
            _buffer[index] = '\0';
        }
    }
    
    string::~string() {
        if (_buffer) {
            delete _buffer;
        }
    }
    
    size_t string::size() const {
        return _size;
    }
    
    size_t string::length() const {
        return _size;
    }
    
    const char *string::c_str() const {
        return _buffer;
    }
    
    void string::reserve(size_t n) {
        char *pNewBuffer;
        
        if (_buffer) {
            if (n > _capacity) {
                pNewBuffer = new char[n];
                
                memcpy(pNewBuffer, _buffer, _capacity);
                
                delete _buffer;
                
                _buffer = pNewBuffer;
                _capacity = n;
            } else if (n < _capacity) {
                pNewBuffer = new char[n];
                
                memcpy(pNewBuffer, _buffer, n - 1);
                pNewBuffer[n - 1] = '\0';
                
                delete _buffer;
                
                _buffer = pNewBuffer;
                _capacity = n;
                if (_size >= _capacity) {
                    _size = _capacity - 1;
                }
            }
        } else {
            if (n > 0) {
                _capacity = n;
                
                _buffer = new char[_capacity];
            }
        }
    }

    void string::resize(size_t n) {
    	if (_buffer && n > 0) {
    		if (n < _size) {
    			_size = n;

    			_buffer[_size] = '\0';
    		} else if (n > _size) {
    			reserve(n + 1);

    			for (size_t i = _size; i <= n; i ++) {
    				_buffer[i] = '\0';
    			}

    			_size = n;
    		}
    	}
    }
    
    bool string::empty() const {
        return (_size <= 0);
    }
    
    const char *string::data() const {
        return _buffer;
    }
    
    string string::substr(size_t pos, size_t len) const {
        if (!_buffer || pos >= _size || len <= 0) {
            return string();
        } else {
            if (len == npos || _size <= pos + len) {
                return string(&_buffer[pos], &_buffer[_size]);
            } else {
                return string(&_buffer[pos], &_buffer[pos + len]);
            }
        }
    }
    
    size_t string::find(const string& str, size_t pos) const {
        if (!_buffer || pos < 0 || pos >= _size) {
            return npos;
        } else {
            char *ret = (char *)utf8str(_buffer + pos, str._buffer);
            
            if (ret) {
                return ret - _buffer;
            } else {
                return npos;
            }
        }
    }
    
    size_t string::find(char c, size_t pos) const {
        if (!_buffer || pos < 0 || pos >= _size) {
            return npos;
        } else {
            char *ret = (char *)utf8chr(_buffer + pos, c);
            
            if (ret) {
                return ret - _buffer;
            } else {
                return npos;
            }
        }
    }
    
    size_t string::find_first_not_of(const char *s, size_t pos) const {
        if (!_buffer || pos < 0 || pos >= _size) {
            return npos;
        } else {
            bool ok;
            char *pSrc;
            int chr;
            
            for (pSrc = _buffer + pos; pSrc != &_buffer[_size];) {
                ok = true;
                
                pSrc = (char *)utf8codepoint(pSrc, &chr);
           
                for (int j = 0; j < strlen(s); j ++) {
                    if (s[j] == chr) {
                        ok = false;
                        
                        break;
                    }
                }
                
                if (ok) {
                    return pSrc - _buffer;
                }
            }
            
            return npos;
        }
    }
    
    string &string::replace(size_t pos, size_t len, const string &str) {
        if (!_buffer || !str._buffer || pos < 0 || pos >= _size) {
            return *this;
        }
        
        if (len == npos) {
            len = str._size;
        }
        
        if (len > 0) {
            int a;
            int b;
            
            for (a = pos, b = 0; a < pos + len; a ++, b++) {
                if (a >= _size || b >= str._size) {
                    break;
                }
                
                _buffer[a] = str._buffer[b];
            }
        }
        
        return *this;
    }
    
    char *string::begin() {
        return _buffer;
    }
    
    const char *string::begin() const {
        return _buffer;
    }
    
    char *string::end() {
        if (_buffer) {
            return _buffer + _size;
        } else {
            return _buffer;
        }
    }
    
    const char *string::end() const {
        if (_buffer) {
            return _buffer + _size;
        } else {
            return _buffer;
        }
    }
    
    void string::ltrim() {
        if (_buffer) {
            char *pSrc;
            char *pNewSrc;
            char *pDest;
            int chr;
            
            for (pSrc = _buffer; pSrc != &_buffer[_size];) {
                pNewSrc = (char *)utf8codepoint(pSrc, &chr);
                
                if (!isspace(chr)) {
                    if (pSrc != _buffer) {
                        for (pDest = _buffer; pSrc <= &_buffer[_size]; pSrc ++, pDest ++) {
                            *pDest = *pSrc;
                        }
                        
                        _size = utf8size(_buffer) - 1;
                    }
                    
                    return;
                }
                
                pSrc = pNewSrc;
            }
        }
    }
    
    void string::rtrim() {
        if (_buffer) {
            char *pSrc;
            char *pNewSrc;
            int chr;
            int len = utf8len(_buffer);
            
            if (len > 0) {
                char **pOffsets = new char *[len];
                int index;
                int *pCharsOffsets = new int[len];
                
                for (index = 0, pSrc = _buffer; pSrc != &_buffer[_size]; index ++) {
                    pOffsets[index] = pSrc;
                    
                    pSrc = (char *)utf8codepoint(pSrc, &chr);
                
                    pCharsOffsets[index] = chr;
                }
                
                for (int i = len - 1; i >= 0; i --) {
                    if (isspace(pCharsOffsets[i])) {
                        *(pOffsets[i]) = '\0';
                        
                        _size = utf8size(_buffer) - 1;

                        delete[] pOffsets;
                        delete[] pCharsOffsets;
                        
                        return;
                    }
                }

                delete[] pOffsets;
                delete[] pCharsOffsets;
            }
        }
    }
    
    void string::trim() {
        ltrim();
        rtrim();
    }
    
    void string::push_back(char c) {
        if (_buffer) {
            _buffer[_size ++] = c;
        
            if (_size >= _capacity) {
                reserve(_capacity + 1);
            }
            
            _buffer[_size] = '\0';
        } else {
            _size = 1;
            _capacity = 2;
            
            _buffer = new char[_capacity];
            
            _buffer[0] = c;
            _buffer[1] = '\0';
        }
    }

    string &string::append(const string &str) {
        if (_buffer) {
            if (str._buffer && str._size > 0) {
                size_t newSize = _size + str._size;
                
                if (newSize >= _capacity) {
                    reserve(newSize + 1);
                }
                
                memcpy(_buffer + _size, str._buffer, str._size + 1);
                
                _size = newSize;
            }
        } else {
            *this = str;
        }
        
        return *this;
    }

    string &string::append(const char *s, size_t n) {
    	if (_buffer) {
            if (s && n > 0) {
                size_t newSize = _size + n;
                
                if (newSize >= _capacity) {
                    reserve(newSize + 1);
                }
                
                memcpy(_buffer + _size, s, n);
                
                _size = newSize;

                _buffer[_size] = '\0';
            }
        } else {
            *this = s;
        }
   
        return *this;
    }

    string &string::operator = (const char c) {
    	if (_buffer) {
            delete _buffer;
            _buffer = NULL;
        }

        if (c != '\0') {
        	_capacity = 2;
        	_size = _capacity - 1;

        	_buffer = new char[_capacity];

        	_buffer[0] = c;
        	_buffer[1] = '\0';
        } else {
        	_capacity = 1;
        	_size = _capacity - 1;

        	_buffer = new char[_capacity];

        	_buffer[0] = '\0';
        }

        return *this;
    }
    
    string &string::operator = (const string &rhs) {
        if (_buffer) {
            delete _buffer;
            _buffer = NULL;
        }
        
        _size = rhs._size;
        _capacity = rhs._capacity;
        
        if (rhs._buffer && _capacity > 0) {
            _buffer = new char[_capacity];
            
            memcpy(_buffer, rhs._buffer, _capacity);
        }
        
        return *this;
    }
    
    string &string::operator = (const char *rhs) {
        if (_buffer) {
            delete _buffer;
            _buffer = NULL;
        }
        
        if (rhs) {
            _capacity = utf8size(rhs);
            _size = _capacity - 1;
            
            if (_capacity > 0) {
                _buffer = new char[_capacity];
                
                memcpy(_buffer, rhs, _capacity);
            }
        }
        
        return *this;
    }
    
    bool string::operator == (const string &rhs) const {
        return _buffer && rhs._buffer && utf8casecmp(_buffer, rhs._buffer) == 0;
    }

    bool string::operator != (const string &rhs) const {
        return !_buffer || !rhs._buffer || utf8casecmp(_buffer, rhs._buffer) != 0;
    }

    bool string::operator != (const char *rhs) const {
    	return !_buffer || !rhs || utf8casecmp(_buffer, rhs) != 0;
    }
    
    string &string::operator += (const string &rhs) {
        return append(rhs);
    }
    
    string &string::operator += (const char *rhs) {
    	return append(rhs, utf8size(rhs) - 1);
    }
    
    string &string::operator += (const char rhs) {
        push_back(rhs);
        
        return *this;
    }
    
    string string::operator + (const string &rhs) {
        string tmp(*this);
        
        tmp += rhs;
        
        return tmp;
    }
    
    string string::operator + (const char *rhs) {
        string tmp(*this);
        
        tmp += rhs;
        
        return tmp;
    }
    
    char &string::operator [] (size_t pos) {
        return (char &)_buffer[pos];
    }
    
    const char &string::operator [] (size_t pos) const {
        return (const char &)_buffer[pos];
    }
    
    bool string::operator < (const string &rhs) const {
        if (_buffer && rhs._buffer && utf8casecmp(_buffer, rhs._buffer) < 0) {
            return true;
        } else {
            return false;
        }
    }
    
    string operator + (const char *lhs, const string &rhs) {
        string tmp(lhs);
        
        tmp += rhs;
        
        return tmp;
    }
    
    string operator + (const string &lhs, const string &rhs) {
        string tmp(lhs);
        
        tmp += rhs;
        
        return tmp;
    }
    
    string operator + (const string &lhs, const char *rhs) {
        string tmp(lhs);
        
        tmp += rhs;
        
        return tmp;
    }

    ostream clog;
    ostream cerr;
    ostream cout;

    string to_string(int val) {
        FString buf = FString::FromInt(val);

        return string(TCHAR_TO_ANSI(*buf));
    }
#endif
    
    // trim from start
    string &ltrim(string &s) {
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
        s.ltrim();
#else
        s.erase(s.begin(), gsstl::find_if(s.begin(), s.end(), gsstl::not1(gsstl::ptr_fun<int, int>(isspace))));
#endif
        
        return s;
    }
    
    // trim from end
    string &rtrim(string &s) {
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
        s.rtrim();
#else
        s.erase(gsstl::find_if(s.rbegin(), s.rend(), gsstl::not1(gsstl::ptr_fun<int, int>(isspace))).base(), s.end());
#endif
        
        return s;
    }
    
    // trim from both ends
    string &trim(string &s) {
#if ((GS_TARGET_PLATFORM == GS_PLATFORM_IOS || GS_TARGET_PLATFORM == GS_PLATFORM_MAC) && defined(__UNREAL__))
        s.trim();

        return s;
#else
        return ltrim(rtrim(s));
#endif
    }
}
