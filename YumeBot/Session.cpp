#include "Session.h"

using namespace YumeBot;

SessionFactory::UserPassword::UserPassword(UsingStringView const& password) noexcept
{
	Cryptography::Md5::Calculate(gsl::as_bytes(password.GetTrimmedSpan()), PasswordMd5);
}

SessionFactory::UserPassword::UserPassword(
    gsl::span<const std::byte, 16> const& passwordMd5) noexcept
{
	std::memcpy(PasswordMd5.data(), passwordMd5.data(), 16);
}

SessionFactory::SessionFactory(Request::RequestContext commonInitialContext)
    : m_CommonInitialContext{ std::move(commonInitialContext) }
{
}

Request::RequestContext& SessionFactory::GetContext()
{
	return m_CommonInitialContext;
}
