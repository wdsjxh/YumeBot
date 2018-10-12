#include "Jce.h"

using namespace NatsuLib;
using namespace YumeBot;
using namespace Jce;

JceStruct::~JceStruct()
{
}

JceInputStream::JceInputStream(natRefPointer<natBinaryReader> reader)
	: m_Reader{ std::move(reader) }
{
}

JceInputStream::~JceInputStream()
{
}

JceOutputStream::JceOutputStream(natRefPointer<natBinaryWriter> writer)
	: m_Writer{ std::move(writer) }
{
}

JceOutputStream::~JceOutputStream()
{
}

#define TLV_CODE(name, code, ...) \
	name::~name()\
	{\
	}\
	\
	std::string_view name::GetJceStructName() const noexcept\
	{\
		return #name;\
	}\
	\
	TypeEnum name::GetJceStructType() const noexcept\
	{\
		return TypeEnum::name;\
	}

#include "TlvCodeDef.h"
