#include "ShaderCompiler.hpp"


#include <dxc/dxcapi.h>


namespace
{
    class shaderIncludeHandler : public IDxcIncludeHandler
    {
    public:

        shaderIncludeHandler(IDxcLibrary* library)  :
                mLibrary(library) {}

        virtual HRESULT LoadSource(
                _In_ const wchar_t* pFilename,                             // Candidate filename.
                _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource  // Resultant source object for included file, nullptr if not found.
        ) override final
        {
            uint32_t codePage = CP_UTF8;
            IDxcBlobEncoding* sourceBlob;
            HRESULT hr = mLibrary->CreateBlobFromFile(pFilename, &codePage, &sourceBlob);

            *ppIncludeSource = sourceBlob;

            return hr;
        }

        //Stub these.
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(
                /* [in] */ REFIID,
                /* [iid_is][out] */ _COM_Outptr_ void**) override final
        {
            return S_OK;
        }

        virtual ULONG STDMETHODCALLTYPE AddRef(void) override final
        {
            return 0ULL;
        }

        virtual ULONG STDMETHODCALLTYPE Release(void) override final
        {
            return 0ULL;
        }

    private:

        IDxcLibrary* mLibrary;
    };
}

namespace Core
{

    ShaderCompiler::ShaderCompiler()
    {
        HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&mLibrary));

        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
    }

    ShaderCompiler::~ShaderCompiler()
    {
        mLibrary->Release();
        mCompiler->Release();
    }


    IDxcBlob* ShaderCompiler::compileShader(const std::filesystem::path& path,
                                            const std::vector<ShaderDefine>& prefix,
                                            const wchar_t** args,
                                            const uint32_t argCount)
    {
        uint32_t codePage = CP_UTF8;
        IDxcBlobEncoding* sourceBlob;
        std::wstring wfilePath = path.wstring();
        HRESULT hr = mLibrary->CreateBlobFromFile(wfilePath.data(), &codePage, &sourceBlob);

        std::vector<DxcDefine> defines{};
        defines.reserve(prefix.size());
        for(auto& define : prefix)
        {
            defines.push_back(DxcDefine{define.getName().data(), define.getValue().data()});
        }

        const wchar_t* shader_profile = L"ps_6_0";
        if (path.extension() == ".vert")
            shader_profile = L"vs_6_0";
        else if (path.extension() == ".frag")
            shader_profile = L"ps_6_0";
        else if (path.extension() == ".comp")
            shader_profile = L"cs_6_0";

        shaderIncludeHandler includer(mLibrary);
        IDxcOperationResult* result;
        hr = mCompiler->Compile(
                sourceBlob,
                wfilePath.data(),
                L"main",
                shader_profile,
                &args[0], argCount,
                defines.data(), defines.size(),
                &includer,
                &result);
        if (SUCCEEDED(hr))
            result->GetStatus(&hr);
        if (FAILED(hr))
        {
            if (result)
            {
                IDxcBlobEncoding* errorsBlob;
                hr = result->GetErrorBuffer(&errorsBlob);
                if (SUCCEEDED(hr) && errorsBlob)
                {
                    printf("Compilation failed with errors:\n%s\n",
                                  (const char*)errorsBlob->GetBufferPointer());

                    fflush(stdout);
                }
            }

            sourceBlob->Release();
            result->Release();

            return nullptr;
        }

        IDxcBlob* binaryBlob;
        hr = result->GetResult(&binaryBlob);

        sourceBlob->Release();
        result->Release();

        return binaryBlob;
    }

}
