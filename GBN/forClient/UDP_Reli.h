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
#include<windows.h>
#pragma comment (lib, "ws2_32.lib")
#define MSG_SIZE 4096 //最大data字节
#define MAX_RESEND 5 //最大重发次数
#define MSG_COUNT 4096 //本地最大缓存数量
#define TIMEOUT 2 //超时时间
//flag采用独热编码 目前只用到8位 共16位
#define FLAG_ACK 1<<0 
#define FLAG_SYN 1<<1
#define FLAG_EXIST 1<<2
#define FLAG_STR 1<<3
#define FLAG_END 1<<4
#define FLAG_FIN 1<<5
#define FLAG_RESEND 1<<6
#define FLAG_CONTINUE 1<<7
#define FLAG_NULL 0
#define STATUS_OFF 0
#define STATUS_ON 1
#define STATUS_SENDING 1<<1
#define LOOPIP "127.0.0.1" //本机回环地址
#define TESTIP "10.139.157.143" //router测试IP 不固定
#define SERVERPORT 7655 //服务器端口
#define CLIENTPORT 7654 //不bind就没用的客户端端口
#define TESTPORT 7656 // router测试端口
#define WIN_SIZE 10  //窗口大小
#define ELAINA 260<<1
using namespace std;
#pragma pack(1) 
class UDP_Reli { // 28+1024
private:
	uint32_t srcIp; //源ip
	uint32_t dstIp; //目的IP
	uint32_t Seq;	//序号
	uint32_t Ack;	//确认序号
	uint16_t Flags;	//标志位 多余位备用
	uint16_t counts; //对于被划分为多个包的完整数据，标识共多少个分包，便于检验
	uint16_t srcPort;//源端口
	uint16_t dstPort;//目的端口
	uint16_t totalLen;//data段长度（STR包除外）
	uint16_t checkSum;//校验和
public:
	char dataBuf[MSG_SIZE]; //数据

	UDP_Reli() { //初始化全为0
		this->srcIp = this->dstIp = this->Seq = this->Ack = 0;
		this->Flags = this->counts = this->srcPort = this->dstPort = this->totalLen = this->checkSum = 0;
		memset(this->dataBuf, 0, sizeof(dataBuf));
	}

	UDP_Reli(const UDP_Reli& u) {
		this->srcIp = u.srcIp;
		this->dstIp = u.dstIp;
		this->Seq = u.Seq;
		this->Ack = u.Ack;
		this->Flags = u.Flags;
		this->counts = u.counts;
		this->srcPort = u.srcPort;
		this->dstPort = u.dstPort;
		this->totalLen = u.totalLen;
		this->checkSum = u.checkSum;
		memcpy(this->dataBuf, u.dataBuf, MSG_SIZE);
	}

	//计算校验和 和头部大小有关 改头部长度也必须改这里
	void setCheckSum();
	//检查校验和 和头部大小有关 改头部长度也必须改这里
	bool check();
	//设置flags为Ack  同时设置seq为正确序号
	void setAckSeq(UDP_Reli u);
	//设置flags为Syn 
	void setSyn();
	//设置Seq
	void setSeq(uint32_t seq);
	void setAck(uint32_t ack);
	//设置标志位 不会清零
	void setFlags(uint16_t flag);
	//设置源信息
	void setSrc(SOCKADDR_IN src);
	//设置目标信息
	void setDst(SOCKADDR_IN dst);
	//设置包数目
	void setCounts(uint16_t count);
	void setLen(uint16_t len);
	//清空报文
	void clearUDP();
	//获取ACK
	uint16_t getLen();
	uint16_t getCounts();
	uint32_t getAck();
	uint32_t getSeq();
	uint16_t getFlags();
	uint16_t getCheckSum();
	SOCKADDR_IN getSrc();
	SOCKADDR_IN getDst();
};


// 为了能传递给线程多个参数而定义的类
class UDP_Reli_Thread {
public:
	uint16_t totalLen;
	uint16_t count;
	UDP_Reli msg;

	UDP_Reli_Thread(UDP_Reli& u) {
		this->totalLen = u.getLen();
		this->count = u.getCounts();
		msg = u;
	}
	UDP_Reli_Thread() {
		totalLen = count = 0;
	}
};
#pragma pack()

// 发送整合
// 发送包体 套接字 目标地址
int UDPsend(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_to);
// 接收整合
// 接收包体 套接字 来源地址
void UDPrecv(UDP_Reli& u, SOCKET& soc, SOCKADDR_IN& addr_from);
//确认重传
//发送包体 接收包体 套接字 目标地址 来源地址 当前全局seq
int stopAndWaitSend(UDP_Reli& u, UDP_Reli& v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq);
//停等接收 自动设置序号 和 ACK
//接收包体 回复包体 套接字 目标地址 来源地址 当前全局seq
int stopAndWaitRecv(UDP_Reli& u, UDP_Reli& v, SOCKET& soc, SOCKADDR_IN& addr_to, SOCKADDR_IN& addr_from, uint32_t& seq);
#endif // !_UDP_RELI_H_
