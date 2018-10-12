#include "Jce.h"

using namespace NatsuLib;
using namespace QQBot;
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
