// C++ ʹ��ģ��
CocosNet::getMe().init();
CocosNet::getMe().newClient(1,"112.124.26.245",5050);
LuaReqData *data= LuaReqData::create();
data->content = "hello,world";
CocosNet::getMe().sendtoServer(1,data);


-- ��������ģ�� luaʹ��ģ��
sync.init()
sync.req({reqData=1},function(content)
    print(content)
end)
sync.req({reqData=2},function(content)
    print(content)
end)
local obj = "ewewe"
sync.req({idData=2},function(content)
    print(obj)
end)

sync.send(1,{}) -- ������Ϣ
sync.with(2,function(data) -- ����ָ���ķ�����Ϣ

end)