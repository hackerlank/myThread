//
//  MyNet.cpp
//  Dragon
//
//  Created by GameDeveloper on 13-10-20.
//
//

#include "MyNet_linux.h"
#if PLATFORM == PLATFORM_UNIX
#include "sys/socket.h"
#include "sys/epoll.h"
#include <sys/time.h>
#include "sys/poll.h"
#include "sys/select.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "fcntl.h"
#include "netdb.h"
#include "signal.h"
#include "memory.h"
#include <arpa/inet.h>
#include <pthread.h>
#include "strings.h"
#include "iconv.h"
#include <strings.h>
#include "unistd.h"
#include "poll.h"
#include "vector"
#include "list"
#include "stdio.h"
#include "sstream"

namespace mynet {
    /**
     * *******************decoder 处理器**********************
     */
    unsigned Decoder::leftsize()
	{
		return contents.size() -currentoffset;
	}
	void Decoder::refresh()
	{
		//
		switch(nowstate)
		{
			case START:
			{
				contents.resize(1);
				nowstate = PICK_HEAD;
				currentoffset = 0;
				tag = 0;
			}break;
			case PICK_HEAD:
			{
				tag = contents[0];
				if (tag & MIN_HEAD)
					contents.resize(1);
				else if (tag & MAX_HEAD)
					contents.resize(2);
				else
					printf("Decorder::refresh error %d\n",tag);
				nowstate = PICK_BODY;
				currentoffset = 0;
			}break;
			case PICK_BODY:
			{
				contents.resize(getbodysize());
				nowstate = END;
				currentoffset = 0;
				//printf("Will take body.size():%u\n",contents.size());
			}break;
			case END:
			{
				nowstate = START;
				contents.resize(0);
				currentoffset = 0;
				tag = 0;
			}break;
		}
	}
	bool Decoder::isFinished()
	{
		// ÂÆåÊï¥Êä•Êñá‰∫?
		return ((nowstate==END) && leftsize() == 0);
	}
	void Decoder::setbodysize(unsigned int size)
	{
		if (tag & MIN_HEAD)
		{
			*(unsigned char*)(&contents[1]) = size;
		}
		if (tag & MAX_HEAD)
		{
			*(unsigned short*)(&contents[1]) = size;
		}
	}
	unsigned int Decoder::getbodysize()
	{
		if (tag & MIN_HEAD)
		{
			return *(unsigned char*)&contents[0];
		}
		if (tag & MAX_HEAD)
		{
			return *(unsigned short*)&contents[0];
		}
		return 0;
	}
	Record * Decoder::getRecord()
	{
		Record *record = new Record(&contents[0],contents.size());
		return record;
	}
	void Decoder::encode(void *data,unsigned int len,bool ziptag,bool destag) // ÊâìÂåÖÂô?
	{
		// ËÆæÁΩÆtag
		tag = 0;
		unsigned int headcontent = 0;
		if (len <= 255)
		{
			tag |= MIN_HEAD;
			ziptag = false; // Â∞èÊä•ÊñáÂº∫Âà∂‰∏çÊâìÂåÖ
			headcontent = 1; // ‰∏Ä‰∏™Â≠óËäÇÈïøÂ∫¶ÁöÑÊä•Êñá
		}
		else
		{
			tag |= MAX_HEAD;
			headcontent = 2;
		}
		if (ziptag) tag |= ZIP;
		if (destag) tag |= DES;
		
		if (ziptag)
		{
			len = zip(data,len,headcontent + 1);
		}
		else
		{
			contents.resize(len + headcontent + 1);
			memcpy(&contents[headcontent+1],data,len);
		}
		if (destag)
		{
			des();
		}
		
		contents[0] = tag;
		setbodysize(len);
	}
	unsigned int Decoder::unzip_size(unsigned int zip_size)
	{
		return zip_size * 120 / 100 + 12;
	}
	unsigned int Decoder::unzip(unsigned char *buffer,unsigned int len,unsigned int offset)
	{
		// Ëß£Âéã
		if (tag & ZIP)
		{
			unsigned int unZipLen = len;
			int retcode = uncompress(buffer,(uLongf*)&unZipLen,&contents[offset],contents.size() - offset);
			return unZipLen;
		}
		return 0;
	}
	
	void Decoder::undes()
	{
		// Ëß£ÂØÜ
		if (tag & DES)
		{
			
		}
	}
	unsigned int Decoder::zip(void *data,unsigned int len,unsigned int offset)
	{
		if (tag & ZIP)
		{
			contents.resize(unzip_size(len));
			unsigned int outlen = 0;
			int retcode = compress(&contents[offset],(uLongf*)&outlen,(const Bytef *)data,len);
			contents.resize(outlen);
			len = outlen;
		}
		return len;
	}
	
