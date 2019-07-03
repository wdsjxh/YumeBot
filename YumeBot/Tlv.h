#pragma once
#include "Cryptography.h"
#include "Misc.h"
#include <Cafe/Io/StreamHelpers/BinaryWriter.h>
#include <Cafe/Io/Streams/MemoryStream.h>
#include <asio/ip/address_v4.hpp>
#include <random>

namespace YumeBot::Tlv
{
	template <std::uint16_t Cmd>
	struct TlvT
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
		}
	};

	class TlvBuilder
	{
	public:
		explicit TlvBuilder(Cafe::Io::OutputStream* stream);

		template <std::uint16_t Cmd>
		void WriteTlv(TlvT<Cmd> const& tlv)
		{
			const auto seekableStream = dynamic_cast<Cafe::Io::SeekableStreamBase*>(m_Writer.GetStream());
			assert(seekableStream);

			m_Writer.Write(Cmd);
			const auto lengthPos = seekableStream->GetPosition();
			m_Writer.Write(std::uint16_t{});

			tlv.Write(m_Writer);

			const auto length = seekableStream->GetPosition() - lengthPos - 2;
			assert(length <= std::numeric_limits<std::uint16_t>::max());
			seekableStream->SeekFromBegin(lengthPos);
			m_Writer.Write(static_cast<std::uint16_t>(length));
		}

	private:
		Cafe::Io::BinaryWriter m_Writer;
	};

	template <>
	struct TlvT<0x1>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(Uin);
			writer.Write(ServerTime);
			if (!ClientIp.is_unspecified())
			{
				writer.GetStream()->WriteBytes(gsl::as_bytes(gsl::make_span(ClientIp.to_bytes())));
			}
		}

		std::uint32_t Uin;
		std::uint32_t ServerTime;
		asio::ip::address_v4 ClientIp = {};
	};

	template <>
	struct TlvT<0x2>
	{
		static constexpr std::uint16_t SigVer = 0;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(SigVer);
			const auto codeSize = Code.size();
			const auto keySize = Key.size();

			assert(codeSize <= std::numeric_limits<std::uint16_t>::max() &&
			       keySize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(static_cast<std::uint16_t>(codeSize));
			writer.GetStream()->WriteBytes(Code);
			writer.Write(static_cast<std::uint16_t>(keySize));
			writer.GetStream()->WriteBytes(Key);
		}

		gsl::span<const std::byte> Code;
		gsl::span<const std::byte> Key;
	};

	template <>
	struct TlvT<0x8>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(TimeZoneVer);
			writer.Write(LocaleId);
			writer.Write(TimeZoneOffset);
		}

		std::uint16_t TimeZoneVer = 0;
		std::uint32_t LocaleId = static_cast<std::uint32_t>(UsingLocaleId);
		std::uint16_t TimeZoneOffset = 0;
	};

	template <>
	struct TlvT<0x18>
	{
		static constexpr std::uint16_t PingVersion = 1;
		static constexpr std::uint32_t SsoVersion = 0x0600;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(PingVersion);
			writer.Write(SsoVersion);
			writer.Write(AppId);
			writer.Write(ClientVersion);
			writer.Write(Uin);
			writer.Write(Rc);
			writer.Write(std::uint16_t{});
		}

		std::uint32_t AppId;
		std::uint32_t ClientVersion;
		std::uint32_t Uin;
		std::uint16_t Rc;
	};

	template <>
	struct TlvT<0x100>
	{
		static constexpr std::uint16_t DbBufVer = 1;
		static constexpr std::uint32_t SsoVer = 5;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(DbBufVer);
			writer.Write(SsoVer);
			writer.Write(AppId);
			writer.Write(WxAppId);
			writer.Write(ClientVer);
			writer.Write(GetSig);
		}

		std::uint32_t AppId;
		std::uint32_t WxAppId;
		std::uint32_t ClientVer;
		std::uint32_t GetSig;
	};

	template <>
	struct TlvT<0x104>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(SigSession);
		}

		gsl::span<const std::byte> SigSession;
	};

	template <>
	struct TlvT<0x106>
	{
		static constexpr std::uint16_t TGTGTVer = 3;
		static constexpr std::uint32_t SsoVer = 5;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			std::byte unencryptedBody[98]{};
			Cafe::Io::ExternalMemoryOutputStream unencryptedBodyStream{ gsl::make_span(unencryptedBody) };
			Cafe::Io::BinaryWriter unencryptedBodyWriter{ &unencryptedBodyStream, std::endian::big };

			unencryptedBodyWriter.Write(TGTGTVer);

			std::default_random_engine ran{ std::random_device{}() };
			std::uniform_int_distribution<std::uint32_t> dist{ 0,
				                                                 std::numeric_limits<std::int32_t>::max() };

			unencryptedBodyWriter.Write(dist(ran));

			unencryptedBodyWriter.Write(SsoVer);
			unencryptedBodyWriter.Write(AppId);
			unencryptedBodyWriter.Write(ClientVer);
			unencryptedBodyWriter.Write(Uin);
			unencryptedBodyStream.WriteBytes(InitTime);
			if (!ClientIp.is_unspecified())
			{
				unencryptedBodyStream.WriteBytes(gsl::as_bytes(gsl::make_span(ClientIp.to_bytes())));
			}
			unencryptedBodyWriter.Write(SavePwd);
			unencryptedBodyStream.WriteBytes(Md5);
			unencryptedBodyStream.WriteBytes(TGTGTKey);
			unencryptedBodyWriter.Write(std::uint32_t{});
			unencryptedBodyWriter.Write(ReadFlg);

			if (Guid.empty())
			{
				std::uniform_int_distribution<std::uint32_t> dist{};
				for (std::size_t i = 0; i < 4; ++i)
				{
					unencryptedBodyWriter.Write(dist(ran));
				}
			}
			else
			{
				unencryptedBodyStream.WriteBytes(Guid);
			}

			unencryptedBodyWriter.Write(SubAppId);
			unencryptedBodyWriter.Write(SigSrc);

			std::byte encryptKeyContent[24];
			Cafe::Io::ExternalMemoryOutputStream encryptKeyStream{ gsl::make_span(encryptKeyContent) };
			Cafe::Io::BinaryWriter encryptKeyWriter{ &encryptKeyStream, std::endian::big };

			encryptKeyStream.WriteBytes(Md5);
			if (MSalt)
			{
				encryptKeyWriter.Write(MSalt);
			}
			else
			{
				encryptKeyWriter.Write(Uin);
			}

			std::uint32_t encryptKey[4];
			Cryptography::Md5::Calculate(gsl::make_span(encryptKeyContent),
			                             gsl::as_writeable_bytes(gsl::make_span(encryptKey)));

			std::byte encryptedBody[Cryptography::Tea::CalculateOutputSize(std::size(unencryptedBody))];
			const auto size =
			    Cryptography::Tea::Encrypt(gsl::make_span(unencryptedBody), gsl::make_span(encryptedBody),
			                               gsl::make_span(encryptKey));

			writer.GetStream()->WriteBytes(gsl::make_span(encryptedBody, size));
		}

		std::uint32_t AppId;
		std::uint32_t SubAppId;
		std::uint32_t ClientVer;
		std::uint64_t Uin;
		gsl::span<const std::byte> InitTime;
		asio::ip::address_v4 ClientIp;
		bool SavePwd;
		gsl::span<const std::byte> Md5;
		std::uint64_t MSalt;
		gsl::span<const std::byte> TGTGTKey;
		bool ReadFlg;
		gsl::span<const std::byte> Guid;
		std::uint32_t SigSrc;
	};

	template <>
	struct TlvT<0x107>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(PicType);
			writer.Write(CapType);
			writer.Write(PicSize);
			writer.Write(RetType);
		}

		std::uint16_t PicType;
		std::uint8_t CapType;
		std::uint16_t PicSize;
		std::uint8_t RetType;
	};

	template <>
	struct TlvT<0x108>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(Ksid);
		}

		gsl::span<const std::byte> Ksid;
	};

	template <>
	struct TlvT<0x109>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(Imei);
		}

		gsl::span<const std::byte> Imei;
	};

	template <>
	struct TlvT<0x10A>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(TGT);
		}

		gsl::span<const std::byte> TGT;
	};

	template <>
	struct TlvT<0x112>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(gsl::as_bytes(Name.GetTrimmedSpan()));
		}

		UsingStringView Name;
	};

	template <>
	struct TlvT<0x116>
	{
		static constexpr std::uint8_t Ver = 0;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(Ver);
			writer.Write(Bitmap);
			writer.Write(GetSig);
			const auto size = AppId.size();
			assert(size <= std::numeric_limits<std::uint8_t>::max());
			writer.Write(static_cast<std::uint8_t>(size));
			for (const auto appId : AppId)
			{
				writer.Write(appId);
			}
		}

		std::uint32_t Bitmap;
		std::uint32_t GetSig;
		gsl::span<const std::uint32_t> AppId;
	};

	template <>
	struct TlvT<0x124>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto osTypeTrimmedSpan = gsl::as_bytes(OsType.GetTrimmedSpan());
			const auto osType =
			    osTypeTrimmedSpan.subspan(0, std::min(osTypeTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			const auto osVerTrimmedSpan = gsl::as_bytes(OsVer.GetTrimmedSpan());
			const auto osVer =
			    osVerTrimmedSpan.subspan(0, std::min(osVerTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			const auto netDetailTrimmedSpan = gsl::as_bytes(NetDetail.GetTrimmedSpan());
			const auto netDetail = netDetailTrimmedSpan.subspan(
			    0, std::min(netDetailTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			const auto addrTrimmedSpan = gsl::as_bytes(Addr.GetTrimmedSpan());
			const auto addr =
			    addrTrimmedSpan.subspan(0, std::min(addrTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			const auto apnTrimmedSpan = gsl::as_bytes(Apn.GetTrimmedSpan());
			const auto apn =
			    apnTrimmedSpan.subspan(0, std::min(apnTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			writer.Write(static_cast<std::uint16_t>(osType.size()));
			writer.GetStream()->WriteBytes(osType);
			writer.Write(static_cast<std::uint16_t>(osVer.size()));
			writer.GetStream()->WriteBytes(osVer);
			writer.Write(NetType);
			writer.Write(static_cast<std::uint16_t>(netDetail.size()));
			writer.GetStream()->WriteBytes(netDetail);
			writer.Write(static_cast<std::uint16_t>(addr.size()));
			writer.GetStream()->WriteBytes(addr);
			writer.Write(static_cast<std::uint16_t>(apn.size()));
			writer.GetStream()->WriteBytes(apn);
		}

		UsingStringView OsType;
		UsingStringView OsVer;
		std::uint16_t NetType;
		UsingStringView NetDetail;
		UsingStringView Addr;
		UsingStringView Apn;
	};

	template <>
	struct TlvT<0x127>
	{
		static constexpr std::uint16_t Version = 0;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(Version);

			const auto codeSize = Code.size();
			const auto randomSize = Random.size();

			assert(codeSize <= std::numeric_limits<std::uint16_t>::max() &&
			       randomSize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(static_cast<std::uint16_t>(codeSize));
			writer.GetStream()->WriteBytes(Code);
			writer.Write(static_cast<std::uint16_t>(randomSize));
			writer.GetStream()->WriteBytes(Random);
		}

		gsl::span<const std::byte> Code;
		gsl::span<const std::byte> Random;
	};

	template <>
	struct TlvT<0x128>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto deviceTypeTrimmedSpan = gsl::as_bytes(DeviceType.GetTrimmedSpan());
			const auto deviceType = deviceTypeTrimmedSpan.subspan(
			    0, std::min(deviceTypeTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			const auto guidTrimmedSpan = gsl::as_bytes(Guid.GetTrimmedSpan());
			const auto guid =
			    guidTrimmedSpan.subspan(0, std::min(guidTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			const auto brandTrimmedSpan = gsl::as_bytes(Brand.GetTrimmedSpan());
			const auto brand =
			    brandTrimmedSpan.subspan(0, std::min(brandTrimmedSpan.size(), std::ptrdiff_t{ 16 }));

			writer.Write(std::uint16_t{});
			writer.Write(NewIns);
			writer.Write(ReadGuid);
			writer.Write(GuidChg);
			writer.Write(Flag);

			writer.Write(static_cast<std::uint16_t>(deviceType.size()));
			writer.GetStream()->WriteBytes(deviceType);

			writer.Write(static_cast<std::uint16_t>(guid.size()));
			writer.GetStream()->WriteBytes(guid);

			writer.Write(static_cast<std::uint16_t>(brand.size()));
			writer.GetStream()->WriteBytes(brand);
		}

		std::uint8_t NewIns;
		std::uint8_t ReadGuid;
		std::uint8_t GuidChg;
		std::uint32_t Flag;
		UsingStringView DeviceType;
		UsingStringView Guid;
		UsingStringView Brand;
	};

	template <>
	struct TlvT<0x141>
	{
		static constexpr std::uint16_t Version = 1;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto operatorName = gsl::as_bytes(OperatorName.GetTrimmedSpan());
			const auto apn = gsl::as_bytes(Apn.GetTrimmedSpan());

			const auto operatorNameSize = operatorName.size();
			const auto apnSize = apn.size();

			assert(operatorNameSize <= std::numeric_limits<std::uint16_t>::max() &&
			       apnSize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(Version);

			writer.Write(static_cast<std::uint16_t>(operatorNameSize));
			writer.GetStream()->WriteBytes(operatorName);

			writer.Write(NetworkType);

			writer.Write(static_cast<std::uint16_t>(apnSize));
			writer.GetStream()->WriteBytes(apn);
		}

		UsingStringView OperatorName;
		std::uint16_t NetworkType;
		UsingStringView Apn;
	};

	template <>
	struct TlvT<0x142>
	{
		static constexpr std::uint16_t Version = 0;

		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto id = gsl::as_bytes(Id.GetTrimmedSpan());
			const auto idSize = id.size();
			assert(idSize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(Version);

			writer.Write(static_cast<std::uint16_t>(idSize));
			writer.GetStream()->WriteBytes(id);
		}

		UsingStringView Id;
	};

	template <>
	struct TlvT<0x143>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(B);
		}

		gsl::span<const std::byte> B;
	};

	template <>
	struct TlvT<0x144>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const std::uint16_t tlvCount = !_109.empty() + !_124.empty() + !_128.empty() + !_148.empty() +
			                               !_151.empty() + !_153.empty() + !_16e.empty();
			Cafe::Io::MemoryStream unencryptedDataStream;
			Cafe::Io::BinaryWriter unencryptedDataWriter{ &unencryptedDataStream, std::endian::big };
			unencryptedDataWriter.Write(tlvCount);
			unencryptedDataStream.WriteBytes(_109);
			unencryptedDataStream.WriteBytes(_124);
			unencryptedDataStream.WriteBytes(_128);
			unencryptedDataStream.WriteBytes(_148);
			unencryptedDataStream.WriteBytes(_151);
			unencryptedDataStream.WriteBytes(_153);
			unencryptedDataStream.WriteBytes(_16e);

			std::vector<std::byte> encrypedData(
			    Cryptography::Tea::CalculateOutputSize(unencryptedDataStream.GetTotalSize()));
			const auto size = Cryptography::Tea::Encrypt(unencryptedDataStream.GetInternalStorage(),
			                                             gsl::make_span(encrypedData), Key);

			writer.GetStream()->WriteBytes(gsl::make_span(encrypedData.data(), size));
		}

		gsl::span<const std::byte> _109;
		gsl::span<const std::byte> _124;
		gsl::span<const std::byte> _128;
		gsl::span<const std::byte> _148;
		gsl::span<const std::byte> _151;
		gsl::span<const std::byte> _153;
		gsl::span<const std::byte> _16e;
		gsl::span<const std::uint32_t, 4> Key;
	};

	template <>
	struct TlvT<0x145>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(Guid);
		}

		gsl::span<const std::byte> Guid;
	};

	template <>
	struct TlvT<0x147>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto appVerTrimmedSpan = gsl::as_bytes(AppVer.GetTrimmedSpan());
			const auto appVer =
			    appVerTrimmedSpan.subspan(0, std::min(appVerTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			const auto appSignTrimmedSpan = gsl::as_bytes(AppSign.GetTrimmedSpan());
			const auto appSign =
			    appSignTrimmedSpan.subspan(0, std::min(appSignTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			writer.Write(AppVerId);

			writer.Write(static_cast<std::uint16_t>(appVer.size()));
			writer.GetStream()->WriteBytes(appVer);

			writer.Write(static_cast<std::uint16_t>(appSign.size()));
			writer.GetStream()->WriteBytes(appSign);
		}

		std::uint32_t AppVerId;
		UsingStringView AppVer;
		UsingStringView AppSign;
	};

	template <>
	struct TlvT<0x148>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto appNameTrimmedSpan = gsl::as_bytes(AppName.GetTrimmedSpan());
			const auto appName =
			    appNameTrimmedSpan.subspan(0, std::min(appNameTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			const auto appVerTrimmedSpan = gsl::as_bytes(AppVer.GetTrimmedSpan());
			const auto appVer =
			    appVerTrimmedSpan.subspan(0, std::min(appVerTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			const auto appSignTrimmedSpan = gsl::as_bytes(AppSign.GetTrimmedSpan());
			const auto appSign =
			    appSignTrimmedSpan.subspan(0, std::min(appSignTrimmedSpan.size(), std::ptrdiff_t{ 32 }));

			writer.Write(static_cast<std::uint16_t>(appName.size()));
			writer.GetStream()->WriteBytes(appName);

			writer.Write(SsoVer);
			writer.Write(AppID);
			writer.Write(SubAppID);

			writer.Write(static_cast<std::uint16_t>(appVer.size()));
			writer.GetStream()->WriteBytes(appVer);

			writer.Write(static_cast<std::uint16_t>(appSign.size()));
			writer.GetStream()->WriteBytes(appSign);
		}

		UsingStringView AppName;
		std::uint32_t SsoVer;
		std::uint32_t AppID;
		std::uint32_t SubAppID;
		UsingStringView AppVer;
		UsingStringView AppSign;
	};

	template <>
	struct TlvT<0x153>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(IsRoot);
		}

		std::uint16_t IsRoot;
	};

	template <>
	struct TlvT<0x154>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(SsoSeq);
		}

		std::uint32_t SsoSeq;
	};

	template <>
	struct TlvT<0x166>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(ImgType);
		}

		std::uint8_t ImgType;
	};

	template <>
	struct TlvT<0x16A>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(NoPicSig);
		}

		gsl::span<const std::byte> NoPicSig;
	};

	template <>
	struct TlvT<0x16B>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto domainSize = Domains.size();
			assert(domainSize <= std::numeric_limits<std::uint16_t>::max());
			writer.Write(static_cast<std::uint16_t>(domainSize));

			for (const auto& domain : Domains)
			{
				const auto trimedDomain = domain.Trim();
				const auto size = trimedDomain.GetSize();
				assert(size <= std::numeric_limits<std::uint16_t>::max());
				writer.Write(static_cast<std::uint16_t>(size));
				writer.GetStream()->WriteBytes(gsl::as_bytes(trimedDomain.GetSpan()));
			}
		}

		gsl::span<const UsingStringView> Domains;
	};

	template <>
	struct TlvT<0x16E>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(gsl::as_bytes(DeviceName.GetTrimmedSpan()));
		}

		UsingStringView DeviceName;
	};

	template <>
	struct TlvT<0x172>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(RollbackSig);
		}

		gsl::span<const std::byte> RollbackSig;
	};

	template <>
	struct TlvT<0x174>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(SecSig);
		}

		gsl::span<const std::byte> SecSig;
	};

	template <>
	struct TlvT<0x177>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto buildVersion = gsl::as_bytes(BuildVersion.GetTrimmedSpan());
			const auto buildVersionSize = buildVersion.size();
			assert(buildVersionSize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(std::uint8_t{ 1 });
			writer.Write(BuildTime);

			writer.Write(static_cast<std::uint16_t>(buildVersionSize));
			writer.GetStream()->WriteBytes(buildVersion);
		}

		std::uint32_t BuildTime;
		UsingStringView BuildVersion;
	};

	template <>
	struct TlvT<0x17A>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(SmsAppId);
		}

		std::uint32_t SmsAppId;
	};

	template <>
	struct TlvT<0x17C>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto smsCodeSize = SmsCode.size();
			assert(smsCodeSize <= std::numeric_limits<std::uint16_t>::max());

			writer.Write(static_cast<std::uint16_t>(smsCodeSize));
			writer.GetStream()->WriteBytes(SmsCode);
		}

		gsl::span<const std::byte> SmsCode;
	};

	template <>
	struct TlvT<0x183>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(MSalt);
		}

		std::uint64_t MSalt;
	};

	template <>
	struct TlvT<0x184>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			const auto mPassword = gsl::as_bytes(MPassword.GetTrimmedSpan());

			std::byte body[24];
			Cryptography::Md5::Calculate(mPassword, gsl::make_span(body).subspan(0, 16));

			Cafe::Io::ExternalMemoryOutputStream stream{ gsl::make_span(body) };
			Cafe::Io::BinaryWriter{ &stream, std::endian::big }.Write(MSalt);

			std::byte md5Body[16];
			Cryptography::Md5::Calculate(gsl::make_span(body), gsl::make_span(md5Body));

			writer.GetStream()->WriteBytes(gsl::make_span(md5Body));
		}

		std::uint64_t MSalt;
		UsingStringView MPassword;
	};

	template <>
	struct TlvT<0x185>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.Write(std::uint8_t{ 1 });
			writer.Write(Flag);
		}

		std::uint8_t Flag;
	};

	template <>
	struct TlvT<0x187>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(Mac);
		}

		gsl::span<const std::byte, 16> Mac;
	};

	template <>
	struct TlvT<0x188>
	{
		void Write(Cafe::Io::BinaryWriter& writer) const
		{
			writer.GetStream()->WriteBytes(AndroidID);
		}

		gsl::span<const std::byte, 16> AndroidID;
	};
} // namespace YumeBot::Tlv
