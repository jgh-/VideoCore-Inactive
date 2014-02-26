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
#ifndef __videocore__Golomb__
#define __videocore__Golomb__

#include <iostream>
namespace videocore { namespace h264 {
    
    typedef uint32_t WORD;
    
    const size_t kBufWidth = sizeof(WORD)*8;
    
    static inline WORD swap(WORD input) { return __builtin_bswap32(input); };
    
    class GolombDecode
    {
    public:
        
        GolombDecode(const WORD* inBitstream);
        ~GolombDecode();
        
        uint32_t getBits(const size_t num_bits);
        uint32_t unsignedDecode();
        int32_t  signedDecode();
        
        const size_t bitsRead() const { return m_bitsRead; };
        const size_t lastBitsRead() const { return m_lastBitsRead; };
    private:
        
        const WORD*   m_bitstream;
        WORD*         m_pCurrentPosition;
        
        size_t        m_leftBit;
        size_t        m_bitsRead;
        size_t        m_lastBitsRead;
    };
    
}
}
#endif /* defined(__videocore__Golomb__) */
