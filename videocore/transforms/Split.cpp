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
/*
 *  Split transform :: push a buffer to multiple outputs.
 *
 *
 */
#include <videocore/transforms/Split.h>

namespace videocore {
    Split::Split() {}
    Split::~Split() {}
    
    void
    Split::setOutput(std::shared_ptr<IOutput> output)
    {
        const auto inHash = std::hash<std::shared_ptr<IOutput>>()(output);
        bool duplicate = false;
        for ( auto & it : m_outputs) {
            auto outp = it.lock();
            if(outp) {
                auto hash = std::hash<std::shared_ptr<IOutput>>()(outp);
                duplicate |= (hash == inHash);
            }
        }
        if(!duplicate) {
            m_outputs.push_back(output);
        }
    }
    void
    Split::removeOutput(std::shared_ptr<IOutput> output)
    {
        const auto inHash = std::hash<std::shared_ptr<IOutput>>()(output);
        
        auto it = std::remove_if(m_outputs.begin(), m_outputs.end(), [=](const std::weak_ptr<IOutput>& rhs) {
           
            auto outp = rhs.lock();
            if(outp) {
                return inHash == std::hash<std::shared_ptr<IOutput>>()(outp);
            }
            return true;
        });
        m_outputs.resize(std::distance(m_outputs.begin(), it));
    }
    void
    Split::pushBuffer(const uint8_t* const data, size_t size, IMetadata& metadata)
    {
        for ( auto & it : m_outputs ) {
            auto outp = it.lock();
            if(outp) {
                outp->pushBuffer(data, size, metadata);
            }
        }
    }
}