	void Decoder::des()
	{
		if (tag & DES)
		{
			
		}
	}
    /**
     * *******************处理消息**************************
     */
    /**
	 * 增加消息处理的句柄
	 * \param type 消息类型
	 * \param T_HANDLE handle 处理句柄
	 **/
	bool Handles::addHandle(unsigned short type,MsgFuncHandler* handle)
	{
		if (type >= tables.size())
		{
			tables.resize(type + 1);
		}
		if (tables[type]) return false;
		tables[type] = handle;
		return true;
	}
	/**
	 * 处理消息
	 * \param object 对象
	 * \param cmd 消息
	 * \param len 消息长度
	 **/
	bool Handles::handle(unsigned short type,void*cmd,unsigned int cmdLen)
    {
		MsgFuncHandler *h = NULL;
		if (type >= tables.size())
		{
			return false;
		}
		h =  tables[type];
		if (h) {
			h->call(cmd,cmdLen);
			return true;
		}
		return false;
    }
    /**
     * Pool 处理器
     **/
	void EventBase::deal(int eventType)
	{
		target->evt = this;
		ev.data.ptr = this;
		set(eventType);
		epoll_ctl(poolHandle,EPOLL_CTL_ADD,target->getHandle(),&ev);
	}
	void EventBase::set(int eventType)
	{
		EventBase::eventType = eventType;
		EventBase::ev.events = 0;
		if (eventType & IN_EVT)
		{
			EventBase::ev.events |= EPOLLIN;
		}
		if (eventType & OUT_EVT)
		{
			EventBase::ev.events |= EPOLLOUT;
		}
		if (eventType & ACCEPT_EVT)
		{
			EventBase::ev.events |= EPOLLIN;
		}
	//	EventBase::ev.events |= EPOLLERR;
	//	EventBase::ev.events |= EPOLLPRI;
		//ev.events |= EPOLLHUP;
	}
	virtual void EventBase::stopWrite()
	{
		set(IN_EVT);
		epoll_ctl(poolHandle,EPOLL_CTL_MOD,target->getHandle(),&ev);
	}
	void EventBase::startRead()
	{
	//	epoll_ctl(poolHandle,EPOLL_CTL_ADD,target->getHandle(),&ev);
	}
	void EventBase::stopRead()
	{
		set(OUT_EVT);
		epoll_ctl(poolHandle,EPOLL_CTL_MOD,target->getHandle(),&ev);
	}
	void EventBase::startWrite()
	{
		set(OUT_EVT|IN_EVT);
		epoll_ctl(poolHandle,EPOLL_CTL_MOD,target->getHandle(),&ev);	
	}
	int EventBase::getPeerHandle()
	{
		return target->getPeerHandle();
	}
	bool EventBase::isOut()
	{
		return (ev.events & EPOLLOUT);
	}
	bool EventBase::isIn()
	{
		return (ev.events & EPOLLIN);
	}
	bool EventBase::isErr()
	{
		return (eventType & ERROR_EVT);
	}
	bool EventBase::isAccept()
	{
		return (eventType & ACCEPT_EVT) && (ev.events & EPOLLIN);
	} 
	void EventPool::init()
	{
		eventBuffer.resize(1024);
		poolHandle = epoll_create(256);
	}	
	void EventPool::bindEvent(Target *target,int eventType)
	{
		Event<Target> *evt = new Event<Target>(target);
		evt->poolHandle = poolHandle;
		evt->deal(eventType); // 构建事件 
	}
	EventBase *EventPool::pullEvent()
	{
		static std::list<EventBase*> events;
		int retcode = epoll_wait(poolHandle,&eventBuffer[0],eventBuffer.size(),-1);
		if (events.empty())
		{
			for (int i = 0; i < retcode;i++)
			{
				EventBase *target = (EventBase*) eventBuffer[i].data.ptr;
				if (target)
				{
					events.push_back(target);
				}
			}
		}
		else
		{
			events.pop_front();
		}
		if (events.empty()) return NULL;
		return events.front();
	}	
	
	  bool Connection::destroy()
	 {
		::close(socket);
		return true;	 
	 }
	  void doGetCommand(void *cmd,unsigned int len){}
	 
	 /**
	  * ·¢ËÍÏûÏ¢
	  */
	 void Connection::sendCmd(void *cmd,unsigned int len)
	 {
		Decoder  decoder;
		decoder.encode(cmd,len);
	//	sends.push_back(decoder.getRecord());
		sends.write(decoder.getRecord());
		if (evt)
			evt->startWrite(); 
	}
	void Connection::sendBuffer(void *cmd,unsigned int len)
	{
		Record *record = new Record(cmd,len);
		sends.write(record);
		if (evt)
		{
			evt->startWrite();
		}	
	}
	int Connection::send(void *cmd,unsigned int len)
	{
		return ::send(socket,cmd,len,0);
	}

