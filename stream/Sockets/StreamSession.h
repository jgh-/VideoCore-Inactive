#ifndef __videocore_Sockets_StreamSession
#define __videocore_Sockets_StreamSession

#include <string>
#include <thread>

#include <videocore/stream/IStreamSession.hpp>
#include <videocore/system/Buffer.hpp>

#include <netinet/in.h>

namespace videocore {
    namespace Sockets {
        
        class StreamSession : public IStreamSession
        {
        public:
            StreamSession();
            ~StreamSession();
            
            void connect(std::string host, int port, StreamSessionCallback_t);
            void disconnect();
            
            size_t write(uint8_t* buffer, size_t size);
            size_t read(uint8_t* buffer, size_t size);
            
            int unsent() { return 0; };
            int unread() { return 0; };
            
            const StreamStatus_t status() const {
                return m_status;
            };
            
        private:
            void setStatus(StreamStatus_t status, bool clear = false);
            void network();
            int resolveHostName(std::string hostname, struct in_addr* addr);

        private:
            RingBuffer                  m_ringBuffer;
            std::thread                 m_network;

            StreamSessionCallback_t     m_callback;
            StreamStatus_t              m_status;
          
            int m_sd;

            bool m_exiting;
            
        };
    }
}
#endif