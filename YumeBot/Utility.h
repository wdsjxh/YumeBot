#pragma once
#include <chrono>
#include <cstddef>
#include <gsl/span>
#include <type_traits>

namespace YumeBot::Utility
{
	constexpr std::size_t AlignTo(std::size_t num, std::size_t alignment) noexcept
	{
		return num + alignment - 1 & ~(alignment - 1);
	}

	template <typename T>
	struct ResultType
	{
		using Type = T;
	};

	template <typename TypeSequence, template <typename> class Predicate, typename FallbackType>
	struct GetFirstOr : ResultType<FallbackType>
	{
	};

	template <template <typename...> class TypeSequenceTemplate, typename FirstArg,
	          typename... RestArgs, template <typename> class Predicate, typename FallbackType>
	struct GetFirstOr<TypeSequenceTemplate<FirstArg, RestArgs...>, Predicate, FallbackType>
	    : std::conditional_t<Predicate<FirstArg>::value, ResultType<FirstArg>,
	                         GetFirstOr<TypeSequenceTemplate<RestArgs...>, Predicate, FallbackType>>
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

	template <template <typename...> class FromTemplate, typename... Args,
	          template <typename...> class ToTemplate>
	struct ApplyToTrait<FromTemplate<Args...>, ToTemplate>
	{
		using Type = ToTemplate<Args...>;
	};

	template <typename From, template <typename...> class ToTemplate>
	using ApplyTo = typename ApplyToTrait<From, ToTemplate>::Type;

	template <typename T, template <typename...> class Template>
	struct IsTemplateOf : std::false_type
	{
	};

	template <template <typename...> class Template, typename... Args>
	struct IsTemplateOf<Template<Args...>, Template> : std::true_type
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

	template <template <typename...> class Template, typename FirstOperation,
	          typename... RestOperations>
	constexpr auto RecursiveApply()
	{
		return RecursiveApply<typename FirstOperation::template Apply<Template>::Type,
		                      RestOperations...>();
	}

	template <template <typename> class Predicate, typename FallbackType>
	constexpr decltype(auto) ReturnFirst()
	{
		return FallbackType{};
	}

	template <template <typename> class Predicate, typename FallbackType, typename FirstArg,
	          typename... Args>
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

	template <template <typename...> class Trait, typename... T>
	using GetType = typename Trait<T...>::type;

	// Workaround: 临时糊掉 C2210：pack expansions cannot be used as arguments to non-packed
	// parameters in alias templates
	template <typename... T>
	using RemoveCvRef = std::remove_cv_t<GetType<std::remove_reference, T...>>;

	template <typename T, template <typename> class Template>
	struct MayRemoveTemplate : ResultType<T>
	{
	};

	template <typename T, template <typename> class Template>
	struct MayRemoveTemplate<Template<T>, Template> : ResultType<T>
	{
	};

	template <typename T, typename U>
	constexpr std::enable_if_t<
	    std::is_integral_v<RemoveCvRef<T>> && std::is_integral_v<RemoveCvRef<U>>, bool>
	InRangeOf(U&& value)
	{
		using TType = RemoveCvRef<T>;
		using UType = RemoveCvRef<U>;

		// 因为有 char 所以必须 signed 和 unsigned 都测试
		if constexpr (sizeof(TType) >= sizeof(UType) &&
		              std::is_signed_v<TType> == std::is_signed_v<UType> &&
		              std::is_unsigned_v<TType> == std::is_unsigned_v<UType>)
		{
			return true;
		}
		else
		{
			return value >= static_cast<UType>(std::numeric_limits<TType>::min()) &&
			       value <= static_cast<UType>(std::numeric_limits<TType>::max());
		}
	}

	template <template <typename> class Trait>
	constexpr auto Filter() noexcept
	{
		return std::tuple();
	}

	template <template <typename> class Trait, typename Arg, typename... Args>
	constexpr auto Filter(Arg&& arg, Args&&... args) noexcept
	{
		if constexpr (Trait<Arg>::value)
		{
			return std::tuple_cat(std::forward_as_tuple<Arg>(arg),
			                      Filter<Trait>(std::forward<Args>(args)...));
		}
		else
		{
			return Filter<Trait>(std::forward<Args>(args)...);
		}
	}

	namespace Detail
	{
		template <typename T, typename Tuple, std::size_t... Indexes>
		constexpr void
		InitializeWithTuple(T& obj, Tuple&& args, std::index_sequence<Indexes...>) noexcept(
		    std::is_nothrow_constructible_v<
		        T, std::tuple_element_t<Indexes, std::remove_reference_t<Tuple>>...>)
		{
			new (static_cast<void*>(std::addressof(obj)))
			    T(std::get<Indexes>(std::forward<Tuple>(args))...);
		}
	} // namespace Detail

	template <typename T, typename Tuple>
	constexpr void
	InitializeWithTuple(T& obj, Tuple&& args) noexcept(noexcept(Detail::InitializeWithTuple(
	    obj, std::forward<Tuple>(args),
	    std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>())))
	{
		Detail::InitializeWithTuple(
		    obj, std::forward<Tuple>(args),
		    std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>());
	}

	inline std::uint32_t GetPosixTime() noexcept
	{
		const auto count = std::chrono::duration_cast<std::chrono::seconds>(
		                       std::chrono::system_clock::now().time_since_epoch())
		                       .count();
		assert(count <= std::numeric_limits<std::uint32_t>::max());
		return static_cast<std::uint32_t>(count);
	}
} // namespace YumeBot::Utility
