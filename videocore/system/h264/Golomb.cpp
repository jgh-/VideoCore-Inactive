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
#include <videocore/system/h264/Golomb.h>

namespace videocore { namespace h264 {
 
    GolombDecode::GolombDecode(const WORD* inBitstream)
    :   m_pCurrentPosition(const_cast<WORD*>(inBitstream)), m_leftBit(kBufWidth),  m_bitsRead(0), m_lastBitsRead(0)
    {
        
    }
    GolombDecode::~GolombDecode()
    {
        
    }
    
    uint32_t
    GolombDecode::getBits(const size_t num_bits)
    {
        uint32_t val = 0;
        WORD bytes = *m_pCurrentPosition;
        const size_t bufWidth = kBufWidth;
        const size_t bitCount = std::min(num_bits, sizeof(WORD)*8);
        
        if(bitCount <= m_leftBit)
        {
            val = (bytes >> (bufWidth - bitCount));
            m_leftBit -= bitCount;
            bytes <<= bitCount;
           
        } else {
            unsigned long remain = bitCount - m_leftBit;
            val = (bytes >> (bufWidth - m_leftBit)) << remain;
            bytes = *(++m_pCurrentPosition);
            val |= (bytes >> (bufWidth - remain));
            bytes <<= remain;
            m_leftBit = bufWidth - remain;
        }
        m_bitsRead += bitCount;
        m_lastBitsRead = bitCount;

        *m_pCurrentPosition = bytes;
        
        if(!m_leftBit) {
            m_leftBit = bufWidth;
            m_pCurrentPosition++;
        }

        return val;
    }

    uint32_t
    GolombDecode::unsignedDecode()
    {
        WORD bytes   = *m_pCurrentPosition;
        uint32_t ret = 0;
        const size_t bufWidth = kBufWidth;
        
        if(bytes!=0)
        {
            int number_of_bits = __builtin_clz(bytes);
            ret = getBits(number_of_bits * 2 + 1);
            ret--;
           
        }
        else if(m_leftBit > 0)
        {
            int number_of_bits;
            bytes = *(++m_pCurrentPosition);
            number_of_bits = __builtin_clz(bytes) +static_cast<unsigned>( m_leftBit );
            m_leftBit = bufWidth;
            ret = getBits(number_of_bits*2+1);
            ret--;

        }
        
        return ret;
    }
    int32_t
    GolombDecode::signedDecode()
    {
        uint32_t unsignedVal = unsignedDecode();

        int32_t  val = unsignedVal >> 1 ;
        
        if( unsignedVal & 1 )
        {
            val *= -1;
        }
        
        return val;
    }
}
}