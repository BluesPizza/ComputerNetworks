#include"UDP_Reli.h"
using namespace std;
SOCKET client;
timeval timeout_C; //������socket��ʱ
SOCKADDR_IN serIp_C;
SOCKADDR_IN cliIp_C;
char sendbuff[MSG_COUNT][MSG_SIZE]; //���ط��ͻ����� ��������ļ���С������4KB �ļ��ܴ�С������16MB
UDP_Reli sendList[MSG_COUNT];  // ���Ͷ���
uint16_t cliWinNext = 0;  // ��һ���������ͳ�����Ϣ�ı�ţ�����β����
uint16_t base = 0;		  // ����ͷ��
uint16_t Eof = 0;		  // ���Ͷ��н�β���ļ�β+1��
HANDLE recvTD_C;		  // �����߳�
HANDLE sendTD_C;		  // �����߳�
uint8_t status_C = 0;
// ��ӡ��־ �������
void printCliWindow() {
	printf("[��־]base:%u cliWinNext:%u WindowSize:%d\n", base, cliWinNext, WIN_SIZE);
}

//�����߳�
DWORD WINAPI recvHD(LPVOID lparam) {
	printCliWindow();
	while (base < Eof) {
		if (cliWinNext > base + WIN_SIZE || base >= cliWinNext) { // ���������
			Sleep(3 * TIMEOUT); // �ȴ�
		}
		UDP_Reli u;
		UDPrecv(u, client, serIp_C);
		if (u.check() && u.getFlags() & FLAG_EXIST && u.getFlags() & FLAG_ACK && u.getSrc().sin_addr.S_un.S_addr != 0) { // ���checkSum ��Чλ ACKλ
			if (u.getAck() >= base) { //  ����յ�u.getAck() > base ������ACK�ն��� ֱ��Ĭ��֮��İ����Ѿ��յ�
				printf("[����]ACK=%u", u.getAck());
				printCliWindow();
				base = u.getAck() + 1; //�ۻ�ȷ��
			}
			else {
				printCliWindow();
				printf("[��־]����ACKΪ%u\n", u.getAck());
				printf("[����]δ�յ�Ԥ��ACK����\n");
			}
		}
	}
	return 1;
}

