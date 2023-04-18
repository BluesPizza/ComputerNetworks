#include"UDP_Reli.h"
using namespace std;
#define LOOPIP "127.0.0.1" //本机回环地址
#define CLIENTPORT 7654
#define SERVERPORT 7655
SOCKET client;
timeval timeout; //非阻塞socket延时
SOCKADDR_IN cliIp_C;
SOCKADDR_IN serIp_C;
uint32_t cliSeq = 0;
uint8_t status_C = 0;
char sendbuff[4096][MSG_SIZE]; //单次最大文件大小不超过4KB 文件总大小不超过16MB
//建立连接
int clientConnect() {
	UDP_Reli u, v;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_SYN); // Syn请求置位 
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq) == 1){
		printf("[日志]SYN请求成功\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		printf("[提示]建立连接成功\n");
		return 1;
	}
	return 0;
}
//断开连接
int clientFin() {
	UDP_Reli u, v;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_FIN);
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq)) {
		printf("[日志]FIN请求成功\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		printf("[提示]断开连接成功\n");
		return 1;
	}
	printf("[警告]断开连接失败\n");
	return 0;
}

//读取目标文件
int getFile(char* path, uint16_t& len, uint16_t& count) {
	count = len = 0;
	ifstream ifs;
	ifs.open(path,ios::in | ios::binary); // 读+二进制
	if (!ifs.is_open()) {
		printf("[警告]文件读取失败\n");
		return 0;
	}
	char tmp = ifs.get();
	while (!ifs.eof()) { //读到文件末尾
		sendbuff[count][len % MSG_SIZE] = tmp;
		len++;
		if (len % MSG_SIZE == 0) {
			len = 0;
			count++;
		}
		tmp = ifs.get();
	}
	ifs.close();
	return 1;
}

int sendFile(char* path) {
	int i;
	//清空缓存区备用
	for (i = 0; i < 4096; i++) {
		memset(sendbuff[i], 0, MSG_SIZE);
	}
	uint16_t len, count;
	len = count = 0;
	getFile(path, len, count);
	UDP_Reli u, v;
	u.setCounts(count);
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_STR); //开始标志位
	u.setLen(len); //长度仅16位 不足30M 所以第一个包的长度段我们告知最后一个包的长度 这样通过count即可以算出总长度
	memcpy(u.dataBuf, path, strlen(path));//第一个包不传输数据，只是告知要传输文件了
	time_t start = clock();
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq) == 0) {
		printf("[警告]建立文件传输失败\n");
		return 0;
	}
	printf("[日志]STATE_STR报文已被接收\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
	for (i = 0; i <= count; i++) {
		UDP_Reli tmp;
		tmp.setSrc(cliIp_C);
		tmp.setDst(serIp_C);
		tmp.setCounts(count);
		if (i != count) { //除最后一个包外 totalLen都是MSG_SIZE
			tmp.setLen(MSG_SIZE);
			for (int j = 0; j < MSG_SIZE; j++) {
				tmp.dataBuf[j] = sendbuff[i][j];
			}
		}
		else {
			tmp.setFlags(FLAG_END);
			tmp.setLen(len);
			for (int j = 0; j < len; j++) {
				tmp.dataBuf[j] = sendbuff[i][j];
			}
		}
		if(stopAndWaitSend(tmp, v, client, serIp_C, serIp_C, cliSeq) == 0) {
			printf("[警告]文件传输中断\n");
			return 0;
		}
		if (tmp.getFlags() & FLAG_END) {
			printf("[日志]STATE_END报文已被接收\nseq=%u, ack=%u, checksum=%u\n", tmp.getSeq(), tmp.getAck(), tmp.getCheckSum());
		}
	}
	time_t end = clock();
	double fileSize = (double)(count * (sizeof(UDP_Reli) - 28) + len) / 1e6;
	double time = (double)(end - start) / CLOCKS_PER_SEC;
	double rate = fileSize * 8.0 / time;
	printf("[日志]传输完毕，耗时:%lfS\n文件大小:%lfMB\n传输速率:%lfMbps\n", time, fileSize, rate);
	return 1;
}

int main(int argc, char* argv[]) {
	//初始化全局发送缓存区
	for (int i = 0; i < 4096; i++) {
		memset(sendbuff[i], 0, MSG_SIZE);
	}

	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	if (WSAStartup(ver, &data) != 0) {
		cout << "[警告]初始化socket库失败" << endl;
		return 0;
	}
	cout << "[提示]WSAStartup成功\n";

	if ((client = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		cout << "[警告]初始化socket失败" << endl;
		return 0;
	}
	cout << "[提示]Socket创建成功\n";

	timeout.tv_sec = 2; //s
	timeout.tv_usec = 0;  //ms
	//套接字描述符 + 被设置的选项的级别必须为SOL_SOCKET + 指定准备设置的选项名
	if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) { 
		cout << "[警告]更改sockt配置失败" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &cliIp_C.sin_addr.S_un.S_addr);
	cliIp_C.sin_family = AF_INET;
	cliIp_C.sin_port = htons(CLIENTPORT);
	inet_pton(AF_INET, LOOPIP, &serIp_C.sin_addr.S_un.S_addr);
	serIp_C.sin_family = AF_INET;
	serIp_C.sin_port = htons(SERVERPORT);

	cout << "[提示]客户端准备就绪" << endl;
	char flag;
	char path[50];
	while (1) {
		memset(path, 0, sizeof(path)); //每次重置掉上次的path 否则可能因为输入长度不同导致路径出错
		cout << "[提示]请输入文件路径" << endl;
		cin >> path;
		if (status_C == STATUS_OFF) {
			cout << "[提示]开始建立连接" << endl;
			if (clientConnect() == 1) {
				status_C = STATUS_ON;
				sendFile(path);
			}
			else {
				cout << "[警告]建立连接失败" << endl;
			}
		}
		else {
			sendFile(path);
		}
		cout << "[警告]是否继续程序(y/n)" << endl;
		cin >> flag;
		if (!(flag == 'y' || flag == 'Y')) {
			cliSeq = 0;
			status_C = STATUS_OFF;
			if (clientFin() == 1) break;
			else { // 如果在服务器保存文件成功之前发出断连请求 服务器不会回复直到保存完后 因此无需始终等待 强制退出即可
				cout << "[警告]服务器可能正在保存文件 即将强制退出" << endl;   
				break;
			}
		}
	}

	closesocket(client);
	WSACleanup();
	return 0;
}