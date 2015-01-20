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
			DLog("resolving %s\n", name.c_str());
			int res = 0;
			struct addrinfo* addr;

			res = getaddrinfo(name.c_str(), nullptr, nullptr, &addr);
			if(res==0) {
				memcpy(host, &((struct sockaddr_in*) addr->ai_addr)->sin_addr, sizeof(struct in_addr));
				freeaddrinfo(addr);
			}
			DLog("result=%d\n", res);
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
			
			DLog("Connecting to %s:%d\n", host.c_str(), port);
			if(resolveHostName(host, &(address.sin_addr)) != 0) {
				// Couldn't resolve hostname.
				DLog("Couldn't resolve hostname, could be an IP\n");
				if(inet_pton(PF_INET, host.c_str(), &(address.sin_addr)) == 1) { 
					DLog("Got IP\n");
					canConnect = true;
				} else {
					setStatus(kStreamStatusErrorEncountered, true);
				}
			} else {
				canConnect = true;
			}
			if(canConnect) {
				DLog("Connecting...\n");
				int sd = socket(AF_INET, SOCK_STREAM, 0);
				if( sd > 0 ) {
					if(::connect(sd, (struct sockaddr*)&address, sizeof(address)) != 0) {
						setStatus(kStreamStatusErrorEncountered, true);
						DLog("Error %d (%d)\n", errno, sd);
					} else {
						m_sd = sd;
						DLog("Conected, starting network\n");
						m_network = std::thread([&]() {
							this->network();
						});
						setStatus(kStreamStatusConnected | kStreamStatusWriteBufferHasSpace, true);
					}
				} else {
					DLog("::socket(%d) error(%d)\n", sd, errno);
					setStatus(kStreamStatusErrorEncountered, true);
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
				ssize_t ret = ::read(m_sd, buf, 4096);
				if(ret > 0) {
					m_ringBuffer.put(buf, ret);
					setStatus(kStreamStatusReadBufferHasBytes, false);
				} else if ( ret < 0 ) {
					setStatus(kStreamStatusErrorEncountered, false);
				}
			}
		}

		ssize_t
		StreamSession::write(uint8_t* buffer, size_t size) {
			ssize_t ret = ::write(m_sd, buffer, size);

			if( ret < 0 ) {
				setStatus(kStreamStatusErrorEncountered, false);
			}

			return ret;
		}

		ssize_t
		StreamSession::read(uint8_t* buffer, size_t size) {
			ssize_t ret = m_ringBuffer.get(buffer, size);
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