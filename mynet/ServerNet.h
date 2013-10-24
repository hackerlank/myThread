//
//  CocosNet.h
//  Dragon
//
//  Created by GameDeveloper on 13-10-20.
//
//


#include "mynet.h"
#include "cmdobject.h"
class ServerNet;
class IO{
public:
	void start(ServerNet *net);
};
class ServerNet:public mynet::Handles,public mynet::stGetPackage{
public:
	ServerNet()
	{
	}
    ~ServerNet()
    {}
    void newClient(int index,const char *ip,unsigned short port);
	void newServer(unsigned short port);
	void updateClient(int index);
	void init();
	void stop();
    static CocosNet * get()
    {
        return new CocosNet();
    }
	void go();
    void sendtoServer(int index,LuaReqData *reqData);
public:
    IO io;
    mynet::EventPool *pool;
    std::vector<mynet::Client *> clients;
	std::vector<mynet::Server *> servers;
	static CocosNet & getMe()
	{
		static CocosNet t;
		return t;
	}
    void updateTimer(float dt);
	void bind(unsigned short commandID,mynet::MsgFuncHandler *handler)
	{
		addHandle(commandID,handler);
	}
    /**
	 * 
	 */
	void run();
	bool valid;
    
	void sendCmd(int index,void *cmd,unsigned int len);
private:
	void handle(void *cmd,unsigned int len);
    void doGetCommand(void *cmd,unsigned int len)
	{
		pushCommands(new mynet::Record(cmd,len));
	}
	void pushCommands(mynet::Record *record)
	{
		records.write(record); 
	}
	mynet::MyList<mynet::Record*> records;
	/**
     * ��������Ϣ   
	 */
	void popCommands()
	{
		mynet::Record * record = NULL;
		if (records.readAndPop(record))
		{
			handle(record->contents,record->contentSize); //
			delete record;
		}
		doSendFile();
	}
public:
	/**
	 * ȷ���ļ���С �� ��ʼֵ
	 */
	void sendFile(int index,const char *fileName);
	/**
	 * �����ļ��ķ���
	 */
	mynet::MyList<stWaitSendFileInfo*> files;
	/**
	 * ʵ�ʷ����ļ�
	 **/
	void doSendFile();

private:
    void sendObject(int index,CmdObject *object)
    {
        cmd::Stream ss = object->toStream();
        sendCmd(index,ss.content(),ss.size());
    }
	
    
    std::vector<stReqData*> reqDatas;
    std::set<int> emptyIDs; // ����ID��
    stReqData * getReqData(int reqId)
    {
        if (reqId < reqDatas.size())
        {
            return reqDatas[reqId];
        }
        return NULL;
    }
    void setReqData(stReqData *data)
    {
        if (emptyIDs.size())
        {
            int id = *emptyIDs.begin();
            emptyIDs.erase(emptyIDs.begin());
            reqDatas[id] = data;
            data->reqId = id;
        }else{
            data->reqId = reqDatas.size();
            reqDatas.push_back(data);
        }
    }
    void removeReqData(stReqData *data)
    {
        emptyIDs.insert(data->reqId);
        reqDatas[data->reqId] = NULL;
    }
};


#endif
