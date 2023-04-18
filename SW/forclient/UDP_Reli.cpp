#include"UDP_Reli.h"
using namespace std;
// class UDP_Reli#########################################
void UDP_Reli::setCheckSum() {
	uint32_t checksum = 0;
	uint8_t* sec = (uint8_t*)this;
	for (int i = 0; i < 14; i++){
		checksum += ((sec[2 * i] << 8) + (sec[2 * i + 1]));
	}
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	this->checkSum = (uint16_t)~checksum;
}

bool UDP_Reli::check(){
	uint32_t checksum = 0;
	uint8_t* sec = (uint8_t*)this;
	for (int i = 0; i < 14; i++){
		if (i != 13)
		checksum += ((sec[2 * i] << 8) + (sec[2 * i + 1]));
	}
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);
	//checksum�������� ��ԭ����ͬ����
	if ((uint16_t)~checksum == this->checkSum)
		return true;
	return false;
}

void UDP_Reli::setAckSeq(UDP_Reli u) {
	this->flags = this->flags | FLAG_ACK;
	this->Ack = u.Seq;
}

void UDP_Reli::setSyn() {
	this->flags = this->flags | FLAG_SYN;
}

void UDP_Reli::setSeq(uint32_t seq) {
	this->Seq = seq;
}

void UDP_Reli::setAck(uint32_t ack) {
	this->Ack = ack;
}

void UDP_Reli::setFlags(uint16_t flag) {
	if (flag == FLAG_NULL)this->flags &= FLAG_NULL;
	else this->flags = this->flags | flag;
}

void UDP_Reli::setSrc(SOCKADDR_IN src) {
	this->srcIp = src.sin_addr.S_un.S_addr;
	this->srcPort = src.sin_port;
}

void UDP_Reli::setDst(SOCKADDR_IN dst) {
	this->dstIp = dst.sin_addr.S_un.S_addr;
	this->dstPort = dst.sin_port;
}

void UDP_Reli::setCounts(uint16_t count) {
	this->counts = count;
}

void UDP_Reli::setLen(uint16_t len) {
	this->totalLen = len;
}

uint16_t UDP_Reli::getLen() {
	return this->totalLen;
}

uint32_t UDP_Reli::getAck() {
	return this->Ack;
}

uint32_t UDP_Reli::getSeq() {
	return this->Seq;
}

uint16_t UDP_Reli::getFlags() {
	return this->flags;
}

uint16_t UDP_Reli::getCounts() {
	return this->counts;
}

uint16_t UDP_Reli::getCheckSum() {
	return this->checkSum;
}

SOCKADDR_IN UDP_Reli::getSrc() {
	SOCKADDR_IN tmp;
	tmp.sin_family = AF_INET;
	tmp.sin_port = this->srcPort;
	tmp.sin_addr.S_un.S_addr = this->srcIp;
	return tmp;
}
SOCKADDR_IN UDP_Reli::getDst() {
	SOCKADDR_IN tmp;
	tmp.sin_family = AF_INET;
	tmp.sin_port = this->dstPort;
	tmp.sin_addr.S_un.S_addr = this->dstIp;
	return tmp;
}

void UDP_Reli::clearUDP() {
	memset(this, 0, sizeof(this));
}

//others ######################################
//UDP������װ �Զ�������Чλ У���
int UDPsend(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_to) {
	u.setFlags(FLAG_EXIST); //��Чλ
	u.setCheckSum();
	if (sendto(soc, (char*)&u, sizeof(u), 0, (SOCKADDR*)&addr_to, sizeof(addr_to)) == SOCKET_ERROR) {
		cout << "[����]���ݷ���ʧ��" << endl;
		return 0;
	}
	return 1;
}

void UDPrecv(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_from) {
	memset(u.dataBuf, 0, sizeof(u.dataBuf));
	int len = sizeof(addr_from);
	recvfrom(soc, (char*)&u, sizeof(u), 0, (SOCKADDR*)&addr_from, &len);
}

int stopAndWaitSend(UDP_Reli& u, UDP_Reli v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq) {
	int count = 0;
	u.setSeq((seq++)); //�������
	UDPsend(u, soc, addr_to);
	time_t start = clock(); //��ʱ�� 
	time_t end;
	while (true) {
		UDPrecv(v, soc, addr_from);
		if ((v.getFlags() & FLAG_ACK) && v.getAck() == u.getSeq()) { //���ACK�Ƿ�Ϸ� �Ƿ�ACK��������� ��ʱ����
			printf("[��־]�յ���Ч�ش�ACK\nseq:%u, ack=%u, checksum=%u\n", v.getSeq(), v.getAck(), v.getCheckSum());
			return 1;//ȷ�ϻظ�
		}
		if (count == MAX_RESEND) {
			printf("[����]����ط�ʧ��\n");
			return 0; //�ط������ʧ��
		}
		end = clock();
		if (((end - start) / CLOCKS_PER_SEC) >= TIMEOUT) { //��ʱ�ط�
			start = clock();
			UDPsend(u, soc, addr_to);
			count++;
			printf("[��־]��%d���ط���seq=%u, ack=%u, checksum=%u\n", count, u.getSeq(), u.getAck(), u.getCheckSum());
		}
	}
}

int stopAndWaitRecv(UDP_Reli& u, UDP_Reli v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq) {
	while (1) {
		UDPrecv(u, soc, addr_from);
		if (u.getFlags() & FLAG_EXIST) { //�Ϸ��Լ�� �Ƿ��������Ӧ
			if (u.check()) {
				v.setAckSeq(u);
				v.setSeq(seq++); //ACK�ش�
				u.setSeq((seq++));
				UDPsend(v, soc, addr_to);
				memset((char*)&v, 0, sizeof(UDP_Reli));
				return 1; //���ճɹ�
			}
			else {
				printf("[��־]����һ��У��ʹ�����\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
			}
		}
	}
	printf("[����]ǿ���˳�\n");// ���ǿ���˳����ֶ�������������
	return 0;
}
