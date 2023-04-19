#include"UDP_Reli.h"
using namespace std;
#define TIMEOUT_SERVER 50
#define ACC_COUNT 1 // 累计确认计数 每4条消息回传一个ACK
SOCKET server;
timeval timeout_S; //非阻塞socket延时
SOCKADDR_IN serIp_S;
SOCKADDR_IN cliIp_S;
char recvbuff[MSG_COUNT][MSG_SIZE]; //4K*4K 即单次最大报文长度大小不超过4KB 单个文件总大小不超过16MB
UDP_Reli recvList[MSG_COUNT];  // 发送队列 长度是最大接收缓存区可接受的最大报文数量4k
uint16_t serWinNext = 0; // 下一条要处理的消息位置
uint16_t fileCount = 0; //计数在recvList的何位置
uint8_t status_S = 0;
HANDLE recvHD_R; // 接收线程

void printSerWindow() {
	printf("[日志]serWinNext:%u WindowSize:%d\n",serWinNext, ACC_COUNT); 
}

// 接收线程 回调函数原型写法是固定的
// 把收到的报文的数据部分写入本地缓存区
DWORD WINAPI recvFile(LPVOID lparam) { 
	UDP_Reli_Thread* file = (UDP_Reli_Thread*)(LPVOID)lparam;
	memset(recvbuff[file->count], 0, MSG_SIZE);
	for (int i = 0; i < file->totalLen; i++) {
		recvbuff[file->count][i] = file->msg.dataBuf[i];
	}
	return 1;
}

// 数据写入本地文件
int writeFile(uint16_t& len, uint16_t& count) {
	char path[50];
	memset(path, 0, sizeof(path));
	printf("[提示]请输入保存完整路径 \n");
	cin >> path;
	ofstream ofs;
	ofs.open(path, ios::binary);
	if (!ofs.is_open()) {
		printf("[警告]文件路径非法\n");
		ofs.close();
		return 0;
	}
	for (int i = 0; i <= count; i++)
	{
		if (i != count) {
			for (int j = 0; j < MSG_SIZE; j++) ofs << recvbuff[i][j];
		}
		else {
			for (int j = 0; j < len; j++) ofs << recvbuff[i][j];
		}
	}
	ofs.close();
	printf("[提示]文件保存成功\n");
	return 1;
}


