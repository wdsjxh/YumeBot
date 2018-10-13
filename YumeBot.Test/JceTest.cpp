#include "pch.h"
#include <catch.hpp>
#include <Jce.h>
#include <natStream.h>

using namespace NatsuLib;
using namespace YumeBot;

TEST_CASE("Jce", "[Jce]")
{
	using namespace Jce;

	SECTION("Serialization")
	{
		const auto test = make_ref<TlvTest>();
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

		natRefPointer<TlvTest> ptr;

		{
			JceInputStream inputStream{ make_ref<natBinaryReader>(memoryStream) };
			const auto readSucceed = inputStream.Read(0, ptr);
			REQUIRE(readSucceed);
		}

		CHECK(ptr->GetTestFloat() == test->GetTestFloat());
		CHECK(ptr->GetTestInt() == test->GetTestInt());
		CHECK(ptr->GetTestMap() == test->GetTestMap());
	}
}
