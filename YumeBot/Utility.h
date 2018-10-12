#pragma once
#include <cstddef>
#include <gsl/span>
#include <type_traits>

namespace YumeBot::Utility
{
	constexpr std::size_t AlignTo(std::size_t num, std::size_t alignment) noexcept
	{
		return num + alignment - 1 & ~(alignment - 1);
	}

	template <typename T, typename U, bool = std::is_const_v<T>>
	struct MayAddConst
	{
		using Type = std::add_const_t<U>;
	};

	template <typename T, typename U>
	struct MayAddConst<T, U, false>
	{
		using Type = U;
	};

	template <typename T, std::size_t N>
	constexpr typename MayAddConst<T, std::byte>::Type(&ToByteArray(T(&arr)[N]) noexcept)[sizeof(T) * N / sizeof(std::byte)]
	{
		return reinterpret_cast<typename MayAddConst<T, std::byte>::Type(&)[sizeof(T) * N / sizeof(std::byte)]>(arr);
	}

	template <typename T, std::size_t N>
	constexpr gsl::span<typename MayAddConst<T, std::byte>::Type, sizeof(T) * N / sizeof(std::byte)> ToByteSpan(T(&arr)[N]) noexcept
	{
		return ToByteArray(arr);
	}

	template <typename T>
	struct ResultType
	{
		using Type = T;
	};

	template <typename TypeSequence, template <typename> class Predicate, typename FallbackType>
	struct GetFirstOr
		: ResultType<FallbackType>
	{
	};

	template <template <typename...> class TypeSequenceTemplate, typename FirstArg, typename... RestArgs, template <typename> class Predicate, typename FallbackType>
	struct GetFirstOr<TypeSequenceTemplate<FirstArg, RestArgs...>, Predicate, FallbackType>
		: std::conditional_t<Predicate<FirstArg>::value, ResultType<FirstArg>, GetFirstOr<TypeSequenceTemplate<RestArgs...>, Predicate, FallbackType>>
	{
	};

	template <typename T>
	constexpr bool IsTemplate() noexcept
	{
		return false;
	}

	template <template <typename...> class Template>
	constexpr bool IsTemplate() noexcept
	{
		return true;
	}

	template <typename From, template <typename...> class ToTemplate>
	struct ApplyToTrait
	{
		using Type = ToTemplate<From>;
	};

	template <template <typename...> class FromTemplate, typename... Args, template <typename...> class ToTemplate>
	struct ApplyToTrait<FromTemplate<Args...>, ToTemplate>
	{
		using Type = ToTemplate<Args...>;
	};

	template <typename From, template <typename...> class ToTemplate>
	using ApplyTo = typename ApplyToTrait<From, ToTemplate>::Type;

	template <typename T, template <typename...> class Template>
	struct IsTemplateOf
		: std::false_type
	{
	};

	template <template <typename...> class Template, typename... Args>
	struct IsTemplateOf<Template<Args...>, Template>
		: std::true_type
	{
	};

	template <typename T>
	constexpr auto RecursiveApply()
	{
		return ResultType<T>{};
	}

	template <typename T, typename FirstOperation, typename... RestOperations>
	constexpr auto RecursiveApply()
	{
		return RecursiveApply<typename FirstOperation::template Apply<T>::Type, RestOperations...>();
	}

	template <template <typename...> class TemplateArg>
	struct TemplatePlaceholder
	{
		template <typename... Args>
		using Template = TemplateArg<Args...>;
	};

	template <template <typename...> class Template>
	constexpr auto RecursiveApply()
	{
		return TemplatePlaceholder<Template>{};
	}

	template <template <typename...> class Template, typename FirstOperation, typename... RestOperations>
	constexpr auto RecursiveApply()
	{
		return RecursiveApply<typename FirstOperation::template Apply<Template>::Type, RestOperations...>();
	}

	template <template <typename> class Predicate, typename FallbackType>
	constexpr decltype(auto) ReturnFirst()
	{
		return FallbackType{};
	}

	template <template <typename> class Predicate, typename FallbackType, typename FirstArg, typename... Args>
	constexpr decltype(auto) ReturnFirst(FirstArg&& firstArg, Args&&... args)
	{
		if constexpr (Predicate<FirstArg&&>::value)
		{
			return std::forward<FirstArg>(firstArg);
		}
		else
		{
			return ReturnFirst<Predicate, FallbackType>(std::forward<Args>(args)...);
		}
	}

	template <template <typename...> class Template, typename... Args>
	struct BindTrait
	{
		template <typename... RestArgs>
		using Result = Template<Args..., RestArgs...>;
	};

	template <template <typename...> class Template1, template <typename...> class Template2>
	struct ConcatTrait
	{
		template <typename... Args>
		using Result = Template2<Template1<Args...>>;
	};

	template <typename T>
	using RemoveCvRef = std::remove_cv_t<std::remove_reference_t<T>>;
}
