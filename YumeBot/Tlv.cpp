#include "Tlv.h"

using namespace YumeBot;
using namespace Tlv;

TlvBuilder::TlvBuilder(Cafe::Io::OutputStream *stream) : m_Writer{ stream, std::endian::big }
{
	assert(dynamic_cast<Cafe::Io::SeekableStreamBase*>(stream) && "stream should be seekable.");
}
