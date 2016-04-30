/*
 
 Video Core
 Copyright (c) 2014 James G. Hurley
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */
#ifndef videocore_Buffer_h
#define videocore_Buffer_h

#include <memory>
#include <vector>
#include <string>
#include <stdint.h>
#include <mutex>

#ifdef __APPLE__
#import <CoreFoundation/CFByteOrder.h> // TODO: Remove Apple framework reliance in this code.

#define double_swap CFConvertFloat64HostToSwapped
#else
#include <byteswap.h>
inline double double_swap(double value){
    union v {
        double       f;
        uint64_t     i;
    };
    
    union v val;
    
    val.f = value;
    val.i = bswap_64(val.i);
    
    return val.f;
}

#define CFConvertDoubleSwappedToHost double_swap

typedef double CFSwappedFloat64;

#endif



typedef enum
{ kAMFNumber = 0,
    kAMFBoolean,
    kAMFString,
    kAMFObject,
    kAMFMovieClip,		/* reserved, not used */
    kAMFNull,
    kAMFUndefined,
    kAMFReference,
    kAMFEMCAArray,
    kAMFObjectEnd,
    kAMFStrictArray,
    kAMFDate,
    kAMFLongString,
    kAMFUnsupported,
    kAMFRecordSet,		/* reserved, not used */
    kAMFXmlDoc,
    kAMFTypedObject,
    kAMFAvmPlus,		/* switch to AMF3 */
    kAMFInvalid = 0xff
} AMFDataType_t;
/************************************************
 * Buffer writing funcs
 ************************************************/
static inline void put_buff(std::vector<uint8_t>& data, const uint8_t *src, size_t srcsize)
{
    size_t pos = data.size();
    data.resize(data.size() + srcsize);
    memcpy(&data[pos],src,srcsize);
}

static inline void put_byte(std::vector<uint8_t>& data, uint8_t val)
{
    data.push_back(val);
}

static inline void put_be16(std::vector<uint8_t>& data, short val)
{
    char buf[2];
    buf[1] = val & 0xff;
    buf[0] = (val >> 8) & 0xff;
    
    put_buff(data,(const uint8_t*)buf,sizeof(uint16_t));
}
static inline int get_be16(uint8_t* val) {
    return ((val[0]&0xff) << 8) | ((val[1]&0xff)) ;
}
static inline void put_be24(std::vector<uint8_t>& data, int32_t val)
{
    char buf[3];
    
    buf[2] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
    buf[0] = (val >> 16) & 0xff;
    
    put_buff(data, (const uint8_t*)buf, 3);
    
}
static inline int get_be24(uint8_t* val) {
    int ret = ((val[2]&0xff)) | ((val[1]&0xff) << 8) | ((val[0]&0xff)<<16) ;
    return ret;
}
static inline void put_be32(std::vector<uint8_t>& data, int32_t val)
{
    char buf[4];

    buf[3] = val & 0xff;
    buf[2] = (val >> 8) & 0xff;
    buf[1] = (val >> 16) & 0xff;
    buf[0] = (val >> 24) & 0xff;

    put_buff(data, (const uint8_t*)buf, sizeof(int32_t));
}
static inline int get_be32(uint8_t* val) {
    return ((val[0]&0xff)<<24) | ((val[1]&0xff)<<16) | ((val[2]&0xff) << 8) | ((val[3]&0xff)) ;
}

static inline void put_tag(std::vector<uint8_t>& data, uint8_t *tag)
{
    while (*tag) {
        put_byte(data, *tag++);
    }
}

static inline void put_string(std::vector<uint8_t>& data, std::string string) {
    if(string.length() < 0xFFFF) {
        put_byte(data, kAMFString);
        put_be16(data, string.length());
    } else {
        put_byte(data, kAMFLongString);
        put_be32(data, static_cast<int32_t>(string.length()));
    }
    put_buff(data, (const uint8_t*)string.c_str(), string.length());
}

static inline std::string get_string(uint8_t* buf, int& bufsize) {
    int len = 0;
    if(*buf++ == kAMFString) {
        len = get_be16(buf);
        buf+=2;
        bufsize = 2 + len;
    } else {
        len = get_be32(buf);
        buf+=4;
        bufsize = 4 + len;
    }
    
    std::string val((const char*)buf,len);
    return val;
}
static inline std::string get_string(uint8_t* buf) {
    int buflen = 0;
    return get_string(buf, buflen);
}
static inline void put_double(std::vector<uint8_t>& data, double val) {
    put_byte(data, kAMFNumber);
    
    CFSwappedFloat64 buf = double_swap(val);
    
    put_buff(data, (uint8_t*)&buf, sizeof(CFSwappedFloat64));
}
static inline double get_double(uint8_t* buf) {
    CFSwappedFloat64 arg;
    memcpy(&arg, buf, sizeof(arg));
    return CFConvertDoubleSwappedToHost(arg);
}
static inline void put_bool(std::vector<uint8_t>& data, bool val) {
    put_byte(data, kAMFBoolean);
    put_byte(data, val);
}
static inline void put_name(std::vector<uint8_t>& data, std::string name) {
    put_be16(data, name.size());
    put_buff(data, (uint8_t*)name.c_str(), name.size());
}
static inline void put_named_double(std::vector<uint8_t>& data, std::string name, double val) {
    put_name(data,name);
    put_double(data, val);
}
static inline void put_named_string(std::vector<uint8_t>& data, std::string name, std::string val) {
    put_name(data, name);
    put_string(data, val);
}
static inline void put_named_bool(std::vector<uint8_t>& data, std::string name, bool val) {
    put_name(data,name);
    put_bool(data, val);
}

