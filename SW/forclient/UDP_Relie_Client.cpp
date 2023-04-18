#include"UDP_Reli.h"
using namespace std;
#define LOOPIP "127.0.0.1" //�����ػ���ַ
#define CLIENTPORT 7654
#define SERVERPORT 7655
SOCKET client;
timeval timeout; //������socket��ʱ
SOCKADDR_IN cliIp_C;
SOCKADDR_IN serIp_C;
uint32_t cliSeq = 0;
uint8_t status_C = 0;
char sendbuff[4096][MSG_SIZE]; //��������ļ���С������4KB �ļ��ܴ�С������16MB
//��������
int clientConnect() {
	UDP_Reli u, v;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_SYN); // Syn������λ 
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq) == 1){
		printf("[��־]SYN����ɹ�\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		printf("[��ʾ]�������ӳɹ�\n");
		return 1;
	}
	return 0;
}
//�Ͽ�����
int clientFin() {
	UDP_Reli u, v;
	u.setSrc(cliIp_C);
	u.setDst(serIp_C);
	u.setFlags(FLAG_FIN);
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq)) {
		printf("[��־]FIN����ɹ�\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		printf("[��ʾ]�Ͽ����ӳɹ�\n");
		return 1;
	}
	printf("[����]�Ͽ�����ʧ��\n");
	return 0;
}

//��ȡĿ���ļ�
int getFile(char* path, uint16_t& len, uint16_t& count) {
	count = len = 0;
	ifstream ifs;
	ifs.open(path,ios::in | ios::binary); // ��+������
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
	return 1;
}

int sendFile(char* path) {
	int i;
	//��ջ���������
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
	u.setFlags(FLAG_STR); //��ʼ��־λ
	u.setLen(len); //���Ƚ�16λ ����30M ���Ե�һ�����ĳ��ȶ����Ǹ�֪���һ�����ĳ��� ����ͨ��count����������ܳ���
	memcpy(u.dataBuf, path, strlen(path));//��һ�������������ݣ�ֻ�Ǹ�֪Ҫ�����ļ���
	time_t start = clock();
	if (stopAndWaitSend(u, v, client, serIp_C, serIp_C, cliSeq) == 0) {
		printf("[����]�����ļ�����ʧ��\n");
		return 0;
	}
	printf("[��־]STATE_STR�����ѱ�����\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
	for (i = 0; i <= count; i++) {
		UDP_Reli tmp;
		tmp.setSrc(cliIp_C);
		tmp.setDst(serIp_C);
		tmp.setCounts(count);
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
		if(stopAndWaitSend(tmp, v, client, serIp_C, serIp_C, cliSeq) == 0) {
			printf("[����]�ļ������ж�\n");
			return 0;
		}
		if (tmp.getFlags() & FLAG_END) {
			printf("[��־]STATE_END�����ѱ�����\nseq=%u, ack=%u, checksum=%u\n", tmp.getSeq(), tmp.getAck(), tmp.getCheckSum());
		}
	}
	time_t end = clock();
	double fileSize = (double)(count * (sizeof(UDP_Reli) - 28) + len) / 1e6;
	double time = (double)(end - start) / CLOCKS_PER_SEC;
	double rate = fileSize * 8.0 / time;
	printf("[��־]������ϣ���ʱ:%lfS\n�ļ���С:%lfMB\n��������:%lfMbps\n", time, fileSize, rate);
	return 1;
}

int main(int argc, char* argv[]) {
	//��ʼ��ȫ�ַ��ͻ�����
	for (int i = 0; i < 4096; i++) {
		memset(sendbuff[i], 0, MSG_SIZE);
	}

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

	timeout.tv_sec = 2; //s
	timeout.tv_usec = 0;  //ms
	//�׽��������� + �����õ�ѡ��ļ������ΪSOL_SOCKET + ָ��׼�����õ�ѡ����
	if (setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) { 
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
		memset(path, 0, sizeof(path)); //ÿ�����õ��ϴε�path ���������Ϊ���볤�Ȳ�ͬ����·������
		cout << "[��ʾ]�������ļ�·��" << endl;
		cin >> path;
		if (status_C == STATUS_OFF) {
			cout << "[��ʾ]��ʼ��������" << endl;
			if (clientConnect() == 1) {
				status_C = STATUS_ON;
				sendFile(path);
			}
			else {
				cout << "[����]��������ʧ��" << endl;
			}
		}
		else {
			sendFile(path);
		}
		cout << "[����]�Ƿ��������(y/n)" << endl;
		cin >> flag;
		if (!(flag == 'y' || flag == 'Y')) {
			cliSeq = 0;
			status_C = STATUS_OFF;
			if (clientFin() == 1) break;
			else { // ����ڷ����������ļ��ɹ�֮ǰ������������ ����������ظ�ֱ��������� �������ʼ�յȴ� ǿ���˳�����
				cout << "[����]�������������ڱ����ļ� ����ǿ���˳�" << endl;   
				break;
			}
		}
	}

	closesocket(client);
	WSACleanup();
	return 0;
}