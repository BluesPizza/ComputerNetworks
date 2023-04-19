#include"UDP_Reli.h"
using namespace std;
SOCKET client;
timeval timeout_C; //非阻塞socket延时
SOCKADDR_IN serIp_C;
SOCKADDR_IN cliIp_C;
char sendbuff[MSG_COUNT][MSG_SIZE]; //本地发送缓存区 单次最大文件大小不超过4KB 文件总大小不超过16MB
UDP_Reli sendList[MSG_COUNT];  // 发送队列
uint16_t cliWinNext = 0;  // 下一条即将发送出的消息的编号（窗口尾部）
uint16_t base = 0;		  // 窗口头部
uint16_t Eof = 0;		  // 发送队列结尾（文件尾+1）
HANDLE recvTD_C;		  // 接收线程
HANDLE sendTD_C;		  // 发送线程
uint8_t status_C = 0;
// 打印日志 窗口情况
void printCliWindow() {
	printf("[日志]base:%u cliWinNext:%u WindowSize:%d\n", base, cliWinNext, WIN_SIZE);
}

//接收线程
DWORD WINAPI recvHD(LPVOID lparam) {
	printCliWindow();
	while (base < Eof) {
		if (cliWinNext > base + WIN_SIZE || base >= cliWinNext) { // 窗口满或空
			Sleep(3 * TIMEOUT); // 等待
		}
		UDP_Reli u;
		UDPrecv(u, client, serIp_C);
		if (u.check() && u.getFlags() & FLAG_EXIST && u.getFlags() & FLAG_ACK && u.getSrc().sin_addr.S_un.S_addr != 0) { // 检查checkSum 有效位 ACK位
			if (u.getAck() >= base) { //  如果收到u.getAck() > base 可能是ACK收丢了 直接默认之间的包都已经收到
				printf("[调试]ACK=%u", u.getAck());
				printCliWindow();
				base = u.getAck() + 1; //累积确认
			}
			else {
				printCliWindow();
				printf("[日志]错误ACK为%u\n", u.getAck());
				printf("[警告]未收到预期ACK报文\n");
			}
		}
	}
	return 1;
}

//发送线程
DWORD WINAPI sendHD(LPVOID lparam) {
	int resend = 0; //重传计数
	bool timeOutFlag = false; //超时标志
	time_t start = clock();
	time_t end;
	while (base < Eof) {
		if (timeOutFlag) { // 超时重传
			printCliWindow();
			printf("[警告]超时重传中\n");
			for (int i = base; i < cliWinNext; i++) {
				sendList[i].setFlags(FLAG_RESEND);
				UDPsend(sendList[i], client, serIp_C);
			}
			timeOutFlag = false;
			start = clock();
			resend++;
		}
		else {
			int j = base;
			if (base + WIN_SIZE == cliWinNext) { // 发送窗口达上限
				Sleep(15 * TIMEOUT); // 等待窗口恢复
			}
			for (; cliWinNext < base + WIN_SIZE && cliWinNext < Eof; cliWinNext++) { //Eof最后的值是实际有效序号+1
				if (!timeOutFlag) {
					UDPsend(sendList[cliWinNext], client, serIp_C);
					start = clock();// 重置时间
				}
				else break; //超时

				if (j == base) { // base没变 说明始终未收到ACK
					end = clock();
					if ((double)(end - start) / CLOCKS_PER_SEC >= TIMEOUT) {
						timeOutFlag = true;
						cliWinNext++; 
						break;
					}
				}
			}
		}
		end = clock();
		if ((double)(end - start) / CLOCKS_PER_SEC >= TIMEOUT) timeOutFlag = true;
		if (resend > MAX_RESEND) {
			printf("[警告]重发次数过多\n");
			return 0;
		}
	}
	return 1;
}

int clientConnect() {
	UDP_Reli u;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setSeq(Eof);
	u.setFlags(FLAG_SYN); // Syn请求置位
	sendList[Eof++] = u;
	return 1;
}
int clientFin() {
	UDP_Reli u;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setSeq(Eof);
	u.setFlags(FLAG_FIN); // Syn请求置位
	sendList[Eof++] = u; //加入队列
	return 1;
}

