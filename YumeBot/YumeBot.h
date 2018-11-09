#pragma once

#ifdef _WIN32
#include <SDKDDKVer.h>
#endif // _WIN32

#define ASIO_STANDALONE 1
#define _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING 1
#include <asio.hpp>

#include <natRefObj.h>

namespace YumeBot
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
