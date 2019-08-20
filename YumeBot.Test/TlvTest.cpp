#include <Tlv.h>
#include <Utility.h>
#include <catch2/catch.hpp>

using namespace YumeBot;
using namespace Cafe::Encoding::StringLiterals;

TEST_CASE("Tlv", "[Tlv]")
{
	using namespace Tlv;

	SECTION("Serialization")
	{
		Cafe::Io::MemoryStream stream;

		TlvBuilder builder{ &stream };
		builder.WriteTlv(
		    TlvT<1>{ .Uin = 123456, .ServerTime = Utility::GetPosixTime(), .ClientIp = {} });
	}
}
