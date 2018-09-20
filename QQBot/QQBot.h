#pragma once

#pragma warning (disable : 4996)

#include <natRefObj.h>
#define ASIO_STANDALONE 1
#define _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING 1
#include <asio.hpp>

namespace QQBot
{
	class Bot
		: public NatsuLib::natRefObjImpl<Bot>
	{
	public:
		Bot();
		~Bot();

	private:
		asio::io_service m_Service;
		asio::ip::tcp::socket m_Socket;
	};
}
