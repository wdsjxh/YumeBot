#pragma once

#include "Tlv.h"

namespace YumeBot::Request
{
	struct KeyStorage
	{
		std::byte PubKey[25];
		std::byte ShareKey[16];
		std::byte RandomKey[16];

		struct RandomizeTag
		{
		};

		static constexpr RandomizeTag Randomize{};

		KeyStorage(gsl::span<std::byte, 25> const& pubKey, gsl::span<std::byte, 16> const& shareKey,
		           gsl::span<std::byte, 16> const& randomKey)
		{
			std::memcpy(PubKey, pubKey.data(), 25);
			std::memcpy(ShareKey, shareKey.data(), 16);
			std::memcpy(RandomKey, randomKey.data(), 16);
		}

		explicit KeyStorage(RandomizeTag)
		{
			Cryptography::Ecdh::GenerateKeyPair(gsl::make_span(PubKey), gsl::make_span(ShareKey));

			std::random_device rd;
			std::default_random_engine engine{ rd };
			std::uniform_int_distribution<> dist{ 0, std::numeric_limits<std::uint8_t>::max() };
			std::generate(std::begin(RandomKey), std::end(RandomKey),
			              [&] { return static_cast<std::byte>(dist(engine)); });
		}
	};

	struct RequestContext
	{
		std::uint32_t Uin;
		std::array<std::byte, 16> PasswordMd5;
		std::uint32_t ServerTime = Utility::GetPosixTime();
		LocaleIdEnum CurrentLocaleId = LocaleIdEnum::ZH_CN;

		UsingString OsVersion = DefaultOsVersion;

		UsingString Imei;
		UsingString WifiMac;
		UsingString AndroidId;

		KeyStorage Keys{ KeyStorage::Randomize };

		UsingString SimOperatorName;
		ConnectionTypeEnum ConnectionType;
		UsingString Apn;

		UsingString ApkVersion = DefaultApkVersion;
		gsl::span<const std::byte> ApkSignature = gsl::as_bytes(gsl::make_span(Signature));

		std::array<std::byte, 16> const& GetGuid() const
		{
			if (m_Guid.has_value())
			{
				return m_Guid.value();
			}

			const auto tmp = Imei + WifiMac;
			std::array<std::byte, 16> result;
			Cryptography::Md5::Calculate(gsl::as_bytes(tmp.GetView().GetTrimmedSpan()),
			                             gsl::make_span(result));
			return result;
		}

		std::size_t AcquireRequestSeq() const noexcept
		{
			return std::exchange(m_RequestSeq, (m_RequestSeq + 1) % 200);
		}

		std::size_t AcquireClientSeq() const noexcept
		{
			return std::exchange(m_ClientSeq, (m_ClientSeq + 1) % 200);
		}

	private:
		std::optional<std::array<std::byte, 16>> m_Guid;
		mutable std::size_t m_RequestSeq{};
		mutable std::size_t m_ClientSeq{};
	};

	/// @brief  加密类型
	enum class EncryptType
	{
		Ecdh,
		Kc
	};

	template <typename T, std::uint16_t CmdValue, std::uint16_t SubCmdValue,
	          EncryptType EncryptTypeValue>
	struct RequestBase
	{
		static constexpr std::uint16_t Cmd = CmdValue;
		static constexpr std::uint16_t SubCmd = SubCmdValue;
		static constexpr EncryptType UsingEncryptType = EncryptTypeValue;

		void Write(Tlv::TlvBuilder& tlvBuilder, RequestContext const& context, std::size_t seq) const
		{
			static_cast<const T*>(this)->DoWrite(tlvBuilder, context, seq);
		}
	};

	template <typename T, std::uint16_t SubCmdValue>
	struct ResponseBase
	{
		static constexpr std::uint16_t SubCmd = SubCmdValue;

		void ProcessResponse(Cafe::Io::BinaryReader& reader, RequestContext& context,
		                     std::size_t seq) const
		{
			return static_cast<const T*>(this)->DoProcessResponse(reader, context, seq);
		}
	};

	class RequestBuilder
	{
	public:
		static constexpr std::size_t RequestHeadSize = 27;

		explicit RequestBuilder(RequestContext context) : m_Context{ std::move(context) }
		{
		}

		/// @return Seq
		template <typename T, std::uint16_t CmdValue, std::uint16_t SubCmdValue,
		          EncryptType EncryptTypeValue>
		std::size_t
		WriteRequest(Cafe::Io::OutputStream* stream,
		             RequestBase<T, CmdValue, SubCmdValue, EncryptTypeValue> const& request) const
		{
			const auto seq = m_Context.AcquireRequestSeq();

			Cafe::Io::MemoryStream unencryptedBodyStream;
			Cafe::Io::BinaryWriter writer{ &unencryptedBodyStream, std::endian::big };

			writer.Write(SubCmdValue);

			const auto tlvNumPos = unencryptedBodyStream.GetPosition();
			writer.Write(std::uint16_t{});

			const auto tlvNum = [&] {
				Tlv::TlvBuilder tlvBuilder{ &unencryptedBodyStream };
				request.Write(tlvBuilder, m_Context, seq);
				return tlvBuilder.GetTlvCount();
			}();

			unencryptedBodyStream.SeekFromBegin(tlvNumPos);
			writer.Write(tlvNum);

			Cafe::Io::MemoryStream requestContentStream;
			Cafe::Io::BinaryWriter requestWriter{ &requestContentStream, std::endian::big };

			// 写入 Head
			const auto clientSeq = m_Context.AcquireClientSeq();
			requestWriter.Write(std::uint8_t{ 2 });
			const auto totalSizePos = requestContentStream.GetPosition();
			requestWriter.Write(std::uint16_t{});
			requestWriter.Write(DefaultClientVersion);
			requestWriter.Write(CmdValue);
			requestWriter.Write(seq);
			requestWriter.Write(m_Context.Uin);
			requestWriter.Write(std::uint8_t{ 3 });
			requestWriter.Write(std::uint8_t{ 7 });
			requestWriter.Write(std::uint8_t{});     // retry
			requestWriter.Write(std::uint32_t{ 2 }); // ext type
			requestWriter.Write(std::uint32_t{});    // app client type
			requestWriter.Write(std::uint32_t{});    // ext instance

			// 写入加密的 Body
			const auto bodyBeginPos = requestContentStream.GetPosition();
			EncryptBody<EncryptTypeValue>(&requestContentStream,
			                              unencryptedBodyStream.GetInternalStorage());
			const auto bodyEndPos = requestContentStream.GetPosition();
			const auto bodySize = bodyEndPos - bodyBeginPos;

			requestContentStream.SeekFromBegin(totalSizePos);
			requestWriter.Write(static_cast<std::uint16_t>(RequestHeadSize + 2 + bodySize));

			requestContentStream.SeekFromBegin(bodyEndPos);

			// 写入 End
			requestWriter.Write(std::uint8_t{ 3 });

			EncodeRequest(stream, requestContentStream.GetInternalStorage());

			return seq;
		}