//读取目标文件
int getFile(char* path, uint16_t& len, uint16_t& count) {
	count = len = 0;
	ifstream ifs;
	ifs.open(path, ios::in | ios::binary); // 读+二进制
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
	printf("[提示]文件读取完毕\n");
	return 1;
}

//ifCont代表是否重用
int file_To_List(char* path, int& ifCont) { 
	int i;
	//清空缓存区备用
	for (i = 0; i < MSG_COUNT; i++) {
		memset(sendbuff[i], 0, MSG_SIZE);
	}
	uint16_t len, count;
	len = count = 0;
	if (getFile(path, len, count) == 0) return 0;
	UDP_Reli u;
	u.setSeq(Eof);
	u.setCounts(count);
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_STR); //开始标志位
	if (ifCont == 1) //复用标志
	{
		u.setFlags(FLAG_CONTINUE);
	}
	u.setLen(len); //长度仅16位 不足30M 所以第一个包的长度段我们告知最后一个包的长度 这样通过count即可以算出总长度
	memcpy(u.dataBuf, path, strlen(path));//第一个包不传输数据，只是告知要传输文件了
	sendList[Eof++] = u;

	for (i = 0; i <= count; i++) {
		UDP_Reli tmp;
		tmp.setSrc(cliIp_C);
		tmp.setDst(serIp_C);
		tmp.setCounts(count);
		tmp.setSeq(Eof);
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
		sendList[Eof++] = tmp;
	}
	return 1;
}

int main() {
	for (int i = 0; i < MSG_COUNT; i++) {
		memset(sendbuff[i], 0, MSG_SIZE);
	}
	memset(sendList, 0, MSG_COUNT);

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

	timeout_C.tv_sec = 2; //s
	timeout_C.tv_usec = 0;  //ms
	//套接字描述符 + 被设置的选项的级别必须为SOL_SOCKET + 指定准备设置的选项名
	if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_C, sizeof(timeout_C)) == -1) {
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
		int ifContinue = 0; 
		memset(path, 0, sizeof(path));
		memset(sendList, 0, sizeof(sendList)); // 重置发送队列内容
		printf("发送完毕后是否继续发送Y/N\n");
		cin >> flag;
		if (flag == 'Y' || flag == 'y') {
			ifContinue = 1;
		}
		printf("[提示]请输入文件路径\n");
		cin >> path;
		if (status_C == STATUS_OFF) {
			clientConnect();
			status_C = STATUS_ON;
		}
		if (file_To_List(path, ifContinue) == 0) {
			printf("[警告]即将自动退出\n");
			break;
		}
		if (flag != 'Y' && flag != 'y') {
			printf("[提示]将在传输结束后断开连接\n");
			clientFin();
		}
		time_t start = clock();
		time_t end;
		if (base < Eof) {
			recvTD_C = CreateThread(NULL, NULL, sendHD, LPVOID(), 0, NULL); // client的线程不需要额外参数 空着
			sendTD_C = CreateThread(NULL, NULL, recvHD, LPVOID(), 0, NULL);
			// 句柄 + 超时间隔(INFINITE：则仅当发出对象信号时，该函数才会返回)
			if (DWORD dwRet1 = WaitForSingleObject(sendTD_C, 40000) == WAIT_TIMEOUT) {
				printf("[警告]传送失败\n");
				break;
			}
			DWORD dwRet2 = WaitForSingleObject(recvTD_C, INFINITE);
			if (base == Eof) {
				printf("[提示]发送完成\n");
			}
		}
		end = clock();
		double fileSize = (double)((Eof - 2) * sizeof(UDP_Reli)) / 1e6;
		double time = (double)(end - start) / CLOCKS_PER_SEC;
		double rate = fileSize * 8.0 / time;
		printf("[日志]传输完毕，耗时:%lfS\n文件大小:%lfMB\n传输速率:%lfMbps\n", time, fileSize, rate);

		base = Eof = cliWinNext = 0; // 每次重置发送队列的各参数
		if (flag != 'Y' && flag != 'y') {
			printf("[提示]程序即将退出\n");
			break;
		}
	}
	closesocket(client);
	WSACleanup();
	return 0;
}