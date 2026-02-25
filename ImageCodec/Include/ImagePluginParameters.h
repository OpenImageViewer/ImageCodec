#pragma once

#include "ImageCommon.h"

#include <any>
#include <map>
#include <utility>
#include <variant>

namespace IMCodec
{
    using SharedParameterValue = std::variant<int, double, string_type>;

    enum class ParametrType
    {
        None,
        DesiredSize
    };

    class Parameters
    {
    public:
        using SharedParametersMap = std::map<ParametrType, SharedParameterValue>;
        using CustomParametersMap = std::map<string_type, std::any>;

        void SetShared(ParametrType type, SharedParameterValue value)
        {
            sharedParameters_.insert_or_assign(type, std::move(value));
        }

        const SharedParameterValue* GetShared(ParametrType type) const
        {
            auto it = sharedParameters_.find(type);
            return it != sharedParameters_.end() ? &it->second : nullptr;
        }

        bool HasShared(ParametrType type) const
        {
            return sharedParameters_.find(type) != sharedParameters_.end();
        }

        template <class T>
        const T* GetSharedAs(ParametrType type) const
        {
            auto it = sharedParameters_.find(type);
            if (it != sharedParameters_.end())
                return std::get_if<T>(&it->second);
            return nullptr;
        }

        void SetCustom(string_type key, std::any value)
        {
            customParameters_.insert_or_assign(std::move(key), std::move(value));
        }

        const std::any* GetCustom(const string_type& key) const
        {
            auto it = customParameters_.find(key);
            return it != customParameters_.end() ? &it->second : nullptr;
        }

        bool HasCustom(const string_type& key) const
        {
            return customParameters_.find(key) != customParameters_.end();
        }

        template <class T>
        const T* GetCustomAs(const string_type& key) const
        {
            auto it = customParameters_.find(key);
            if (it != customParameters_.end())
                return std::any_cast<T>(&it->second);
            return nullptr;
        }

    private:
        SharedParametersMap sharedParameters_;
        CustomParametersMap customParameters_;
    };
}
