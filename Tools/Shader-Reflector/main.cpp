#include <filesystem>

#include <dxc/dxcapi.h>

#include "spirv_reflect.h"

#include "ShaderCompiler.hpp"

namespace
{
    std::string reflected_descriptor_type_to_vulkan_handle(const SpvReflectDescriptorType reflected_type)
    {
        switch(reflected_type)
        {
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                return "Sampler*";
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return "ImageView*";
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return "BufferView*";
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return "AccelerationStructure*";

            default:
                return "INVALID_TYPE";
        }
    }

    bool is_image(const SpvReflectDescriptorType reflected_type)
    {
        switch(reflected_type)
        {
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return true;

            default:
                return false;
        }
    }

    bool is_buffer(const SpvReflectDescriptorType reflected_type)
    {
        switch(reflected_type)
        {
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return true;

            default:
                return false;
        }
    }
}

void process_shader(const std::filesystem::path& base_output_dir, const std::filesystem::path& base_input_dir, const std::filesystem::path& shader_file)
{
    std::unique_ptr<Core::ShaderCompiler> shader_compiler = std::make_unique<Core::ShaderCompiler>();

    const wchar_t* args[] = {L"-spirv", L"-fspv-target-env=vulkan1.2", L"-O3"};
    IDxcBlob* shader_binary = shader_compiler->compileShader(shader_file, {}, args, 3);

    if(!shader_binary) // TODO for now just skip this shader, the compiler will already log out the specifics of the compilation failure.
        return;

    const unsigned char* spirvBuffer = static_cast<const unsigned char*>(shader_binary->GetBufferPointer());
    std::vector<char> SPIRV;
    SPIRV.resize(shader_binary->GetBufferSize());
    memcpy(SPIRV.data(), spirvBuffer, SPIRV.size());

    shader_binary->Release();

    {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(SPIRV.size(), SPIRV.data(), &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::string struct_definition = "struct " + shader_file.stem().string() + "_parameters\n"
                                        "{\n";


        const std::string required_descriptor_sets_function =   "uint32_t get_required_descriptor_sets() const\n"
                                                                "{\n"
                                                                "   return " + std::to_string(module.descriptor_set_count) + ";\n"
                                                                "}\n";

        std::string descriptor_set_layouts = "std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layouts() const\n"
                                             "{\n"
                                             "  std::vector<VkDescriptorSetLayoutBinding> bindings;\n";

        std::string descriptor_writes = "std::vector<VkWriteDescriptorSet> get_descriptor_writes(std::vector<VkDescriptorSet>& descriptor_sets)\n"
                                        "{\n"
                                        "   std::vector<VkWriteDescriptorSet> descriptor_writes;\n"
                                        "   image_infos.clear();\n"
                                        "   image_infos.reserve(15);\n"
                                        "   buffer_infos.clear();\n"
                                        "   buffer_infos.reserve(15);\n";
        std::string paramaters;
        for(uint32_t descriptor_set_i = 0; descriptor_set_i < module.descriptor_set_count; descriptor_set_i++)
        {
            auto descriptor_set_info = module.descriptor_sets[descriptor_set_i];

            for(uint32_t descriptor_i = 0; descriptor_i < descriptor_set_info.binding_count; descriptor_i++)
            {
                SpvReflectDescriptorBinding* descriptor_info = descriptor_set_info.bindings[descriptor_i];

                paramaters += " " + reflected_descriptor_type_to_vulkan_handle(descriptor_info->descriptor_type) + " " + descriptor_info->name + ";\n";

                descriptor_set_layouts += " {\n"
                                          "      VkDescriptorSetLayoutbinding binding{};\n"
                                          "      binding.binding = " + std::to_string(descriptor_info->binding) + ";\n" +
                                          "      binding.descriptorCount = " + std::to_string(descriptor_info->count) + ";\n" +
                                          "      binding.descriptorType = " + std::to_string(descriptor_info->descriptor_type) + ";\n" +
                                          "      binding.stageFlags = " + std::to_string(module.shader_stage) + ";\n" +
                                          "      bindings.push_back(binding);\n"
                                          " }\n";

                descriptor_writes += "  {\n"
                                     "      VkWriteDescriptorSet descriptor_write{};\n"
                                     "      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;\n"
                                     "      descriptor_write.pNext = nullptr;\n"
                                     "      descriptor_write.dstSet = descriptor_sets[" + std::to_string(descriptor_set_i) + "]\n" +
                                     "      descriptor_write.dstBinding = " + std::to_string(descriptor_info->binding) + ";\n" +
                                     "      descriptor_write.descriptorCount = " + std::to_string(descriptor_info->count) + ";\n" +
                                     "      descriptor_write.descriptorType = " + std::to_string(descriptor_info->descriptor_type) + ";\n";

                                    if(is_image(descriptor_info->descriptor_type))
                                    {
                                        descriptor_writes += "      VkDescriptorBufferInfo image_info{};\n"
                                                             "      image_info.buffer = " + std::string(descriptor_info->name) + "->get_handle();\n"
                                                             "      image_info.layout = ";
                                                            if(descriptor_info->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                                                            {
                                                                descriptor_writes += std::string(descriptor_info->name) + "->is_depth() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;\n";
                                                            }
                                                            else
                                                            {
                                                                descriptor_writes += "VK_IMAGE_LAYOUT_GENERAL;\n";
                                                            }

                                        descriptor_writes += ""
                                                             "      VkDescriptorImageInfo* sampler_info_ptr = &image_infos.back();\n"
                                                             "      image_infos.push_back(sampler_info);\n"
                                                             "      descriptor_write.pImageInfo = sampler_info_ptr;\n";
                                    }
                                    else if(is_buffer(descriptor_info->descriptor_type))
                                    {
                                        descriptor_writes += "      VkDescriptorBufferInfo buffer_info{};\n"
                                                             "      buffer_info.buffer = " + std::string(descriptor_info->name) + "->get_handle();\n"
                                                             "      buffer_info.offset = " + std::string(descriptor_info->name) + "->get_offset();\n"
                                                             "      buffer_info.size = " + std::string(descriptor_info->name) + "->get_size();\n"
                                                             "\n"
                                                             "      VkDescriptorBufferInfo* buffer_info_ptr = &buffer_infos.back();\n"
                                                             "      buffer_infos.push_back(buffer_info);\n"
                                                             "      descriptor_write.pBufferInfo = buffer_info_ptr;\n";
                                    }
                                    else // It's a sampler
                                    {
                                        descriptor_writes += "      VkDescriptorImageInfo sampler_info{};\n"
                                                             "      sampler_info.sampler = " + std::string(descriptor_info->name) + "->get_handle();\n"
                                                             "\n"
                                                             "      VkDescriptorImageInfo* sampler_info_ptr = &image_infos.back();\n"
                                                             "      image_infos.push_back(sampler_info);\n"
                                                             "      descriptor_write.pImageInfo = sampler_info_ptr;\n";
                                    }

                descriptor_writes += "      descriptor_writes.push_back(descriptor_write);\n"
                                     "  }\n";
            }
        }

        spvReflectDestroyShaderModule(&module);

        paramaters += " private:\n"
                      " std::vector<VkDescriptorImageInfo> image_infos;\n"
                      " std::vector<VkDescriptorBufferInfo> buffer_infos;\n"
                      " public:\n";

        descriptor_set_layouts += "     return bindings;\n"
                                  " }\n";

        descriptor_writes += "      return descriptor_writes;\n"
                             "  }\n";

        struct_definition += paramaters;
        struct_definition += required_descriptor_sets_function;
        struct_definition += descriptor_writes;
        struct_definition += descriptor_set_layouts;
        struct_definition += "};\n";

        // Write out the spir-v binary and the generated reflected shader header.
        std::filesystem::path relative_shader_path = std::filesystem::relative(shader_file, base_input_dir);
        std::filesystem::path output_shader_file = base_output_dir / relative_shader_path;
        {
            output_shader_file.replace_extension(".spirv");
            if(!std::filesystem::exists(output_shader_file.parent_path()))
            {
                std::filesystem::create_directories(output_shader_file.parent_path());
            }
            FILE* spirv_file = fopen(output_shader_file.string().c_str(), "wb");
            fwrite(SPIRV.data(), 1, SPIRV.size(), spirv_file);
            fclose(spirv_file);
        }

        std::string parameters_file_content = "#ifndef " + shader_file.stem().string() + "_HPP\n"
                                      "#define " + shader_file.stem().string() + "_HPP\n"
                                      "\n"
                                      "#include <vulkan/vulkan.h>\n";

        parameters_file_content += struct_definition;
        parameters_file_content += "#endif\n";

        {
            std::filesystem::path parameter_file_path = output_shader_file.replace_extension(".hpp");
            FILE* parameter_file = fopen(parameter_file_path.string().c_str(), "wb");
            fwrite(parameters_file_content.data(), 1, parameters_file_content.size(), parameter_file);
            fclose(parameter_file);
        }

    }
}

int main(int argv, const char** argc)
{

    if(argv != 3)
    {
        printf("Shader-Reflector requires Input and output directory\n");
        return 255;
    }

    const std::filesystem::path input_directory = argc[1];
    const std::filesystem::path output_directory = argc[2];

    for(auto& child : std::filesystem::recursive_directory_iterator(input_directory))
    {
        const std::string file_extension = child.path().extension();
        if(child.is_regular_file() && (file_extension == ".frag" || file_extension == ".vert" || file_extension == ".comp"))
        {
            process_shader(output_directory, input_directory, child.path());
        }
    }
}