//�����߳�
DWORD WINAPI sendHD(LPVOID lparam) {
	int resend = 0; //�ش�����
	bool timeOutFlag = false; //��ʱ��־
	time_t start = clock();
	time_t end;
	while (base < Eof) {
		if (timeOutFlag) { // ��ʱ�ش�
			printCliWindow();
			printf("[����]��ʱ�ش���\n");
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
			if (base + WIN_SIZE == cliWinNext) { // ���ʹ��ڴ�����
				Sleep(15 * TIMEOUT); // �ȴ����ڻָ�
			}
			for (; cliWinNext < base + WIN_SIZE && cliWinNext < Eof; cliWinNext++) { //Eof����ֵ��ʵ����Ч���+1
				if (!timeOutFlag) {
					UDPsend(sendList[cliWinNext], client, serIp_C);
					start = clock();// ����ʱ��
				}
				else break; //��ʱ

				if (j == base) { // baseû�� ˵��ʼ��δ�յ�ACK
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
			printf("[����]�ط���������\n");
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
	u.setFlags(FLAG_SYN); // Syn������λ
	sendList[Eof++] = u;
	return 1;
}
int clientFin() {
	UDP_Reli u;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setSeq(Eof);
	u.setFlags(FLAG_FIN); // Syn������λ
	sendList[Eof++] = u; //�������
	return 1;
}

//��ȡĿ���ļ�
int getFile(char* path, uint16_t& len, uint16_t& count) {
	count = len = 0;
	ifstream ifs;
	ifs.open(path, ios::in | ios::binary); // ��+������
	if (!ifs.is_open()) {
		printf("[����]�ļ���ȡʧ��\n");
		return 0;
	}
	char tmp = ifs.get();
	while (!ifs.eof()) { //�����ļ�ĩβ
		sendbuff[count][len % MSG_SIZE] = tmp;
		len++;
		if (len % MSG_SIZE == 0) {
			len = 0;
			count++;
		}
		tmp = ifs.get();
	}
	ifs.close();
	printf("[��ʾ]�ļ���ȡ���\n");
	return 1;
}

//ifCont�����Ƿ�����
int file_To_List(char* path, int& ifCont) { 
	int i;
	//��ջ���������
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
	u.setFlags(FLAG_STR); //��ʼ��־λ
	if (ifCont == 1) //���ñ�־
	{
		u.setFlags(FLAG_CONTINUE);
	}
	u.setLen(len); //���Ƚ�16λ ����30M ���Ե�һ�����ĳ��ȶ����Ǹ�֪���һ�����ĳ��� ����ͨ��count����������ܳ���
	memcpy(u.dataBuf, path, strlen(path));//��һ�������������ݣ�ֻ�Ǹ�֪Ҫ�����ļ���
	sendList[Eof++] = u;

	for (i = 0; i <= count; i++) {
		UDP_Reli tmp;
		tmp.setSrc(cliIp_C);
		tmp.setDst(serIp_C);
		tmp.setCounts(count);
		tmp.setSeq(Eof);
		if (i != count) { //�����һ������ totalLen����MSG_SIZE
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
		cout << "[����]��ʼ��socket��ʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]WSAStartup�ɹ�\n";

	if ((client = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		cout << "[����]��ʼ��socketʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]Socket�����ɹ�\n";

	timeout_C.tv_sec = 2; //s
	timeout_C.tv_usec = 0;  //ms
	//�׽��������� + �����õ�ѡ��ļ������ΪSOL_SOCKET + ָ��׼�����õ�ѡ����
	if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_C, sizeof(timeout_C)) == -1) {
		cout << "[����]����sockt����ʧ��" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &cliIp_C.sin_addr.S_un.S_addr);
	cliIp_C.sin_family = AF_INET;
	cliIp_C.sin_port = htons(CLIENTPORT);
	inet_pton(AF_INET, LOOPIP, &serIp_C.sin_addr.S_un.S_addr);
	serIp_C.sin_family = AF_INET;
	serIp_C.sin_port = htons(SERVERPORT);

	cout << "[��ʾ]�ͻ���׼������" << endl;
	char flag;
	char path[50];
	while (1) {
		int ifContinue = 0; 
		memset(path, 0, sizeof(path));
		memset(sendList, 0, sizeof(sendList)); // ���÷��Ͷ�������
		printf("������Ϻ��Ƿ��������Y/N\n");
		cin >> flag;
		if (flag == 'Y' || flag == 'y') {
			ifContinue = 1;
		}
		printf("[��ʾ]�������ļ�·��\n");
		cin >> path;
		if (status_C == STATUS_OFF) {
			clientConnect();
			status_C = STATUS_ON;
		}
		if (file_To_List(path, ifContinue) == 0) {
			printf("[����]�����Զ��˳�\n");
			break;
		}
		if (flag != 'Y' && flag != 'y') {
			printf("[��ʾ]���ڴ��������Ͽ�����\n");
			clientFin();
		}
		time_t start = clock();
		time_t end;
		if (base < Eof) {
			recvTD_C = CreateThread(NULL, NULL, sendHD, LPVOID(), 0, NULL); // client���̲߳���Ҫ������� ����
			sendTD_C = CreateThread(NULL, NULL, recvHD, LPVOID(), 0, NULL);
			// ��� + ��ʱ���(INFINITE����������������ź�ʱ���ú����Ż᷵��)
			if (DWORD dwRet1 = WaitForSingleObject(sendTD_C, 40000) == WAIT_TIMEOUT) {
				printf("[����]����ʧ��\n");
				break;
			}
			DWORD dwRet2 = WaitForSingleObject(recvTD_C, INFINITE);
			if (base == Eof) {
				printf("[��ʾ]�������\n");
			}
		}
		end = clock();
		double fileSize = (double)((Eof - 2) * sizeof(UDP_Reli)) / 1e6;
		double time = (double)(end - start) / CLOCKS_PER_SEC;
		double rate = fileSize * 8.0 / time;
		printf("[��־]������ϣ���ʱ:%lfS\n�ļ���С:%lfMB\n��������:%lfMbps\n", time, fileSize, rate);

		base = Eof = cliWinNext = 0; // ÿ�����÷��Ͷ��еĸ�����
		if (flag != 'Y' && flag != 'y') {
			printf("[��ʾ]���򼴽��˳�\n");
			break;
		}
	}
	closesocket(client);
	WSACleanup();
	return 0;
}