namespace videocore {
#pragma mark - Basic buffer
    template<typename T>
    struct TBuffer
    {
        TBuffer() : m_total(0), m_size(0) {};
        
        TBuffer(size_t I) : m_total(I), m_size(0) {
            //memset(&m_buffer[0], 0, I);
        };
        
        virtual size_t put(uint8_t* buf, size_t size)
        {
            if(size > m_total) size = m_total;
            
            memcpy(&m_buffer[0], buf, size);
            m_size = size;
            return size;
        }
        virtual size_t read(uint8_t** buf, size_t size) {
            if(size > m_size) size = m_size;
            
            *buf = &m_buffer[0];
            
            return size;
        }
        virtual void setSize(size_t size) { m_size = size; };
        virtual size_t resize(size_t size) =0;
        virtual size_t size() const { return m_size; };
        virtual size_t total() const { return m_total; };
        virtual void clear() { m_size = 0; };
        
    protected:
        T m_buffer;
        size_t m_total;
        size_t m_size;
    };
    
    struct Buffer : TBuffer<std::unique_ptr<uint8_t[]> >
    {
        Buffer() : TBuffer() {};
        Buffer(size_t I) : TBuffer(I)
        {
            resize(I);
        };
        virtual size_t resize(size_t size) {
            if(size > 0) {
                m_buffer.reset();
                
                m_buffer.reset(new uint8_t[size]);
            } else {
                m_buffer.reset();
            }
            m_total = size;
            m_size = 0;
            
            return size;
        }
        virtual uint8_t* operator ()() const { return &m_buffer[0]; };
    };
    
#pragma mark - Ring buffer
    struct RingBuffer : public Buffer
    {
        RingBuffer() {};
        
        RingBuffer(size_t I) : Buffer(I), m_read(0), m_write(0)
        {
            
        };
        
        size_t writePosition() const {
            return m_write;
        };
        
        size_t advanceWrite(size_t offset) {
            m_mutex.lock();
            
            if(offset > m_size) offset = m_size;
            
            size_t start = m_write;
            size_t end = std::min(start+offset, m_total);
            size_t sz = (end-start);
            m_write += sz;
            if(sz < offset) {
                m_write = (offset - sz);
            }
            m_mutex.unlock();
            
            return offset;
        }
        
        size_t put(uint8_t* data, size_t size) {
            m_mutex.lock();
            if(m_size+size > m_total) size=(m_total-m_size);
            if(size>0) {
                size_t start = m_write;
                size_t end = std::min(start+size, m_total);
                size_t sz = (end-start);
                memcpy(&m_buffer[start], data, sz);
                m_write += sz;
                if(sz < size) {
                    memcpy(&m_buffer[0], data+sz, (size-sz));
                    m_write = (size-sz);
                }
            }
            m_size += size;
            m_mutex.unlock();
            
            return size;
        }
        // Copy data into the supplied buffer.
        size_t get(uint8_t* buf, size_t size, bool clear = false) {
            m_mutex.lock();
            if(size>m_size) size = m_size;
            
            if(size>0) {
                size_t start = m_read;
                size_t end = std::min(start+size, m_total);
                size_t sz = (end-start);
                memcpy(buf, &m_buffer[start], sz);
                if(clear) {
                    memset(&m_buffer[start], 0, sz);
                }
                m_read+=sz;
                if(sz < size) {
                    memcpy(buf+sz, &m_buffer[0], (size-sz));
                    if(clear) {
                        memset(&m_buffer[0], 0, (size-sz));
                    }
                    m_read = (size-sz);
                }
            }
            m_size -= size;
            m_mutex.unlock();
            
            return size;
        }
        // undo some reading
        // TODO: do more test!
        size_t unget(size_t length) {
            if (length + m_size > m_total) {
                length = m_total - m_size;
            }
            m_size += length;
            if (m_read >= length) {
                m_read -= length;
            }
            else {
                length -= m_read;
                m_read = m_total - length;
            }
            return length;
        }

        size_t read(uint8_t** buf, size_t size) {
            return read(buf, size, true);
        }
        
        // FIXME: full of BUGs here
        size_t read(uint8_t** buf, size_t size, bool advance) {
            m_mutex.lock();
            if(size>m_size) size = m_size;
            if(size>0) {
                size_t start = m_read;
                size_t end = std::min(start+size, m_total);
                size = (end-start);
                
                *buf = &m_buffer[m_read];
                if( advance )  {
                    m_read+=size;
                    if(m_read == end) {
                        m_read = 0;
                    }
                    m_size-=size;
                }
            } else {
                *buf = NULL;
            }
            m_mutex.unlock();
            return size;
        }
        size_t size() const { return m_size; }
        void clear() { m_size = 0; m_read = 0; m_write = 0; };
    private:
        std::mutex m_mutex;
        
        size_t m_read;
        size_t m_write;
    };
}
#endif
