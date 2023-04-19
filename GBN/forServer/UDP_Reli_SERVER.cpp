#include"UDP_Reli.h"
using namespace std;
#define TIMEOUT_SERVER 50
#define ACC_COUNT 1 // �ۼ�ȷ�ϼ��� ÿ4����Ϣ�ش�һ��ACK
SOCKET server;
timeval timeout_S; //������socket��ʱ
SOCKADDR_IN serIp_S;
SOCKADDR_IN cliIp_S;
char recvbuff[MSG_COUNT][MSG_SIZE]; //4K*4K ����������ĳ��ȴ�С������4KB �����ļ��ܴ�С������16MB
UDP_Reli recvList[MSG_COUNT];  // ���Ͷ��� �����������ջ������ɽ��ܵ����������4k
uint16_t serWinNext = 0; // ��һ��Ҫ�������Ϣλ��
uint16_t fileCount = 0; //������recvList�ĺ�λ��
uint8_t status_S = 0;
HANDLE recvHD_R; // �����߳�

void printSerWindow() {
	printf("[��־]serWinNext:%u WindowSize:%d\n",serWinNext, ACC_COUNT); 
}

// �����߳� �ص�����ԭ��д���ǹ̶���
// ���յ��ı��ĵ����ݲ���д�뱾�ػ�����
DWORD WINAPI recvFile(LPVOID lparam) { 
	UDP_Reli_Thread* file = (UDP_Reli_Thread*)(LPVOID)lparam;
	memset(recvbuff[file->count], 0, MSG_SIZE);
	for (int i = 0; i < file->totalLen; i++) {
		recvbuff[file->count][i] = file->msg.dataBuf[i];
	}
	return 1;
}

