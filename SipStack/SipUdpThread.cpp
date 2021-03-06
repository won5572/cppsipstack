/* 
 * Copyright (C) 2012 Yee Young Han <websearch@naver.com> (http://blog.naver.com/websearch)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "SipStackThread.h"
#include "ServerUtility.h"
#include "Log.h"
#include "MemoryDebug.h"

/**
 * @ingroup SipStack
 * @brief SIP 메시지를 파싱하여서 SIP stack 에 입력한다.
 * @param pclsSipStack SIP stack
 * @param iThreadId		UDP 쓰레드 번호
 * @param pszBuf			네트워크에서 수신된 SIP 메시지
 * @param iBufLen			네트워크에서 수신된 SIP 메시지의 길이
 * @param pszIp				IP 주소
 * @param iPort				포트 번호
 * @returns 성공하면 true 를 리턴하고 실패하면 false 를 리턴한다.
 */
static bool SipMessageProcess( CSipStack * pclsSipStack, int iThreadId, const char * pszBuf, int iBufLen, const char * pszIp, unsigned short iPort )
{
	CLog::Print( LOG_NETWORK, "UdpRecv(%s:%d) [%s]", pszIp, iPort, pszBuf );

	return pclsSipStack->RecvSipMessage( iThreadId, pszBuf, iBufLen, pszIp, iPort, E_SIP_UDP );
}

/** 
 * @ingroup SipStack
 * @brief UDP 프로토콜로 SIP 메시지 수신 및 SIP 수신 이벤트를 처리하는 쓰레드 함수
 * @param lpParameter SIP stack 포인터
 * @returns 0 을 리턴한다.
 */
THREAD_API SipUdpThread( LPVOID lpParameter )
{
	CSipStack * pclsSipStack = (CSipStack *)lpParameter;
	struct pollfd arrPoll[1];
	int		iThreadId, n, iPacketSize;
	char	szPacket[SIP_PACKET_MAX_SIZE], szIp[INET6_ADDRSTRLEN];
	unsigned short sPort;
	bool	bRes;

	pclsSipStack->IncreateUdpThreadCount( iThreadId );
	TcpSetPollIn( arrPoll[0], pclsSipStack->m_hUdpSocket );

	while( pclsSipStack->m_bStopEvent == false )
	{
		pclsSipStack->m_clsUdpRecvMutex.acquire();
		n = poll( arrPoll, 1, 1000 );
		if( n > 0 )
		{
			iPacketSize = sizeof(szPacket);
			bRes = UdpRecv( pclsSipStack->m_hUdpSocket, szPacket, &iPacketSize, szIp, sizeof(szIp), &sPort, pclsSipStack->m_clsSetup.m_bIpv6 );
			pclsSipStack->m_clsUdpRecvMutex.release();

			if( bRes )
			{
				if( iPacketSize < SIP_PACKET_MIN_SIZE || szPacket[0] == '\0' || szPacket[0] == '\r' || szPacket[0] == '\n' ) continue;

				SipMessageProcess( pclsSipStack, iThreadId, szPacket, iPacketSize, szIp, sPort );
			}
		}
		else
		{
			pclsSipStack->m_clsUdpRecvMutex.release();
		}
	}

	pclsSipStack->ThreadEnd( iThreadId );
	pclsSipStack->DecreateUdpThreadCount();

	return 0;
}

/**
 * @ingroup SipStack
 * @brief UDP 프로토콜로 SIP 메시지 수신 및 SIP 수신 이벤트를 처리하는 Thread Pool 을 시작한다.
 * @param pclsSipStack SIP stack 포인터
 * @returns 성공하면 true 를 리턴하고 실패하면 false 를 리턴한다.
 */
bool StartSipUdpThread( CSipStack * pclsSipStack )
{
	for( int i = 0; i < pclsSipStack->m_clsSetup.m_iUdpThreadCount; ++i )
	{
		char	szMsg[101];

		snprintf( szMsg, sizeof(szMsg), "SipUdpThread(%d)", i + 1 );
		if( StartThread( szMsg, SipUdpThread, pclsSipStack ) == false )
		{
			return false;
		}
	}

	return true;
}