		template <EncryptType EncryptTypeValue>
		void EncryptBody(Cafe::Io::OutputStream* stream, gsl::span<const std::byte> const& body)
		{
			Cafe::Io::BinaryWriter writer{ stream, std::endian::big };

			const auto teaKey = [&] {
				if constexpr (EncryptTypeValue == EncryptType::Ecdh)
				{
					writer.Write(std::uint16_t{ 0x0101 });
					stream->WriteBytes(gsl::make_span(m_Context.Keys.RandomKey));
					writer.Write(std::uint16_t{ 0x0102 });
					writer.Write(static_cast<std::uint16_t>(sizeof m_Context.Keys.PubKey));
					stream->WriteBytes(gsl::make_span(m_Context.Keys.PubKey));

					return Cryptography::Tea::FormatKey(m_Context.Keys.ShareKey);
				}
				else
				{
					writer.Write(std::uint16_t{ 0x0102 });
					stream->WriteBytes(gsl::make_span(m_Context.Keys.RandomKey));
					writer.Write(std::uint16_t{ 0x0102 });
					writer.Write(std::uint16_t{});

					return Cryptography::Tea::FormatKey(m_Context.Keys.RandomKey);
				}
			}();
			Cryptography::Tea::Encrypt(body, stream, teaKey);
		}

		void EncodeRequest(Cafe::Io::OutputStream* stream, gsl::span<const std::byte> const& request)
		{
			// TODO
		}

	private:
		RequestContext m_Context;
	};

	struct RequestTGTGT : RequestBase<RequestTGTGT, 2064, 9, EncryptType::Ecdh>
	{
		void DoWrite(Tlv::TlvBuilder& tlvBuilder, RequestContext const& context, std::size_t seq) const
		{
			const auto& guid = context.GetGuid();

			tlvBuilder.WriteTlv(Tlv::TlvT<0x106>{ AppId, SubAppId, ClientVersion, Uin, InitTime, ClientIp,
			                                      false, PasswordMd5, 0, TGTGTKey, false, guid, 1 });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x100>{ AppId, SubAppId, WxAppId, GetSig1 });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x107>{ PicType, CapType, PicSize, RetType });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x116>{ Bitmap, GetSig, SubAppIdList });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x145>{ guid });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x154>{ seq });
			tlvBuilder.WriteTlv(
			    Tlv::TlvT<0x141>{ context.SimOperatorName, context.ConnectionType, context.Apn });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x8>{ 0, context.CurrentLocaleId, 0 });
			tlvBuilder.WriteTlv(
			    Tlv::TlvT<0x147>{ AppId, DefaultApkVersion, gsl::as_bytes(gsl::make_span(Signature)) });
			tlvBuilder.WriteTlv(Tlv::TlvT<0x177>{ BuildTime, SdkVersion });

			if (!Ksid.empty())
			{
				tlvBuilder.WriteTlv(Tlv::TlvT<0x108>{ std::vector<std::byte>(Ksid.begin(), Ksid.end()) });
			}

			if (!context.WifiMac.IsEmpty())
			{
				tlvBuilder.WriteTlv(Tlv::TlvT<0x187>{ context.WifiMac });
			}

			if (!context.AndroidId.IsEmpty())
			{
				tlvBuilder.WriteTlv(Tlv::TlvT<0x188>{ context.AndroidId });
			}

			if (!context.Imei.IsEmpty())
			{
				tlvBuilder.WriteTlv(Tlv::TlvT<0x109>{ context.Imei });
			}

			// TODO
		}

		std::uint32_t AppId;
		std::uint32_t SubAppId;
		std::uint32_t ClientVersion;
		std::uint32_t Uin;
		std::uint32_t Rc;
		IpV4Addr ClientIp;
		gsl::span<const std::byte> InitTime;
		gsl::span<const std::byte, 16> PasswordMd5;
		gsl::span<const std::byte> TGTGTKey;
		std::uint32_t SigSrc;
		std::uint32_t Bitmap;
		std::uint32_t GetSig;
		gsl::span<const std::uint32_t> SubAppIdList;
		std::uint32_t GetSig1;
		std::uint32_t WxAppId;
		std::uint16_t PicType;
		std::uint8_t CapType;
		std::uint16_t PicSize;
		std::uint8_t RetType;
		gsl::span<const std::byte> Ksid;
		gsl::span<const std::byte> SigSession;
		UsingStringView ApkId;
		gsl::span<const UsingStringView> Domains;
	};
} // namespace YumeBot::Request
