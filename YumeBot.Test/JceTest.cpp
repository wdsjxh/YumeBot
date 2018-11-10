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

	SECTION("Wup.UniPacket")
	{
		using namespace Wup;

		OldUniAttribute uniAttribute{};
		uniAttribute.Put(u8"SomeInt"_ns, 1);
		uniAttribute.Put(u8"SomeFloat"_ns, 1.0f);

		const auto memoryStream = make_ref<natMemoryStream>(0, true, true, true);
		uniAttribute.Encode(make_ref<natBinaryWriter>(memoryStream));

		memoryStream->SetPositionFromBegin(0);

		{
			OldUniAttribute readAttribute{};
			readAttribute.Decode(make_ref<natBinaryReader>(memoryStream));

			CHECK(uniAttribute.Get<std::int32_t>(u8"SomeInt"_ns) == readAttribute.Get<std::int32_t>(u8"SomeInt"_ns));
			CHECK(uniAttribute.Get<float>(u8"SomeFloat"_ns) == readAttribute.Get<float>(u8"SomeFloat"_ns));
		}
	}
}
