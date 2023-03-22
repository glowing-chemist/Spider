#ifndef SHADER_COMPILER_HPP
#define SHADER_COMPILER_HPP

#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>

class IDxcLibrary;
class IDxcCompiler;
class IDxcBlob;

namespace Core
{

    class ShaderDefine
    {
    public:
        ShaderDefine(const std::wstring& name, const uint64_t value) :
            mName(name),
            mValue(std::to_wstring(value)) {}

        const std::wstring& getName() const
        {
            return mName;
        }

        const std::wstring& getValue() const
        {
            return mValue;
        }

        private:

        std::wstring mName;
        std::wstring mValue;
    };

    class ShaderCompiler
    {
    public:
        ShaderCompiler();
        ~ShaderCompiler();


        IDxcBlob* compileShader(const std::filesystem::path& path,
                                const std::vector<ShaderDefine>& defines,
                                const wchar_t** args,
                                const uint32_t argCount);

    private:

        IDxcLibrary* mLibrary;
        IDxcCompiler* mCompiler;
    };

}

#endif