int main() {
	//初始化全局接收缓存区
	for (int i = 0; i < MSG_COUNT; i++) {
		memset(recvbuff[i], 0, MSG_SIZE);
	}
	memset(recvList, 0, MSG_COUNT);
	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	if (WSAStartup(ver, &data) != 0) {
		cout << "[警告]初始化socket库失败" << endl;
		return 0;
	}
	cout << "[提示]WSAStartup成功\n";

	if ((server = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		cout << "[警告]初始化socket失败" << endl;
		return 0;
	}
	cout << "[提示]Socket创建成功\n";

	timeout_S.tv_sec = 2; //s
	timeout_S.tv_usec = 0;  //ms
	//套接字描述符 + 被设置的选项的级别必须为SOL_SOCKET + 指定准备设置的选项名
	if (setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_S, sizeof(timeout_S)) == -1) {
		cout << "[警告]更改sockt配置失败" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &serIp_S.sin_addr.S_un.S_addr);
	serIp_S.sin_family = AF_INET;
	serIp_S.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, LOOPIP, &cliIp_S.sin_addr.S_un.S_addr);
	cliIp_S.sin_family = AF_INET;
	cliIp_S.sin_port = htons(CLIENTPORT);

	if (bind(server, (sockaddr*)&serIp_S, sizeof(sockaddr)) == -1) {
		cout << "[警告]bind失败" << endl;
		return 0;
	}
	cout << "[提示]bind成功" << endl;
	cout << "[提示]server已就绪" << endl;

	bool timeoutTest = true; //发包测试用延时标志位 将造成FLAG_STR报文以及后续报文重传
	time_t start = clock();//计时器 一定时间未收到包将中断连接
	time_t end;
	while (1) {
		char flag; //是否退出
		bool ifTimeOut = false; //确认是否超时断开
		bool ifCont = false; // 确认是否续用
		int countAck = ACC_COUNT - 1; //累计收到报文计数
		uint16_t count=0;  //传给线程 告知目前要写的报文是有效文件中的第几个包
		uint16_t len=0;    //传给线程 告知目前要写的报文的数据段长度
		int i = 0; //用来遍历的临时变量
		printf("[提示]服务器已复位");
		while (1) {
			int countReAck = 3; //累计未捕捉到有效报文计数 每3次校验错误 重传一次ACK
			UDP_Reli tmpRecv;
			UDPrecv(tmpRecv, server, cliIp_S);
			if (tmpRecv.getFlags() & FLAG_EXIST && tmpRecv.getSrc().sin_addr.S_un.S_addr != 0) {
				//检查报文是否有效 + 报文来源正确
				if (tmpRecv.check() && tmpRecv.getSeq() == serWinNext) {
					//检查校验和 + 得到的包序列号是否是期望的序列号 有效的包才能放进报文缓存队列
					recvList[serWinNext] = UDP_Reli(tmpRecv);
					UDP_Reli u;
					u.setFlags(FLAG_ACK);
					u.setSrc(serIp_S);
					u.setDst(cliIp_S);
					u.setAck(serWinNext);
					u.setSeq(serWinNext);
					if (status_S & STATUS_SENDING && !(tmpRecv.getFlags() & FLAG_STR)) { //在SENDING状态下，可选择每ACC_COUNT个报文回传一次ACK 切勿超过窗口大小
						countAck++;
						if (countAck % ACC_COUNT == 0 || tmpRecv.getFlags() & FLAG_END) {
							UDPsend(u, server, cliIp_S);
							printf("[调试]cliSeq=%u", recvList[serWinNext].getSeq());
							printSerWindow();
							countAck = 0;
						}
					}
					else {
						UDPsend(u, server, cliIp_S);
						printf("[调试]cliSeq=%u", recvList[serWinNext].getSeq());
						printSerWindow();
					}
					//printf("[调试]cliSeq=%u", recvList[serWinNext].getSeq());
					//printSerWindow();
					start = clock(); //刷新计时器
					if (status_S == STATUS_OFF && (recvList[serWinNext].getFlags() & FLAG_SYN)) { //FLAG_SYN
						// 未连接状态 + 收到建连请求
						status_S = STATUS_ON;  
						printf("[提示]建立连接成功\n");
					}
					else if (status_S & STATUS_ON && (recvList[serWinNext].getFlags() & FLAG_FIN)) { //FLAG_FIN
						status_S = STATUS_OFF;
						printf("[提示]断开连接成功\n");
						break;
					}
					else if (status_S & STATUS_ON && !(status_S & STATUS_SENDING) && (recvList[serWinNext].getFlags() & FLAG_STR)) { //FLAG_STR
						status_S |= STATUS_SENDING;
						if (recvList[serWinNext].getFlags() & FLAG_CONTINUE) { //检查第一个FLAG_STR报文中是否带有CONTINUE标记 这关系着是否需要回复FIN
							ifCont = true;
						}
						if (timeoutTest) { //为了展示重传 对于第一个FLAG_STR报文不进行回复
							Sleep(2500);
							timeoutTest = false;
						}
						printf("[提示]开始传输文件\n");
						printf("[提示]接收文件名为:");
						count = recvList[serWinNext].getCounts();
						len = recvList[serWinNext].getLen();
						while (recvList[serWinNext].dataBuf[i]) {
							printf("%c", recvList[serWinNext].dataBuf[i]);
							i++;
						}
						printf("\n");
					}
					else if (status_S & STATUS_ON && status_S & STATUS_SENDING && (recvList[serWinNext].getFlags() & FLAG_END)) { //FLAG_END
						status_S = status_S & ~(STATUS_SENDING);
						if (fileCount != count) { //如果加上最后一个包countNow最后应该为count+1
							printf("[警告]接收到的文件有误\n");
							return 0;
						}
						UDP_Reli_Thread* ut = new UDP_Reli_Thread;
						ut->count = count;  
						ut->totalLen = len;
						ut->msg = recvList[serWinNext];
						/*
						* lpThreadAttributes	内核对象的安全属性，一般传入NULL    
						* dwStackSize,			线程栈空间大小,传入0表示使用默认大小
						* lpStartAddress,		新线程所执行的线程函数地址
						* lpParameter,			传给线程函数的参数
						* dwCreationFlags,		指定额外的标志来控制线程的创建,0表示线程创建之后立即就可以进行调度
						* lpThreadId			返回线程的ID号，传入NULL表示不需要返回该线程ID号
						*/
						recvHD_R = CreateThread(NULL, NULL, recvFile, LPVOID(ut), 0, NULL);
						fileCount++;
						fileCount = 0;
						printf("[提示]文件传输完毕\n");
						if (ifCont) break;
					}
					else if (status_S & STATUS_ON && status_S & STATUS_SENDING) { //文件分包
						fileCount++;
						UDP_Reli_Thread* ut = new UDP_Reli_Thread;
						ut->msg = recvList[serWinNext];
						ut->count = fileCount - 1;
						ut->totalLen = MSG_SIZE;
						recvHD_R = CreateThread(NULL, NULL, recvFile, LPVOID(ut), 0, NULL);
					}
					serWinNext++;
				}
				else {
					printf("[提示]触发重传ACK\n");
					printf("[调试]CliSeq=%u\n", tmpRecv.getSeq());
					countReAck++;
					if (countReAck % 4 == 0) { //返回刚回复过的ACK号
						UDP_Reli re;
						re.setAck(serWinNext - 1);
						re.setFlags(FLAG_ACK);
						re.setFlags(FLAG_RESEND);
						re.setSrc(serIp_S);
						re.setDst(cliIp_S);
						re.setSeq(serWinNext - 1);
						UDPsend(re, server, cliIp_S);
						countReAck = 1;
					}
				}
			}
			else {
				end = clock();
				if (status_S & STATUS_ON && ((1.0 * (end - start)) / CLOCKS_PER_SEC) >= TIMEOUT_SERVER && serWinNext != 0) { // 超时50S自动断开
					status_S = STATUS_OFF;
					printf("[警告]超时断开\n");
					ifTimeOut = true;
					break;
				}
			}
		}
		if (!ifTimeOut) { 
			DWORD dwRet1 = WaitForSingleObject(recvHD_R, INFINITE);
			writeFile(len, count);
			printf("[提示]保存结束，是否继续Y/N\n");
		}
		else //超时断开
		{
			printf("[提示]是否继续Y/N\n");
		}
		cin >> flag;
		if (flag != 'y' && flag != 'Y') break;
		//初始化全局接收缓存区
		for (int i = 0; i < MSG_COUNT; i++) {
			memset(recvbuff[i], 0, MSG_SIZE);
		}
		memset(recvList, 0, MSG_COUNT);
		serWinNext = 0; //缓存区重置
	}
	closesocket(server);//关闭socket
	WSACleanup();
	return 0;
}