#pragma once

#include "Request.h"

namespace YumeBot
{
	/// @brief  抽象 Socket
	/// @note   实现该接口以使用不同后端的 Socket 分发信息
	/// @remark Socket 要求使用 TCP
	template <typename TSocketImpl>
	struct AbstractSocket
	{
		void ConnectTo(IpV4Addr const& ip, std::uint16_t port)
		{
			static_cast<TSocketImpl*>(this)->DoConnectTo(ip, port);
		}

		/// @remark 同一次通讯中 PullData 可能调用多次
		void PullData(gsl::span<std::byte> const& buffer)
		{
			static_cast<TSocketImpl*>(this)->DoPullData(buffer);
		}

		void PushData(gsl::span<const std::byte> const& buffer)
		{
			static_cast<TSocketImpl*>(this)->DoPushData(buffer);
		}

		void CloseConnection()
		{
			static_cast<TSocketImpl*>(this)->DoCloseConnection();
		}

	protected:
		~AbstractSocket() = default;
	};

	enum class LoginStatusEnum
	{
		Success,
		RequestVerify,
		Failed
	};

	template <LoginStatusEnum LoginStatusValue>
	struct LoginResult
	{
		static constexpr LoginStatusEnum LoginStatus = LoginStatusValue;
	};

	template <>
	struct LoginResult<LoginStatusEnum::RequestVerify>
	{
		static constexpr LoginStatusEnum LoginStatus = LoginStatusEnum::RequestVerify;

		const gsl::span<const std::byte> VerifyCodePicture;
		const std::size_t Width, Height;
	};

	class SessionFactory
	{
		struct UserPassword
		{
			UserPassword(UsingStringView const& password) noexcept;
			UserPassword(gsl::span<const std::byte, 16> const& passwordMd5) noexcept;

			std::array<std::byte, 16> PasswordMd5;
		};

		template <typename TSocketImpl>
		class Session
		{
		public:
			Session(AbstractSocket<TSocketImpl>& socket, Request::RequestContext&& initialRequestContext)
			    : m_Socket{ socket }, m_RequestBuilder{ std::move(initialRequestContext) }
			{
			}

			Session(Session const&) = delete;
			Session& operator=(Session const&) = delete;

			template <typename LoginCallback>
			void Login(LoginCallback&& callback)
			{
				Cafe::Io::MemoryStream stream;
				// TODO
				m_RequestBuilder.WriteRequest(stream, Request::RequestTGTGT{});
				m_Socket.PushData(stream.GetInternalStorage());

				// TODO: 验证 Response
				throw std::runtime_error("Not implemented");
			}

		private:
			AbstractSocket<TSocketImpl>& m_Socket;
			Request::RequestBuilder m_RequestBuilder;
		};

	public:
		/// @see    SessionFactory::GetContext()
		SessionFactory(Request::RequestContext commonInitialContext);

		SessionFactory(SessionFactory const&) = delete;
		SessionFactory& operator=(SessionFactory const&) = delete;

		/// @brief  获取共同初始化上下文
		/// @note   不需要存储 Uin 及密码 MD5，会在创建会话时覆盖
		Request::RequestContext& GetContext();

		/// @brief  以 Uin、密码及用户指定的 Socket 构建会话
		/// @remark 会话不具有 Socket 的所有权，用户需自行保证在会话有效期间有效
		template <typename TSocketImpl>
		std::unique_ptr<Session<TSocketImpl>> CreateSession(std::uint32_t uin, UserPassword password,
		                                                    AbstractSocket<TSocketImpl>& socket)
		{
			Request::RequestContext context = m_CommonInitialContext;
			context.Uin = uin;
			context.PasswordMd5 = password.PasswordMd5;
			return std::make_unique<Session>(socket, std::move(context));
		}

	private:
		Request::RequestContext m_CommonInitialContext;
	};
} // namespace YumeBot
