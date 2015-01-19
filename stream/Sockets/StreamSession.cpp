#include <videocore/stream/Sockets/StreamSession.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

namespace videocore { namespace Sockets {

		StreamSession::StreamSession() : m_sd(0), m_exiting(false), m_ringBuffer(1024*1024) {}

		StreamSession::~StreamSession() 
		{
			disconnect();
		};

		int
		StreamSession::resolveHostName(std::string name, struct in_addr* host)
		{
			int res = 0;
			struct addrinfo* addr;

			res = getaddrinfo(name.c_str(), nullptr, nullptr, &addr);
			if(res==0) {
				memcpy(host, &((struct sockaddr_in*) addr->ai_addr)->sin_addr, sizeof(struct in_addr));
				freeaddrinfo(addr);
			}
			return res;
		}

		void
		StreamSession::connect(std::string host, int port, StreamSessionCallback_t callback)
		{
			m_callback = callback;

			struct sockaddr_in address = {0};
			address.sin_family = AF_INET;
			address.sin_port = htons(port);

			bool canConnect = false;
			

			if(resolveHostName(host, &(address.sin_addr)) != 0) {
				// Couldn't resolve hostname.
				if(inet_pton(PF_INET, host.c_str(), &(address.sin_addr)) == 1) { 
					canConnect = true;
				} else {
					setStatus(kStreamStatusErrorEncountered, true);
				}
			} else {
				canConnect = true;
			}
			if(canConnect) {
				int sd = socket(AF_INET, SOCK_STREAM, 0);
				if(::connect(sd, (struct sockaddr*)&address, sizeof(address)) != 0) {
					setStatus(kStreamStatusErrorEncountered, true);
				} else {
					m_sd = sd;
					m_network = std::thread([&]() {
						this->network();
					});
					setStatus(kStreamStatusConnected | kStreamStatusWriteBufferHasSpace, true);
				}
			}
		}
		void
		StreamSession::disconnect() {
			m_exiting = true;

			if(m_sd) {
				close(m_sd);
			}
			m_network.join();
		}
		void
		StreamSession::network() {
			uint8_t buf[4096];
			while(!m_exiting) {
				size_t ret = ::read(m_sd, buf, 4096);
				m_ringBuffer.put(buf, ret);
				setStatus(kStreamStatusReadBufferHasBytes, false);
			}
		}

		size_t
		StreamSession::write(uint8_t* buffer, size_t size) {
			return ::write(m_sd, buffer, size);
		}

		size_t
		StreamSession::read(uint8_t* buffer, size_t size) {
			size_t ret = m_ringBuffer.get(buffer, size);
			if ( ret < size ) {
				m_status ^= kStreamStatusReadBufferHasBytes;
			}
			return ret;
		}

		void
        StreamSession::setStatus(StreamStatus_t status, bool clear)
        {
            if(clear) {
                m_status = status;
            } else {
                m_status |= status;
            }
            m_callback(*this, status);
        }
}}