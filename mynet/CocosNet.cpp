//
//  CocosNet.cpp
//  Dragon
//
//  Created by GameDeveloper on 13-10-20.
//
//

#include "CocosNet.h"
#include "mythread.h"
#if (0)
#include "CCLuaEngine.h"
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "tolua++.h"
#include "tolua_fix.h"
}
#endif
class WorkThread:public thread::Thread{
public:
	WorkThread():thread::Thread("WORKTHREAD"){
		net = NULL;
	}
    
	void run()
	{
		net->run();
	}
	CocosNet *net;
};
void IO::start(CocosNet *net)
{
	WorkThread * work = new WorkThread;
	work->net = net;
	work->start();
}
using namespace cocos2d;
using namespace mynet;
void CocosNet::newClient(int index,const char *ip,unsigned short port)
{
    if (index < clients.size())
    {
        mynet::Client *client = clients[index];
        if (client)
        {
            if (!client->checkValid())
            {
                client->reconnect();
                pool->bindEvent(client,IN_EVT|OUT_EVT);
            }
            return;
        }
    }
    mynet::Client *client = new mynet::Client(ip,port);
	
	pool->bindEvent(client,IN_EVT|OUT_EVT);
    
    if (index >= clients.size())
    {
        clients.resize(index + 1);
    }
    clients[index] = client;
}
void CocosNet::updateClient(int index)
{
    if (index < clients.size())
    {
        mynet::Client *client = clients[index];
        if (client)
        {
            if (!client->checkValid())
            {
                client->reconnect();
                pool->bindEvent(client,IN_EVT|OUT_EVT);
            }
            return;
        }
    }
}

void CocosNet::updateTimer(float dt)
{
    popCommands();
}

void CocosNet::init()
{
    if (valid) return;
    if (pool) return;
	valid = true;
    pool = new EventPool();
	pool->init();
    cocos2d::CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(CocosNet::updateTimer), this, 1.0f/15.0f, false);
    io.start(this);
}
void CocosNet::sendCmd(int index,void *cmd,unsigned int len)
{
    if (index < clients.size())
    {
        mynet::Client *client = clients[index];
        if (client)
            client->sendCmd(cmd,len);
    }
}
void CocosNet::stop()
{
    cocos2d::CCDirector::sharedDirector()->getScheduler()->unscheduleSelector(schedule_selector(CocosNet::updateTimer),this);
    valid = false;
    
}

/**
 * 
 */
void CocosNet::run()
{
#if (1)
	while (valid) // 
	{
		EventBase *evt = pool->pullEvent();
		if (evt)
		{
			if (evt->isErr()) // 
			{
				Connection *conn = (Connection*) evt->target;
				if (conn)
				{
					conn->destroy();
					//delete conn;
				}
				continue;
			}
			if (evt->isOut()) //
			{
				Connection *connEvt = (Connection*) evt->target;
				if (evt->target)
				{
					connEvt->doSend(evt);
				}
			}
			if (evt->isIn()) //
			{
				Connection *connEvt = (Connection*) evt->target;
				if (evt->target)
				{
					connEvt->doRead(evt,this);
				}
			}
            
		}
	}
#endif
}

void CocosNet::sendtoServer(int index,LuaReqData *reqData)
{
    this->updateClient(index);
    protoStringCmd cmd;
	cmd.reqID = reqData->reqId;
    setReqData(reqData);
    
    cmd.content = reqData->content;
    reqData->retain();
    sendObject(index, &cmd);
}

void CocosNet::handle(void *cmd, unsigned int len)
{
	  protoStringCmd stringCmd;
    stringCmd.parsecmd(cmd, len);
    if (stringCmd.__msg__id__ == 1)
	{
		printf("%s",stringCmd.content.c_str());
	}
#if (0)
	// ����ص�
    protoStringCmd stringCmd;
    stringCmd.parsecmd(cmd, len);
    if (stringCmd.__msg__id__ == 1)
    {
        LuaReqData *reqData = (LuaReqData*)getReqData(stringCmd.reqID);
        if (reqData)
        {
            // lua ����ʽ
            lua_getglobal(CCLuaEngine::defaultEngine()->getLuaStack()->getLuaState(), "doLuaNetCallback");
            if (!lua_isfunction(CCLuaEngine::defaultEngine()->getLuaStack()->getLuaState(), -1))
            {
                CCLOG("[LUA ERROR] name '%s' does not represent a Lua function", "doLuaNetCallback");
                lua_pop(CCLuaEngine::defaultEngine()->getLuaStack()->getLuaState(), 1);
                return;
            }
    
            tolua_pushusertype(CCLuaEngine::defaultEngine()->getLuaStack()->getLuaState(), reqData, "LuaReqData");
            CCLuaEngine::defaultEngine()->getLuaStack()->pushString(stringCmd.content.c_str());
            int ret = CCLuaEngine::defaultEngine()->getLuaStack()->executeFunction(2);
            CCLuaEngine::defaultEngine()->getLuaStack()->clean();


            removeReqData(reqData);
            reqData->release();
        }
    }
#endif
}
static long filesize(FILE *stream)
{
	long curpos, length;
	curpos = ftell(stream);
	fseek(stream, 0L, SEEK_END);
	length = ftell(stream);
	fseek(stream, curpos, SEEK_SET);
	return length;
}
/*
* ȷ���ļ���С �� ��ʼֵ
*/
void CocosNet::sendFile(int index,const char *fileName)
{
	stWaitSendFileInfo* info = new stWaitSendFileInfo();
	info->fileName = fileName;
	std::string realpath = CCFileUtils::sharedFileUtils()->fullPathFromRelativePath(fileName);
	info->netIndex = index;
	FILE *file = fopen(realpath.c_str(),"rb");
	if (!file)
	{
		delete info;
		return;
	}
	info->fileSize = filesize(file);
	fclose(file);
	files.write(info);
}

void CocosNet::doSendFile()
{
	stWaitSendFileInfo* info;
	if (files.readOnly(info))
	{
		// ����8192 ��С�����ݿ�
		if (info->offset >= info->fileSize)
		{
			files.pop();
			delete info;
		}
		else
		{
			int left = info->fileSize - info->offset;
			if (left >= 8192) left = 8192;
			// ����left ����
			std::string realpath = CCFileUtils::sharedFileUtils()->fullPathFromRelativePath(info->fileName.c_str());
			FILE *file = fopen(realpath.c_str(),"rb");
			if (!file) return;
			fseek(file,info->offset,SEEK_CUR);
			stSendFileInfo cmd;
			cmd.fileName = info->fileName;
			cmd.buffer.resize(left);
			cmd.offset = info->offset;
			fread(&cmd.buffer[0],left,1,file);
			sendObject(info->netIndex,&cmd);
			fclose(file);
			info->offset += left;
		}
	}
}