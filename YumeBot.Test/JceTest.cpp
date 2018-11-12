#include <catch.hpp>
#include <Jce.h>
#include <Wup.h>
#include <natStream.h>

using namespace NatsuLib;
using namespace YumeBot;

TEST_CASE("Jce", "[Jce]")
{
	using namespace Jce;

	SECTION("Serialization")
	{
		const auto test = make_ref<JceTest>();
		test->SetTestFloat(2.0f);
		test->SetTestInt(233);
		test->GetTestMap()[1] = 2.0f;
		test->GetTestMap()[3] = 5.0f;

		const auto memoryStream = make_ref<natMemoryStream>(0, true, true, true);

		{
			JceOutputStream outputStream{ make_ref<natBinaryWriter>(memoryStream) };
			outputStream.Write(0, test);
		}

		memoryStream->SetPositionFromBegin(0);

		natRefPointer<JceTest> ptr;

		{
			JceInputStream inputStream{ make_ref<natBinaryReader>(memoryStream) };
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
			uniAttribute.Put(u8"SomeInt"_ns, 1);
			std::int32_t intValue;
			REQUIRE(uniAttribute.Get(u8"SomeInt"_ns, intValue));
			CHECK(intValue == 1);

			uniAttribute.Put(u8"SomeFloat"_ns, 1.0f);
			float floatValue;
			REQUIRE(uniAttribute.Get(u8"SomeFloat"_ns, floatValue));
			CHECK(floatValue == 1.0f);
		}

		const auto memoryStream = make_ref<natMemoryStream>(0, true, true, true);
		uniAttribute.Encode(make_ref<natBinaryWriter>(memoryStream));

		memoryStream->SetPositionFromBegin(0);

		{
			OldUniAttribute readAttribute{};
			readAttribute.Decode(make_ref<natBinaryReader>(memoryStream));

			std::int32_t intValue;
			REQUIRE(readAttribute.Get(u8"SomeInt"_ns, intValue));
			CHECK(intValue == 1);

			float floatValue;
			REQUIRE(readAttribute.Get(u8"SomeFloat"_ns, floatValue));
			CHECK(floatValue == 1.0f);
		}
	}

	SECTION("Wup.UniPacket")
	{
		using namespace Wup;

		UniPacket packet;

		packet.GetAttribute().Put(u8"SomeInt"_ns, 1);
		const auto test = make_ref<JceTest>();
		test->SetTestFloat(2.0f);
		test->SetTestInt(233);
		test->GetTestMap()[1] = 2.0f;
		packet.GetAttribute().Put(u8"JceTest"_ns, test);

		packet.GetRequestPacket().SetsFuncName(u8"FuncName?"_nv);
		packet.GetRequestPacket().SetsServantName(u8"ServantName?"_nv);

		const auto memoryStream = make_ref<natMemoryStream>(0, true, true, true);
		packet.Encode(make_ref<natBinaryWriter>(memoryStream));

		memoryStream->SetPositionFromBegin(0);

		{
			UniPacket readPacket;
			readPacket.Decode(make_ref<natBinaryReader>(memoryStream));

			const auto& attribute = readPacket.GetAttribute();
			std::int32_t intValue;
			REQUIRE(attribute.Get(u8"SomeInt"_ns, intValue));
			CHECK(intValue == 1);

			natRefPointer<JceTest> ptrValue;
			REQUIRE(attribute.Get(u8"JceTest"_ns, ptrValue));
			REQUIRE(ptrValue);
			CHECK(ptrValue->GetTestFloat() == 2.0f);
			CHECK(ptrValue->GetTestInt() == 233);
			CHECK(ptrValue->GetTestMap()[1] == 2.0f);

			const auto& requestPacket = readPacket.GetRequestPacket();
			CHECK(requestPacket.GetsFuncName() == u8"FuncName?"_nv);
			CHECK(requestPacket.GetsServantName() == u8"ServantName?"_nv);
		}
	}
}
