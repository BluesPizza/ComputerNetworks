#include"UDP_Reli.h"
using namespace std;
#define LOOPIP "127.0.0.1" //本机回环地址
#define SERVERPORT 7655
#define CLIENTPORT 7654
SOCKET server;
timeval timeout; //非阻塞socket延时
SOCKADDR_IN serIp_S;
SOCKADDR_IN cliIp_S;
uint32_t serSeq = 0;
uint8_t status_S = 0;
char recvbuff[4096][MSG_SIZE]; //单次最大文件大小不超过4KB 文件总大小不超过16MB
//建立连接
int serverConnect(UDP_Reli& u) {
	UDP_Reli v;
	v.setSrc(serIp_S);
	v.setDst(u.getSrc());
	if (u.getFlags() & FLAG_EXIST) {
		printf("[日志]收到来自客户端的SYN请求\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		v.setAckSeq(u);
		v.setSeq(serSeq++);
		if (UDPsend(v, server, cliIp_S) == 0) return 0;
		printf("[提示]建立连接成功\n");
		return 1;
	}
	printf("[警告]建立连接失败\n");
	return 0;
}
//断开连接
int serverFin(UDP_Reli u) {
	UDP_Reli v;
	v.setSrc(serIp_S);
	v.setDst(u.getSrc());
	if (u.getFlags() & FLAG_EXIST) {
		printf("[日志]收到来自客户端的FIN请求\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		v.setAckSeq(u);
		v.setSeq(serSeq++);
		if (UDPsend(v, server, cliIp_S) == 0) return 0;
		printf("[提示]确认断连成功\n");
		return 1;
	}
	else return 0;
}

int recvFile(UDP_Reli& msg) { //msg是STATE_STR包
	int i = 0;
	for (i = 0; i < 4096; i++) { //清空全局缓存区防止上次的数据干扰
		memset(recvbuff[i], 0, MSG_SIZE);
	}
	uint16_t count, len;
	UDP_Reli reply;
	reply.setAckSeq(msg);
	reply.setSrc(serIp_S);
	reply.setDst(msg.getSrc());
	UDPsend(reply, server, cliIp_S);
	count = msg.getCounts();
	len = msg.getLen();
	printf("[日志]接收STATE_STR报文\nseq=%u, ack=%u, checksum=%u\n", msg.getSeq(), msg.getAck(), msg.getCheckSum());
	printf("[提示]接收文件名为:");
	i = 0;
	while (msg.dataBuf[i]) {
		printf("%c", msg.dataBuf[i]);
		i++;
	}
	printf("\n");
	for (i = 0; i <= count; i++) {
		UDP_Reli u, v;
		v.setSrc(serIp_S);
		v.setDst(msg.getSrc());
		if (stopAndWaitRecv(u, v, server, cliIp_S, cliIp_S, serSeq) == 1) { //停等
			if (i == count) {
				for (int j = 0; j < len; j++) recvbuff[i][j] = u.dataBuf[j];
			}
			else {
				for (int j = 0; j < MSG_SIZE; j++) recvbuff[i][j] = u.dataBuf[j];
			}
			if ((i + 1) % 50 == 0) {
				printf("[日志]收到第%d个文件包\nseq=%u, ack=%u, checksum=%u\n", i + 1, u.getSeq(), u.getAck(), u.getCheckSum()); //每50个报文打印一次日志
			}
		}
		else {
			printf("[警告]传输过程中出错\n");
			return 0;
		}

		if (i == count) {
			if (!(u.getFlags() & FLAG_END) || !(u.getFlags() & FLAG_EXIST)) { //最后一个包应当是FLAG_END
				printf("[警告]接收到的文件有误\n");
				return 0;
			}
			else {
				printf("[日志]接收STATE_END报文\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
			}
		}
	}

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
	for (i = 0; i <= count; i++)
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
	status_S = status_S & (uint8_t)~(STATUS_SENDING); //结束sending状态
	return 1;
}

int main(int argc, char* argv[]) {
	//初始化全局接收缓存区
	for (int i = 0; i < 4096; i++) {
		memset(recvbuff[i], 0, MSG_SIZE);
	}

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

	timeout.tv_sec = 2; //s
	timeout.tv_usec = 0;  //ms
	//套接字描述符 + 被设置的选项的级别必须为SOL_SOCKET + 指定准备设置的选项名
	if (setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) {
		cout << "[警告]更改sockt配置失败" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &serIp_S.sin_addr.S_un.S_addr);
	serIp_S.sin_family = AF_INET;
	serIp_S.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, LOOPIP, &cliIp_S.sin_addr.S_un.S_addr);
	cliIp_S.sin_family = AF_INET;
	cliIp_S.sin_port = htons(SERVERPORT);


	if (bind(server, (sockaddr*)&serIp_S, sizeof(sockaddr)) == -1) {
		cout << "[警告]bind失败" << endl;
		return 0;
	}
	cout << "[提示]bind成功" << endl;
	cout << "[提示]server已就绪" << endl;

	char flag;
	while (1) {
		UDP_Reli u;
		UDPrecv(u, server, cliIp_S);
		if (u.getFlags() & FLAG_EXIST && u.getSrc().sin_addr.S_un.S_addr != 0) { //防止空报文干扰 会频繁打印信息并干扰正常信息接收
			if (u.getFlags() & FLAG_SYN) {
				if (serverConnect(u) == 1) status_S = STATUS_ON;
			}
			else if ((u.getFlags() & FLAG_FIN)) {
				cout << "[提示]收到断开连接请求" << endl;
				if (serverFin(u)) {
					status_S = STATUS_OFF;
					serSeq = 0;
					cout << "[警告]是否继续程序(y/n)" << endl;
					cin >> flag;
					if (!(flag == 'y' || flag == 'Y')) break;
				}
				else cout << "[警告]未发现目标连接" << endl;
			}
			else if (u.getFlags() & FLAG_STR) {
				if (status_S == STATUS_ON) {
					status_S = status_S | STATUS_SENDING;
					cout << "[提示]开始接受文件" << endl;
					recvFile(u);
				}
				else cout << "[警告]尚未连接" << endl;
			}
		}
	}


	closesocket(server);//关闭socket
	WSACleanup();
	return 0;
}