// ����д�뱾���ļ�
int writeFile(uint16_t& len, uint16_t& count) {
	char path[50];
	memset(path, 0, sizeof(path));
	printf("[��ʾ]�����뱣������·�� \n");
	cin >> path;
	ofstream ofs;
	ofs.open(path, ios::binary);
	if (!ofs.is_open()) {
		printf("[����]�ļ�·���Ƿ�\n");
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
	printf("[��ʾ]�ļ�����ɹ�\n");
	return 1;
}


int main() {
	//��ʼ��ȫ�ֽ��ջ�����
	for (int i = 0; i < MSG_COUNT; i++) {
		memset(recvbuff[i], 0, MSG_SIZE);
	}
	memset(recvList, 0, MSG_COUNT);
	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	if (WSAStartup(ver, &data) != 0) {
		cout << "[����]��ʼ��socket��ʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]WSAStartup�ɹ�\n";

	if ((server = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		cout << "[����]��ʼ��socketʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]Socket�����ɹ�\n";

	timeout_S.tv_sec = 2; //s
	timeout_S.tv_usec = 0;  //ms
	//�׽��������� + �����õ�ѡ��ļ������ΪSOL_SOCKET + ָ��׼�����õ�ѡ����
	if (setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_S, sizeof(timeout_S)) == -1) {
		cout << "[����]����sockt����ʧ��" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &serIp_S.sin_addr.S_un.S_addr);
	serIp_S.sin_family = AF_INET;
	serIp_S.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, LOOPIP, &cliIp_S.sin_addr.S_un.S_addr);
	cliIp_S.sin_family = AF_INET;
	cliIp_S.sin_port = htons(CLIENTPORT);

	if (bind(server, (sockaddr*)&serIp_S, sizeof(sockaddr)) == -1) {
		cout << "[����]bindʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]bind�ɹ�" << endl;
	cout << "[��ʾ]server�Ѿ���" << endl;

	bool timeoutTest = true; //������������ʱ��־λ �����FLAG_STR�����Լ����������ش�
	time_t start = clock();//��ʱ�� һ��ʱ��δ�յ������ж�����
	time_t end;
	while (1) {
		char flag; //�Ƿ��˳�
		bool ifTimeOut = false; //ȷ���Ƿ�ʱ�Ͽ�
		bool ifCont = false; // ȷ���Ƿ�����
		int countAck = ACC_COUNT - 1; //�ۼ��յ����ļ���
		uint16_t count=0;  //�����߳� ��֪ĿǰҪд�ı�������Ч�ļ��еĵڼ�����
		uint16_t len=0;    //�����߳� ��֪ĿǰҪд�ı��ĵ����ݶγ���
		int i = 0; //������������ʱ����
		printf("[��ʾ]�������Ѹ�λ");
		while (1) {
			int countReAck = 3; //�ۼ�δ��׽����Ч���ļ��� ÿ3��У����� �ش�һ��ACK
			UDP_Reli tmpRecv;
			UDPrecv(tmpRecv, server, cliIp_S);
			if (tmpRecv.getFlags() & FLAG_EXIST && tmpRecv.getSrc().sin_addr.S_un.S_addr != 0) {
				//��鱨���Ƿ���Ч + ������Դ��ȷ
				if (tmpRecv.check() && tmpRecv.getSeq() == serWinNext) {
					//���У��� + �õ��İ����к��Ƿ������������к� ��Ч�İ����ܷŽ����Ļ������
					recvList[serWinNext] = UDP_Reli(tmpRecv);
					UDP_Reli u;
					u.setFlags(FLAG_ACK);
					u.setSrc(serIp_S);
					u.setDst(cliIp_S);
					u.setAck(serWinNext);
					u.setSeq(serWinNext);
					if (status_S & STATUS_SENDING && !(tmpRecv.getFlags() & FLAG_STR)) { //��SENDING״̬�£���ѡ��ÿACC_COUNT�����Ļش�һ��ACK ���𳬹����ڴ�С
						countAck++;
						if (countAck % ACC_COUNT == 0 || tmpRecv.getFlags() & FLAG_END) {
							UDPsend(u, server, cliIp_S);
							printf("[����]cliSeq=%u", recvList[serWinNext].getSeq());
							printSerWindow();
							countAck = 0;
						}
					}
					else {
						UDPsend(u, server, cliIp_S);
						printf("[����]cliSeq=%u", recvList[serWinNext].getSeq());
						printSerWindow();
					}
					//printf("[����]cliSeq=%u", recvList[serWinNext].getSeq());
					//printSerWindow();
					start = clock(); //ˢ�¼�ʱ��
					if (status_S == STATUS_OFF && (recvList[serWinNext].getFlags() & FLAG_SYN)) { //FLAG_SYN
						// δ����״̬ + �յ���������
						status_S = STATUS_ON;  
						printf("[��ʾ]�������ӳɹ�\n");
					}
					else if (status_S & STATUS_ON && (recvList[serWinNext].getFlags() & FLAG_FIN)) { //FLAG_FIN
						status_S = STATUS_OFF;
						printf("[��ʾ]�Ͽ����ӳɹ�\n");
						break;
					}
					else if (status_S & STATUS_ON && !(status_S & STATUS_SENDING) && (recvList[serWinNext].getFlags() & FLAG_STR)) { //FLAG_STR
						status_S |= STATUS_SENDING;
						if (recvList[serWinNext].getFlags() & FLAG_CONTINUE) { //����һ��FLAG_STR�������Ƿ����CONTINUE��� ���ϵ���Ƿ���Ҫ�ظ�FIN
							ifCont = true;
						}
						if (timeoutTest) { //Ϊ��չʾ�ش� ���ڵ�һ��FLAG_STR���Ĳ����лظ�
							Sleep(2500);
							timeoutTest = false;
						}
						printf("[��ʾ]��ʼ�����ļ�\n");
						printf("[��ʾ]�����ļ���Ϊ:");
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
						if (fileCount != count) { //����������һ����countNow���Ӧ��Ϊcount+1
							printf("[����]���յ����ļ�����\n");
							return 0;
						}
						UDP_Reli_Thread* ut = new UDP_Reli_Thread;
						ut->count = count;  
						ut->totalLen = len;
						ut->msg = recvList[serWinNext];
						/*
						* lpThreadAttributes	�ں˶���İ�ȫ���ԣ�һ�㴫��NULL    
						* dwStackSize,			�߳�ջ�ռ��С,����0��ʾʹ��Ĭ�ϴ�С
						* lpStartAddress,		���߳���ִ�е��̺߳�����ַ
						* lpParameter,			�����̺߳����Ĳ���
						* dwCreationFlags,		ָ������ı�־�������̵߳Ĵ���,0��ʾ�̴߳���֮�������Ϳ��Խ��е���
						* lpThreadId			�����̵߳�ID�ţ�����NULL��ʾ����Ҫ���ظ��߳�ID��
						*/
						recvHD_R = CreateThread(NULL, NULL, recvFile, LPVOID(ut), 0, NULL);
						fileCount++;
						fileCount = 0;
						printf("[��ʾ]�ļ��������\n");
						if (ifCont) break;
					}
					else if (status_S & STATUS_ON && status_S & STATUS_SENDING) { //�ļ��ְ�
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
					printf("[��ʾ]�����ش�ACK\n");
					printf("[����]CliSeq=%u\n", tmpRecv.getSeq());
					countReAck++;
					if (countReAck % 4 == 0) { //���ظջظ�����ACK��
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
				if (status_S & STATUS_ON && ((1.0 * (end - start)) / CLOCKS_PER_SEC) >= TIMEOUT_SERVER && serWinNext != 0) { // ��ʱ50S�Զ��Ͽ�
					status_S = STATUS_OFF;
					printf("[����]��ʱ�Ͽ�\n");
					ifTimeOut = true;
					break;
				}
			}
		}
		if (!ifTimeOut) { 
			DWORD dwRet1 = WaitForSingleObject(recvHD_R, INFINITE);
			writeFile(len, count);
			printf("[��ʾ]����������Ƿ����Y/N\n");
		}
		else //��ʱ�Ͽ�
		{
			printf("[��ʾ]�Ƿ����Y/N\n");
		}
		cin >> flag;
		if (flag != 'y' && flag != 'Y') break;
		//��ʼ��ȫ�ֽ��ջ�����
		for (int i = 0; i < MSG_COUNT; i++) {
			memset(recvbuff[i], 0, MSG_SIZE);
		}
		memset(recvList, 0, MSG_COUNT);
		serWinNext = 0; //����������
	}
	closesocket(server);//�ر�socket
	WSACleanup();
	return 0;
}