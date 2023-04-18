#include"UDP_Reli.h"
using namespace std;
#define LOOPIP "127.0.0.1" //�����ػ���ַ
#define SERVERPORT 7655
#define CLIENTPORT 7654
SOCKET server;
timeval timeout; //������socket��ʱ
SOCKADDR_IN serIp_S;
SOCKADDR_IN cliIp_S;
uint32_t serSeq = 0;
uint8_t status_S = 0;
char recvbuff[4096][MSG_SIZE]; //��������ļ���С������4KB �ļ��ܴ�С������16MB
//��������
int serverConnect(UDP_Reli& u) {
	UDP_Reli v;
	v.setSrc(serIp_S);
	v.setDst(u.getSrc());
	if (u.getFlags() & FLAG_EXIST) {
		printf("[��־]�յ����Կͻ��˵�SYN����\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		v.setAckSeq(u);
		v.setSeq(serSeq++);
		if (UDPsend(v, server, cliIp_S) == 0) return 0;
		printf("[��ʾ]�������ӳɹ�\n");
		return 1;
	}
	printf("[����]��������ʧ��\n");
	return 0;
}
//�Ͽ�����
int serverFin(UDP_Reli u) {
	UDP_Reli v;
	v.setSrc(serIp_S);
	v.setDst(u.getSrc());
	if (u.getFlags() & FLAG_EXIST) {
		printf("[��־]�յ����Կͻ��˵�FIN����\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
		v.setAckSeq(u);
		v.setSeq(serSeq++);
		if (UDPsend(v, server, cliIp_S) == 0) return 0;
		printf("[��ʾ]ȷ�϶����ɹ�\n");
		return 1;
	}
	else return 0;
}

int recvFile(UDP_Reli& msg) { //msg��STATE_STR��
	int i = 0;
	for (i = 0; i < 4096; i++) { //���ȫ�ֻ�������ֹ�ϴε����ݸ���
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
	printf("[��־]����STATE_STR����\nseq=%u, ack=%u, checksum=%u\n", msg.getSeq(), msg.getAck(), msg.getCheckSum());
	printf("[��ʾ]�����ļ���Ϊ:");
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
		if (stopAndWaitRecv(u, v, server, cliIp_S, cliIp_S, serSeq) == 1) { //ͣ��
			if (i == count) {
				for (int j = 0; j < len; j++) recvbuff[i][j] = u.dataBuf[j];
			}
			else {
				for (int j = 0; j < MSG_SIZE; j++) recvbuff[i][j] = u.dataBuf[j];
			}
			if ((i + 1) % 50 == 0) {
				printf("[��־]�յ���%d���ļ���\nseq=%u, ack=%u, checksum=%u\n", i + 1, u.getSeq(), u.getAck(), u.getCheckSum()); //ÿ50�����Ĵ�ӡһ����־
			}
		}
		else {
			printf("[����]��������г���\n");
			return 0;
		}

		if (i == count) {
			if (!(u.getFlags() & FLAG_END) || !(u.getFlags() & FLAG_EXIST)) { //���һ����Ӧ����FLAG_END
				printf("[����]���յ����ļ�����\n");
				return 0;
			}
			else {
				printf("[��־]����STATE_END����\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
			}
		}
	}

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
	printf("[��ʾ]�ļ�����ɹ�\n");
	status_S = status_S & (uint8_t)~(STATUS_SENDING); //����sending״̬
	return 1;
}

int main(int argc, char* argv[]) {
	//��ʼ��ȫ�ֽ��ջ�����
	for (int i = 0; i < 4096; i++) {
		memset(recvbuff[i], 0, MSG_SIZE);
	}

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

	timeout.tv_sec = 2; //s
	timeout.tv_usec = 0;  //ms
	//�׽��������� + �����õ�ѡ��ļ������ΪSOL_SOCKET + ָ��׼�����õ�ѡ����
	if (setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == -1) {
		cout << "[����]����sockt����ʧ��" << endl;
		return 0;
	}

	inet_pton(AF_INET, LOOPIP, &serIp_S.sin_addr.S_un.S_addr);
	serIp_S.sin_family = AF_INET;
	serIp_S.sin_port = htons(SERVERPORT);
	inet_pton(AF_INET, LOOPIP, &cliIp_S.sin_addr.S_un.S_addr);
	cliIp_S.sin_family = AF_INET;
	cliIp_S.sin_port = htons(SERVERPORT);


	if (bind(server, (sockaddr*)&serIp_S, sizeof(sockaddr)) == -1) {
		cout << "[����]bindʧ��" << endl;
		return 0;
	}
	cout << "[��ʾ]bind�ɹ�" << endl;
	cout << "[��ʾ]server�Ѿ���" << endl;

	char flag;
	while (1) {
		UDP_Reli u;
		UDPrecv(u, server, cliIp_S);
		if (u.getFlags() & FLAG_EXIST && u.getSrc().sin_addr.S_un.S_addr != 0) { //��ֹ�ձ��ĸ��� ��Ƶ����ӡ��Ϣ������������Ϣ����
			if (u.getFlags() & FLAG_SYN) {
				if (serverConnect(u) == 1) status_S = STATUS_ON;
			}
			else if ((u.getFlags() & FLAG_FIN)) {
				cout << "[��ʾ]�յ��Ͽ���������" << endl;
				if (serverFin(u)) {
					status_S = STATUS_OFF;
					serSeq = 0;
					cout << "[����]�Ƿ��������(y/n)" << endl;
					cin >> flag;
					if (!(flag == 'y' || flag == 'Y')) break;
				}
				else cout << "[����]δ����Ŀ������" << endl;
			}
			else if (u.getFlags() & FLAG_STR) {
				if (status_S == STATUS_ON) {
					status_S = status_S | STATUS_SENDING;
					cout << "[��ʾ]��ʼ�����ļ�" << endl;
					recvFile(u);
				}
				else cout << "[����]��δ����" << endl;
			}
		}
	}


	closesocket(server);//�ر�socket
	WSACleanup();
	return 0;
}