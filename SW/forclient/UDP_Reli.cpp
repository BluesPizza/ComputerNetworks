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
	//checksum置零重算 和原来相同即可
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
//UDP发包封装 自动设置有效位 校验和
int UDPsend(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_to) {
	u.setFlags(FLAG_EXIST); //有效位
	u.setCheckSum();
	if (sendto(soc, (char*)&u, sizeof(u), 0, (SOCKADDR*)&addr_to, sizeof(addr_to)) == SOCKET_ERROR) {
		cout << "[警告]数据发送失败" << endl;
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
	u.setSeq((seq++)); //设置序号
	UDPsend(u, soc, addr_to);
	time_t start = clock(); //计时器 
	time_t end;
	while (true) {
		UDPrecv(v, soc, addr_from);
		if ((v.getFlags() & FLAG_ACK) && v.getAck() == u.getSeq()) { //检查ACK是否合法 非法ACK将不予承认 计时继续
			printf("[日志]收到有效回传ACK\nseq:%u, ack=%u, checksum=%u\n", v.getSeq(), v.getAck(), v.getCheckSum());
			return 1;//确认回复
		}
		if (count == MAX_RESEND) {
			printf("[警告]多次重发失败\n");
			return 0; //重发过多次失败
		}
		end = clock();
		if (((end - start) / CLOCKS_PER_SEC) >= TIMEOUT) { //超时重发
			start = clock();
			UDPsend(u, soc, addr_to);
			count++;
			printf("[日志]第%d次重发，seq=%u, ack=%u, checksum=%u\n", count, u.getSeq(), u.getAck(), u.getCheckSum());
		}
	}
}

int stopAndWaitRecv(UDP_Reli& u, UDP_Reli v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq) {
	while (1) {
		UDPrecv(u, soc, addr_from);
		if (u.getFlags() & FLAG_EXIST) { //合法性检查 非法包不予回应
			if (u.check()) {
				v.setAckSeq(u);
				v.setSeq(seq++); //ACK回传
				u.setSeq((seq++));
				UDPsend(v, soc, addr_to);
				memset((char*)&v, 0, sizeof(UDP_Reli));
				return 1; //接收成功
			}
			else {
				printf("[日志]发现一份校验和错误报文\nseq=%u, ack=%u, checksum=%u\n", u.getSeq(), u.getAck(), u.getCheckSum());
			}
		}
	}
	printf("[警告]强制退出\n");// 这个强制退出是手动结束程序会输出
	return 0;
}
