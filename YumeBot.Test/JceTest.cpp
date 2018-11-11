#include "pch.h"
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

		uniAttribute.Put(u8"SomeInt"_ns, 1);
		CHECK(uniAttribute.Get<std::int32_t>(u8"SomeInt"_ns) == 1);

		uniAttribute.Put(u8"SomeFloat"_ns, 1.0f);
		CHECK(uniAttribute.Get<float>(u8"SomeFloat"_ns) == 1.0f);

		const auto memoryStream = make_ref<natMemoryStream>(0, true, true, true);
		uniAttribute.Encode(make_ref<natBinaryWriter>(memoryStream));

		memoryStream->SetPositionFromBegin(0);

		{
			OldUniAttribute readAttribute{};
			readAttribute.Decode(make_ref<natBinaryReader>(memoryStream));

			CHECK(readAttribute.Get<std::int32_t>(u8"SomeInt"_ns) == 1);
			CHECK(readAttribute.Get<float>(u8"SomeFloat"_ns) == 1.0f);
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
			CHECK(attribute.Get<std::int32_t>(u8"SomeInt"_ns) == 1);

			const auto ptr = attribute.Get<natRefPointer<JceTest>>(u8"JceTest"_ns);
			REQUIRE(ptr);
			CHECK(ptr->GetTestFloat() == 2.0f);
			CHECK(ptr->GetTestInt() == 233);
			CHECK(ptr->GetTestMap()[1] == 2.0f);

			const auto& requestPacket = readPacket.GetRequestPacket();
			CHECK(requestPacket.GetsFuncName() == u8"FuncName?"_nv);
			CHECK(requestPacket.GetsServantName() == u8"ServantName?"_nv);
		}
	}
}
