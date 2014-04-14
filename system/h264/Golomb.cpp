/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
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