#include <Jce.h>
#include <Wup.h>
#include <catch2/catch.hpp>

using namespace YumeBot;
using namespace Cafe::Encoding::StringLiterals;

TEST_CASE("Jce", "[Jce]")
{
	using namespace Jce;

	SECTION("Serialization")
	{
		const auto test = std::make_shared<JceTest>();
		test->SetTestFloat(2.0f);
		test->SetTestInt(233);
		test->GetTestMap()[1] = 2.0f;
		test->GetTestMap()[3] = 5.0f;

		Cafe::Io::MemoryStream memoryStream;

		{
			JceOutputStream outputStream{ &memoryStream };
			outputStream.Write(0, test);
		}

		memoryStream.SeekFromBegin(0);

		std::shared_ptr<JceTest> ptr;

		{
			JceInputStream inputStream{ &memoryStream };
			const auto readSucceed = inputStream.Read(0, ptr);
			REQUIRE(readSucceed);
		}

		CHECK(ptr->GetTestFloat() == test->GetTestFloat());
		CHECK(ptr->GetTestInt() == test->GetTestInt());
		CHECK(ptr->GetTestMap() == test->GetTestMap());
		CHECK(ptr->GetTestList() == std::vector{ 1.0, 2.0, 3.0 });
	}

	SECTION("Wup.UniAttribute")
	{
		using namespace Wup;

		OldUniAttribute uniAttribute{};

		{
			uniAttribute.Put(u8"SomeInt"_s, 1);
			std::int32_t intValue;
			REQUIRE(uniAttribute.Get(u8"SomeInt"_s, intValue));
			CHECK(intValue == 1);

			uniAttribute.Put(u8"SomeFloat"_s, 1.0f);
			float floatValue;
			REQUIRE(uniAttribute.Get(u8"SomeFloat"_s, floatValue));
			CHECK(floatValue == 1.0f);
		}

		Cafe::Io::MemoryStream memoryStream;
		uniAttribute.Encode(&memoryStream);

		const auto span = memoryStream.GetInternalStorage();

		memoryStream.SeekFromBegin(0);

		{
			OldUniAttribute readAttribute{};
			readAttribute.Decode(&memoryStream);

			std::int32_t intValue;
			REQUIRE(readAttribute.Get(u8"SomeInt"_s, intValue));
			CHECK(intValue == 1);

			float floatValue;
			REQUIRE(readAttribute.Get(u8"SomeFloat"_s, floatValue));
			CHECK(floatValue == 1.0f);
		}
	}

	SECTION("Wup.UniPacket")
	{
		using namespace Wup;

		UniPacket packet;

		packet.GetAttribute().Put(u8"SomeInt"_s, 1);
		const auto test = std::make_shared<JceTest>();
		test->SetTestFloat(2.0f);
		test->SetTestInt(233);
		test->GetTestMap()[1] = 2.0f;
		packet.GetAttribute().Put(u8"JceTest"_s, test);

		packet.GetRequestPacket().SetsFuncName(u8"FuncName?"_sv);
		packet.GetRequestPacket().SetsServantName(u8"ServantName?"_sv);

		Cafe::Io::MemoryStream memoryStream;
		packet.Encode(&memoryStream);

		memoryStream.SeekFromBegin(0);

		{
			UniPacket readPacket;
			readPacket.Decode(&memoryStream);

			const auto& attribute = readPacket.GetAttribute();
			std::int32_t intValue;
			REQUIRE(attribute.Get(u8"SomeInt"_s, intValue));
			CHECK(intValue == 1);

			std::shared_ptr<JceTest> ptrValue;
			REQUIRE(attribute.Get(u8"JceTest"_s, ptrValue));
			REQUIRE(ptrValue);
			CHECK(ptrValue->GetTestFloat() == 2.0f);
			CHECK(ptrValue->GetTestInt() == 233);
			CHECK(ptrValue->GetTestMap()[1] == 2.0f);

			const auto& requestPacket = readPacket.GetRequestPacket();
			CHECK(requestPacket.GetsFuncName() == u8"FuncName?"_sv);
			CHECK(requestPacket.GetsServantName() == u8"ServantName?"_sv);
		}
	}
}