	 /**
	  * ½«ÏûÏ¢½ÓÊÜµ½»º´æ
	  **/
	 unsigned int Connection::recv(void *cmd,unsigned int size)
	 {
		unsigned int realcopy = 0;
	//	while (recvs.size())
		while (!recvs.empty())
		{
			//Record *record = recvs.front();
			Record *record = NULL;
			if (recvs.readOnly(record))
			{
				realcopy = record->recv(cmd,size);
				if (record->empty())
				{
					delete record;
					//recvs.pop_front();
					recvs.pop();
				}
				if (realcopy == size)
				{
					return size;
				}
			}else break;
		}
		return realcopy;
	}
	void Connection::doReadBuffer(pool::EventBase *evt,stGetPackage *callback = NULL)
	{
		pool::Event<Connection>* event = static_cast<pool::Event<Connection>*>( evt );
		while(true)
		{
			memset(buffer,0,pool::EventBase::MAX_BUFFER_LEN);
			int leftLen = ::recv(socket,buffer,pool::EventBase::MAX_BUFFER_LEN,0);
			if (leftLen == -1)
			{
				//printf("接受出错拉\n");
				 return;
			}
			if (leftLen == 0)
			{
				evt->eventType = pool::ERROR_EVT;
				return;
			}
			if (directDealCmd) // Ö±½Ó´¦ÀíÏûÏ¢
			{
				if (callback)
					callback->doGetCommand(buffer,leftLen);
				else
					doGetCommand(buffer,leftLen);
			}
			if (leftLen < pool::EventBase::MAX_BUFFER_LEN)
			{
			}
		}
		evt->startRead();
	}

	/**
	 **/
	 void Connection::doRead(pool::EventBase *evt,stGetPackage *callback = NULL)
	{
		pool::Event<Connection>* event = static_cast<pool::Event<Connection>*>( evt );
		while(true)
		{
			memset(buffer,0,pool::EventBase::MAX_BUFFER_LEN);
			int leftLen = ::recv(socket,buffer,pool::EventBase::MAX_BUFFER_LEN,0);
			if (leftLen == -1)
			{
				//printf("---接受数据到头了--\n");
				 return;
			}
			if (leftLen == 0)
			{
				evt->eventType = pool::ERROR_EVT;
				return;
			}
			if (directDealCmd) // Ö±½Ó´¦ÀíÏûÏ¢
			{
				printf("---接受并处理字节数:%d\n",leftLen);
				Record record(buffer,leftLen);
				if (callback)
					decoder.decode(&record,callback);
				else
				{
					decoder.decode(&record,this);
				}
			}
			else
			{
				Record *record = new Record(buffer,leftLen);
				//recvs.push_back(record);
				recvs.write(record);
			}
			if (leftLen < pool::EventBase::MAX_BUFFER_LEN)
			{
		//		break;
			}
		}
		evt->startRead();
	}
	/**
	 * ÔÚpool ÖÐ´¦Àí·¢ËÍ
	 **/
	void Connection::doSend(pool::EventBase *evt)
	{
		bool tag = false;
		pool::Event<Connection>* event = static_cast<pool::Event<Connection>*>( evt );
		event->stopWrite();
		while (!sends.empty())
		{
			Record *record = NULL;
			if (sends.readOnly(record))
			{
				if (record->sendOver(this))
				{
					printf("%s\n","doSend");
					sends.pop();
					delete record;
				}
				else
				{
					 tag = true;
					break;
				}
			}else break;
		}
		if (tag)
		{
			evt->startWrite();
		}
	}

	void Client::init(const char *ip,unsigned short port)
	{
		socket = ::socket(AF_INET,SOCK_STREAM,0);
		if(socket == -1)
		{
			// TODO error
		}
		memset(&addrServer,0,sizeof(sockaddr_in));
		addrServer.sin_family = AF_INET;
		addrServer.sin_addr.s_addr = inet_addr(ip);
		addrServer.sin_port = htons(port);
		this->peerIp = ip;
		setnonblock(socket);
		if(connect(socket,(const struct sockaddr *)&addrServer,sizeof(sockaddr)) != 0)
		{
			//printf("connect error! -->%s:%u\n",ip,port);	
		}

	}
	void Client::setnonblock(int socket)
	{
		int opts;
		opts=fcntl(socket,F_GETFL);
		if(opts<0)
		{
			return;
		}
		opts = opts|O_NONBLOCK;
		if(fcntl(socket,F_SETFL,opts)<0)
		{
			return;
		} 
	}
	bool Client::destroy()
	{
		if (socket != -1)
			::close(socket);
		socket = -1;
		return false;	 
	}
	void Server::init(const char *ip,unsigned short port)
	{
		struct sockaddr_in ServerAddress;

		socket = ::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		setnonblock(socket);
		int reuse = 1;
		setsockopt(socket,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
	
		memset((char *)&ServerAddress,0, sizeof(ServerAddress));
		ServerAddress.sin_family = AF_INET;
		ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(ip);         
		ServerAddress.sin_port = htons(port);                          

		bind(socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)); 
		
		listen(socket,4026);
	}
	void Server::setnonblock(int socket)
	{
		int opts;
		opts=fcntl(socket,F_GETFL);
		if(opts<0)
		{
			return;
		}
		opts = opts|O_NONBLOCK;
		if(fcntl(socket,F_SETFL,opts)<0)
		{
			return;
		} 
	}
	int Server::getPeerHandle(){
		socklen_t clilen = 1024;
		struct sockaddr_in addr;
		int con = ::accept(socket,(struct sockaddr*)(&addr),&clilen);	
		printf("accept:ip:%sport:%u",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));
		setnonblock(con);
		return con;
	}	
}
#endif