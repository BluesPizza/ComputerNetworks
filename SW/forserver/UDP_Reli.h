#pragma once
#ifndef _UDP_RELI_H_
#define _UDP_RELI_H_
#include<iostream>
#include<cstdio>
#include<cstdint>
#include<string>
#include<WinSock2.h>
#include <Ws2tcpip.h>
#include <time.h>
#include<fstream>
#pragma comment (lib, "ws2_32.lib")
#define MSG_SIZE 4096 //���data�ֽ�
#define MAX_RESEND 5 //����ط�����
#define TIMEOUT 2 //��ʱʱ��
//flag���ö��ȱ��� Ŀǰֻ�õ�6λ
#define FLAG_ACK 1<<0 
#define FLAG_SYN 1<<1
#define FLAG_EXIST 1<<2
#define FLAG_STR 1<<3
#define FLAG_END 1<<4
#define FLAG_FIN 1<<5
#define FLAG_NULL 0
#define STATUS_OFF 0
#define STATUS_ON 1
#define STATUS_SENDING 1<<1
#define ELAINA 260<<1
using namespace std;
#pragma pack(1) 

class UDP_Reli { // 28+1024
private:
	uint32_t srcIp; //Դip
	uint32_t dstIp; //Ŀ��IP
	uint32_t Seq;	//���
	uint32_t Ack;	//ȷ�����
	uint16_t flags;	//��־λ ����λ����
	uint16_t counts; //���ڱ�����Ϊ��������������ݣ���ʶ�����ٸ��ְ������ڼ���
	uint16_t srcPort;//Դ�˿�
	uint16_t dstPort;//Ŀ�Ķ˿�
	uint16_t totalLen;//data�γ��ȣ�STR�����⣩
	uint16_t checkSum;//У���
public:
	char dataBuf[MSG_SIZE]; //����

	UDP_Reli() { //��ʼ��ȫΪ0
		this->srcIp = this->dstIp = this->Seq = this->Ack = 0;
		this->flags = this->counts = this->srcPort = this->dstPort = this->totalLen = this->checkSum = 0;
		memset(this->dataBuf, 0, sizeof(dataBuf));
	}

	//����У��� ��ͷ����С�й� ��ͷ������Ҳ���������
	void setCheckSum();
	//���У��� ��ͷ����С�й� ��ͷ������Ҳ���������
	bool check();
	//����flagsΪAck  ͬʱ����seqΪ��ȷ���
	void setAckSeq(UDP_Reli u);
	//����flagsΪSyn 
	void setSyn();
	//����Seq
	void setSeq(uint32_t seq);
	void setAck(uint32_t ack);
	//���ñ�־λ ��������
	void setFlags(uint16_t flag);
	//����Դ��Ϣ
	void setSrc(SOCKADDR_IN src);
	//����Ŀ����Ϣ
	void setDst(SOCKADDR_IN dst);
	//���ð���Ŀ
	void setCounts(uint16_t count);
	void setLen(uint16_t len);
	//��ձ���
	void clearUDP();
	//��ȡACK
	uint16_t getLen();
	uint16_t getCounts();
	uint32_t getAck();
	uint32_t getSeq();
	uint16_t getFlags();
	uint16_t getCheckSum();
	SOCKADDR_IN getSrc();
	SOCKADDR_IN getDst();
};
#pragma pack()

// ��������
// ���Ͱ��� �׽��� Ŀ���ַ
int UDPsend(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_to);
// ��������
// ���հ��� �׽��� ��Դ��ַ
void UDPrecv(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_from);
//ȷ���ش�
//���Ͱ��� ���հ��� �׽��� Ŀ���ַ ��Դ��ַ ��ǰȫ��seq
int stopAndWaitSend(UDP_Reli& u, UDP_Reli v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq);
//ͣ�Ƚ��� �Զ�������� �� ACK
//���հ��� �ظ����� �׽��� Ŀ���ַ ��Դ��ַ ��ǰȫ��seq
int stopAndWaitRecv(UDP_Reli& u, UDP_Reli v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq);
#endif // !_UDP_RELI_H_

