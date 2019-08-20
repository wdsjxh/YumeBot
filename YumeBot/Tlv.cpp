#include "Tlv.h"

using namespace YumeBot;
using namespace Tlv;

TlvBuilder::TlvBuilder(Cafe::Io::OutputStream* stream, std::size_t initialTlvCount)
    : m_Writer{ stream, std::endian::big }, m_TlvCount{ initialTlvCount }
{
	assert(dynamic_cast<Cafe::Io::SeekableStreamBase*>(stream) && "stream should be seekable.");
}

std::size_t TlvBuilder::GetTlvCount() const noexcept
{
	return m_TlvCount;
}

TlvReader::TlvReader(Cafe::Io::InputStream* stream) : m_Reader{ stream, std::endian::big }
{
	assert(dynamic_cast<Cafe::Io::SeekableStreamBase*>(stream) && "stream should be seekable.");
}
